/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup qfea
 *
 * QFEA FDTD electromagnetic solver implementation.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_qfea_types.h"

#include "QFEA_physics.h"
#include "QFEA_tensor.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Physical constants */
#define SPEED_OF_LIGHT 299792458.0f        /* m/s */
#define VACUUM_PERMITTIVITY 8.854187817e-12f /* F/m */
#define VACUUM_PERMEABILITY 1.256637062e-6f  /* H/m */

/* -------------------------------------------------------------------- */
/** \name FDTD Time Step Calculation
 * \{ */

float QFEA_fdtd_compute_cfl_timestep(const QFEAVoxelGrid *voxel_grid,
                                      const QFEAMaterial **materials)
{
  if (!voxel_grid) {
    return 0.0f;
  }

  const float dx = voxel_grid->resolution;
  const float dy = voxel_grid->resolution;
  const float dz = voxel_grid->resolution;

  /* Find maximum wave speed in materials */
  float max_wave_speed = SPEED_OF_LIGHT;

  if (materials) {
    const size_t total_voxels = (size_t)voxel_grid->dims[0] * voxel_grid->dims[1] *
                                 voxel_grid->dims[2];

    for (size_t i = 0; i < total_voxels; i++) {
      const QFEAMaterial *mat = materials[i];
      if (mat) {
        /* Wave speed: c/sqrt(εᵣ·μᵣ) */
        const float v = SPEED_OF_LIGHT / sqrtf(mat->epsilon_r * mat->mu_r);
        max_wave_speed = fmaxf(max_wave_speed, v);
      }
    }
  }

  /* CFL condition for 3D: Δt ≤ 1/(v·√(1/Δx² + 1/Δy² + 1/Δz²)) */
  const float cfl_factor = 1.0f / sqrtf(1.0f / (dx * dx) + 1.0f / (dy * dy) + 1.0f / (dz * dz));

  /* Use safety factor of 0.95 */
  const float dt = 0.95f * cfl_factor / max_wave_speed;

  return dt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FDTD Update Equations
 * \{ */

/**
 * Update magnetic field (H-field) using Faraday's law:
 * ∂H/∂t = -(1/μ) ∇×E
 *
 * Yee grid staggered updates:
 * Hx(i+1/2, j, k) = Hx - (Δt/μ)[(Ez(j+1) - Ez(j))/Δy - (Ey(k+1) - Ey(k))/Δz]
 * Hy(i, j+1/2, k) = Hy - (Δt/μ)[(Ex(k+1) - Ex(k))/Δz - (Ez(i+1) - Ez(i))/Δx]
 * Hz(i, j, k+1/2) = Hz - (Δt/μ)[(Ey(i+1) - Ey(i))/Δx - (Ex(j+1) - Ex(j))/Δy]
 */
static void fdtd_update_h_field(QFEATensorField *tensor,
                                 int ex_ch,
                                 int ey_ch,
                                 int ez_ch,
                                 int hx_ch,
                                 int hy_ch,
                                 int hz_ch,
                                 const float *mu,
                                 float dt)
{
  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float dx = grid->resolution;
  const float dy = grid->resolution;
  const float dz = grid->resolution;

  const int nx = grid->dims[0];
  const int ny = grid->dims[1];
  const int nz = grid->dims[2];

  /* Update Hx */
  for (int k = 0; k < nz - 1; k++) {
    for (int j = 0; j < ny - 1; j++) {
      for (int i = 0; i < nx; i++) {
        const float ez_jp1 = QFEA_tensor_field_get_scalar(tensor, ez_ch, i, j + 1, k);
        const float ez_j = QFEA_tensor_field_get_scalar(tensor, ez_ch, i, j, k);
        const float dEz_dy = (ez_jp1 - ez_j) / dy;

        const float ey_kp1 = QFEA_tensor_field_get_scalar(tensor, ey_ch, i, j, k + 1);
        const float ey_k = QFEA_tensor_field_get_scalar(tensor, ey_ch, i, j, k);
        const float dEy_dz = (ey_kp1 - ey_k) / dz;

        const size_t voxel_idx = i + nx * (j + ny * k);
        const float mu_r = mu ? mu[voxel_idx] : 1.0f;
        const float coef = dt / (VACUUM_PERMEABILITY * mu_r);

        float hx = QFEA_tensor_field_get_scalar(tensor, hx_ch, i, j, k);
        hx -= coef * (dEz_dy - dEy_dz);
        QFEA_tensor_field_set_scalar(tensor, hx_ch, i, j, k, hx);
      }
    }
  }

  /* Update Hy */
  for (int k = 0; k < nz - 1; k++) {
    for (int j = 0; j < ny; j++) {
      for (int i = 0; i < nx - 1; i++) {
        const float ex_kp1 = QFEA_tensor_field_get_scalar(tensor, ex_ch, i, j, k + 1);
        const float ex_k = QFEA_tensor_field_get_scalar(tensor, ex_ch, i, j, k);
        const float dEx_dz = (ex_kp1 - ex_k) / dz;

        const float ez_ip1 = QFEA_tensor_field_get_scalar(tensor, ez_ch, i + 1, j, k);
        const float ez_i = QFEA_tensor_field_get_scalar(tensor, ez_ch, i, j, k);
        const float dEz_dx = (ez_ip1 - ez_i) / dx;

        const size_t voxel_idx = i + nx * (j + ny * k);
        const float mu_r = mu ? mu[voxel_idx] : 1.0f;
        const float coef = dt / (VACUUM_PERMEABILITY * mu_r);

        float hy = QFEA_tensor_field_get_scalar(tensor, hy_ch, i, j, k);
        hy -= coef * (dEx_dz - dEz_dx);
        QFEA_tensor_field_set_scalar(tensor, hy_ch, i, j, k, hy);
      }
    }
  }

  /* Update Hz */
  for (int k = 0; k < nz; k++) {
    for (int j = 0; j < ny - 1; j++) {
      for (int i = 0; i < nx - 1; i++) {
        const float ey_ip1 = QFEA_tensor_field_get_scalar(tensor, ey_ch, i + 1, j, k);
        const float ey_i = QFEA_tensor_field_get_scalar(tensor, ey_ch, i, j, k);
        const float dEy_dx = (ey_ip1 - ey_i) / dx;

        const float ex_jp1 = QFEA_tensor_field_get_scalar(tensor, ex_ch, i, j + 1, k);
        const float ex_j = QFEA_tensor_field_get_scalar(tensor, ex_ch, i, j, k);
        const float dEx_dy = (ex_jp1 - ex_j) / dy;

        const size_t voxel_idx = i + nx * (j + ny * k);
        const float mu_r = mu ? mu[voxel_idx] : 1.0f;
        const float coef = dt / (VACUUM_PERMEABILITY * mu_r);

        float hz = QFEA_tensor_field_get_scalar(tensor, hz_ch, i, j, k);
        hz -= coef * (dEy_dx - dEx_dy);
        QFEA_tensor_field_set_scalar(tensor, hz_ch, i, j, k, hz);
      }
    }
  }
}

/**
 * Update electric field (E-field) using Ampere's law:
 * ∂E/∂t = (1/ε) ∇×H - (σ/ε) E
 *
 * Yee grid staggered updates:
 * Ex(i, j+1/2, k+1/2) = Ex - (Δt/ε)[(Hz(j+1) - Hz(j))/Δy - (Hy(k+1) - Hy(k))/Δz] - (σ·Δt/ε)Ex
 * Ey(i+1/2, j, k+1/2) = Ey - (Δt/ε)[(Hx(k+1) - Hx(k))/Δz - (Hz(i+1) - Hz(i))/Δx] - (σ·Δt/ε)Ey
 * Ez(i+1/2, j+1/2, k) = Ez - (Δt/ε)[(Hy(i+1) - Hy(i))/Δx - (Hx(j+1) - Hx(j))/Δy] - (σ·Δt/ε)Ez
 */
static void fdtd_update_e_field(QFEATensorField *tensor,
                                 int ex_ch,
                                 int ey_ch,
                                 int ez_ch,
                                 int hx_ch,
                                 int hy_ch,
                                 int hz_ch,
                                 const float *epsilon,
                                 const float *sigma,
                                 float dt)
{
  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float dx = grid->resolution;
  const float dy = grid->resolution;
  const float dz = grid->resolution;

  const int nx = grid->dims[0];
  const int ny = grid->dims[1];
  const int nz = grid->dims[2];

  /* Update Ex */
  for (int k = 1; k < nz; k++) {
    for (int j = 1; j < ny; j++) {
      for (int i = 0; i < nx; i++) {
        const float hz_j = QFEA_tensor_field_get_scalar(tensor, hz_ch, i, j, k);
        const float hz_jm1 = QFEA_tensor_field_get_scalar(tensor, hz_ch, i, j - 1, k);
        const float dHz_dy = (hz_j - hz_jm1) / dy;

        const float hy_k = QFEA_tensor_field_get_scalar(tensor, hy_ch, i, j, k);
        const float hy_km1 = QFEA_tensor_field_get_scalar(tensor, hy_ch, i, j, k - 1);
        const float dHy_dz = (hy_k - hy_km1) / dz;

        const size_t voxel_idx = i + nx * (j + ny * k);
        const float eps_r = epsilon ? epsilon[voxel_idx] : 1.0f;
        const float sig = sigma ? sigma[voxel_idx] : 0.0f;

        const float coef = dt / (VACUUM_PERMITTIVITY * eps_r);
        const float loss_factor = 1.0f - (sig * dt) / (VACUUM_PERMITTIVITY * eps_r);

        float ex = QFEA_tensor_field_get_scalar(tensor, ex_ch, i, j, k);
        ex = loss_factor * ex + coef * (dHz_dy - dHy_dz);
        QFEA_tensor_field_set_scalar(tensor, ex_ch, i, j, k, ex);
      }
    }
  }

  /* Update Ey */
  for (int k = 1; k < nz; k++) {
    for (int j = 0; j < ny; j++) {
      for (int i = 1; i < nx; i++) {
        const float hx_k = QFEA_tensor_field_get_scalar(tensor, hx_ch, i, j, k);
        const float hx_km1 = QFEA_tensor_field_get_scalar(tensor, hx_ch, i, j, k - 1);
        const float dHx_dz = (hx_k - hx_km1) / dz;

        const float hz_i = QFEA_tensor_field_get_scalar(tensor, hz_ch, i, j, k);
        const float hz_im1 = QFEA_tensor_field_get_scalar(tensor, hz_ch, i - 1, j, k);
        const float dHz_dx = (hz_i - hz_im1) / dx;

        const size_t voxel_idx = i + nx * (j + ny * k);
        const float eps_r = epsilon ? epsilon[voxel_idx] : 1.0f;
        const float sig = sigma ? sigma[voxel_idx] : 0.0f;

        const float coef = dt / (VACUUM_PERMITTIVITY * eps_r);
        const float loss_factor = 1.0f - (sig * dt) / (VACUUM_PERMITTIVITY * eps_r);

        float ey = QFEA_tensor_field_get_scalar(tensor, ey_ch, i, j, k);
        ey = loss_factor * ey + coef * (dHx_dz - dHz_dx);
        QFEA_tensor_field_set_scalar(tensor, ey_ch, i, j, k, ey);
      }
    }
  }

  /* Update Ez */
  for (int k = 0; k < nz; k++) {
    for (int j = 1; j < ny; j++) {
      for (int i = 1; i < nx; i++) {
        const float hy_i = QFEA_tensor_field_get_scalar(tensor, hy_ch, i, j, k);
        const float hy_im1 = QFEA_tensor_field_get_scalar(tensor, hy_ch, i - 1, j, k);
        const float dHy_dx = (hy_i - hy_im1) / dx;

        const float hx_j = QFEA_tensor_field_get_scalar(tensor, hx_ch, i, j, k);
        const float hx_jm1 = QFEA_tensor_field_get_scalar(tensor, hx_ch, i, j - 1, k);
        const float dHx_dy = (hx_j - hx_jm1) / dy;

        const size_t voxel_idx = i + nx * (j + ny * k);
        const float eps_r = epsilon ? epsilon[voxel_idx] : 1.0f;
        const float sig = sigma ? sigma[voxel_idx] : 0.0f;

        const float coef = dt / (VACUUM_PERMITTIVITY * eps_r);
        const float loss_factor = 1.0f - (sig * dt) / (VACUUM_PERMITTIVITY * eps_r);

        float ez = QFEA_tensor_field_get_scalar(tensor, ez_ch, i, j, k);
        ez = loss_factor * ez + coef * (dHy_dx - dHx_dy);
        QFEA_tensor_field_set_scalar(tensor, ez_ch, i, j, k, ez);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Source Application
 * \{ */

static void fdtd_apply_sources(QFEATensorField *tensor,
                                const ListBase *sources,
                                float time,
                                float dt)
{
  if (!sources) {
    return;
  }

  LISTBASE_FOREACH (QFEAEnergySource *, source, sources) {
    /* Calculate source value at current time */
    float amplitude = 0.0f;

    switch (source->waveform) {
      case QFEA_WAVEFORM_SINE: {
        amplitude = source->amplitude * sinf(2.0f * M_PI * source->frequency * time);
        break;
      }

      case QFEA_WAVEFORM_GAUSSIAN_PULSE: {
        const float t0 = 3.0f / source->frequency;  /* Pulse center */
        const float tau = 1.0f / source->frequency; /* Pulse width */
        const float t_rel = time - t0;
        amplitude = source->amplitude * expf(-(t_rel * t_rel) / (tau * tau));
        break;
      }

      case QFEA_WAVEFORM_RICKER_WAVELET: {
        const float t0 = 1.0f / source->frequency;
        const float t_rel = time - t0;
        const float f = source->frequency;
        const float arg = (M_PI * f * t_rel) * (M_PI * f * t_rel);
        amplitude = source->amplitude * (1.0f - 2.0f * arg) * expf(-arg);
        break;
      }

      case QFEA_WAVEFORM_CUSTOM:
        /* Custom waveform would be sampled from lookup table */
        break;
    }

    /* Apply source at specified position */
    const int i = (int)((source->position[0] - tensor->voxel_grid->origin[0]) /
                        tensor->voxel_grid->resolution);
    const int j = (int)((source->position[1] - tensor->voxel_grid->origin[1]) /
                        tensor->voxel_grid->resolution);
    const int k = (int)((source->position[2] - tensor->voxel_grid->origin[2]) /
                        tensor->voxel_grid->resolution);

    /* Bounds check */
    if (i < 0 || i >= tensor->voxel_grid->dims[0] || j < 0 ||
        j >= tensor->voxel_grid->dims[1] || k < 0 || k >= tensor->voxel_grid->dims[2])
    {
      continue;
    }

    /* Add to appropriate field component */
    const int channel = 0;  /* Ex channel, would be configurable */
    float current = QFEA_tensor_field_get_scalar(tensor, channel, i, j, k);
    current += amplitude;
    QFEA_tensor_field_set_scalar(tensor, channel, i, j, k, current);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boundary Conditions
 * \{ */

/**
 * Apply Perfectly Matched Layer (PML) absorbing boundary.
 *
 * PML is a lossy region that absorbs outgoing waves without reflection.
 * Uses coordinate stretching in complex space.
 */
static void fdtd_apply_pml_boundary(QFEATensorField *tensor,
                                     const QFEABoundaryCondition *bc,
                                     int thickness,
                                     float attenuation)
{
  if (!tensor || !bc || thickness <= 0) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;

  /* Apply PML at specified face */
  for (int i = 0; i < thickness; i++) {
    const float depth_ratio = (float)(thickness - i) / thickness;
    const float sigma_pml = attenuation * depth_ratio * depth_ratio;

    /* Would apply PML conductivity profile in absorption layer */
    /* Implementation details depend on split-field PML formulation */
  }
}

/**
 * Apply Perfect Electric Conductor (PEC) boundary.
 * Sets tangential E-field to zero at boundary.
 */
static void fdtd_apply_pec_boundary(QFEATensorField *tensor,
                                     const QFEABoundaryCondition *bc,
                                     int ex_ch,
                                     int ey_ch,
                                     int ez_ch)
{
  if (!tensor || !bc) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;

  /* Set tangential E-field components to zero at boundary */
  const int nx = grid->dims[0];
  const int ny = grid->dims[1];
  const int nz = grid->dims[2];

  /* Example: X=0 face (set Ey=0, Ez=0) */
  for (int k = 0; k < nz; k++) {
    for (int j = 0; j < ny; j++) {
      QFEA_tensor_field_set_scalar(tensor, ey_ch, 0, j, k, 0.0f);
      QFEA_tensor_field_set_scalar(tensor, ez_ch, 0, j, k, 0.0f);
    }
  }
}

/**
 * Apply periodic boundary condition.
 * Copies field values from opposite boundaries.
 */
static void fdtd_apply_periodic_boundary(QFEATensorField *tensor, const QFEABoundaryCondition *bc)
{
  if (!tensor || !bc) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const int nx = grid->dims[0];
  const int ny = grid->dims[1];
  const int nz = grid->dims[2];

  /* Copy fields from one boundary to opposite boundary */
  for (int ch = 0; ch < tensor->num_channels; ch++) {
    for (int k = 0; k < nz; k++) {
      for (int j = 0; j < ny; j++) {
        /* X-direction periodicity */
        const float val = QFEA_tensor_field_get_scalar(tensor, ch, nx - 2, j, k);
        QFEA_tensor_field_set_scalar(tensor, ch, 0, j, k, val);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main FDTD Step
 * \{ */

void QFEA_fdtd_step(QFEATensorField *tensor,
                    const QFEAMaterial **materials,
                    const ListBase *sources,
                    const ListBase *bcs,
                    float dt)
{
  if (!tensor) {
    return;
  }

  /* Extract material properties */
  const size_t total_voxels = (size_t)tensor->voxel_grid->dims[0] *
                               tensor->voxel_grid->dims[1] * tensor->voxel_grid->dims[2];

  float *epsilon = (float *)MEM_mallocN(sizeof(float) * total_voxels, "epsilon array");
  float *mu = (float *)MEM_mallocN(sizeof(float) * total_voxels, "mu array");
  float *sigma = (float *)MEM_mallocN(sizeof(float) * total_voxels, "sigma array");

  for (size_t i = 0; i < total_voxels; i++) {
    const QFEAMaterial *mat = materials ? materials[i] : NULL;
    epsilon[i] = mat ? mat->epsilon_r : 1.0f;
    mu[i] = mat ? mat->mu_r : 1.0f;
    sigma[i] = mat ? mat->sigma : 0.0f;
  }

  /* Assume channels 0-2 are Ex, Ey, Ez and 3-5 are Hx, Hy, Hz */
  const int ex_ch = 0, ey_ch = 1, ez_ch = 2;
  const int hx_ch = 3, hy_ch = 4, hz_ch = 5;

  /* FDTD leapfrog scheme */

  /* Update H-field (half timestep ahead of E-field) */
  fdtd_update_h_field(tensor, ex_ch, ey_ch, ez_ch, hx_ch, hy_ch, hz_ch, mu, dt);

  /* Apply boundary conditions to H-field */
  if (bcs) {
    LISTBASE_FOREACH (QFEABoundaryCondition *, bc, bcs) {
      switch (bc->type) {
        case QFEA_BC_PML:
          fdtd_apply_pml_boundary(tensor, bc, 10, 0.01f);
          break;
        case QFEA_BC_PERIODIC:
          fdtd_apply_periodic_boundary(tensor, bc);
          break;
        default:
          break;
      }
    }
  }

  /* Update E-field */
  fdtd_update_e_field(tensor, ex_ch, ey_ch, ez_ch, hx_ch, hy_ch, hz_ch, epsilon, sigma, dt);

  /* Apply sources */
  static float time = 0.0f;
  fdtd_apply_sources(tensor, sources, time, dt);
  time += dt;

  /* Apply boundary conditions to E-field */
  if (bcs) {
    LISTBASE_FOREACH (QFEABoundaryCondition *, bc, bcs) {
      switch (bc->type) {
        case QFEA_BC_PEC:
          fdtd_apply_pec_boundary(tensor, bc, ex_ch, ey_ch, ez_ch);
          break;
        case QFEA_BC_PML:
          fdtd_apply_pml_boundary(tensor, bc, 10, 0.01f);
          break;
        case QFEA_BC_PERIODIC:
          fdtd_apply_periodic_boundary(tensor, bc);
          break;
        default:
          break;
      }
    }
  }

  /* Cleanup */
  MEM_freeN(epsilon);
  MEM_freeN(mu);
  MEM_freeN(sigma);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void QFEA_fdtd_apply_pml(QFEATensorField *tensor,
                         const QFEABoundaryCondition *bc,
                         int thickness,
                         float attenuation)
{
  fdtd_apply_pml_boundary(tensor, bc, thickness, attenuation);
}

/** \} */
