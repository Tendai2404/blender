/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup qfea
 *
 * QFEA Schrödinger equation quantum mechanics solver.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_qfea_types.h"

#include "QFEA_physics.h"
#include "QFEA_tensor.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Physical constants */
#define HBAR 1.054571817e-34f          /* J·s (reduced Planck constant) */
#define ELECTRON_MASS 9.1093837015e-31f /* kg */
#define ELEMENTARY_CHARGE 1.602176634e-19f /* C */

/* -------------------------------------------------------------------- */
/** \name Split-Operator Method
 * \{ */

/**
 * Split-operator method for time evolution:
 * ψ(t+Δt) = exp(-iĤΔt/ℏ) ψ(t)
 *
 * Split Hamiltonian: Ĥ = T̂ + V̂
 * where T̂ = -ℏ²∇²/(2m) (kinetic) and V̂ = V(r) (potential)
 *
 * Second-order Trotter splitting:
 * exp(-iĤΔt/ℏ) ≈ exp(-iT̂Δt/(2ℏ)) exp(-iV̂Δt/ℏ) exp(-iT̂Δt/(2ℏ))
 *
 * Algorithm:
 * 1. Apply half-step kinetic evolution in momentum space (FFT required)
 * 2. Apply full-step potential evolution in position space
 * 3. Apply half-step kinetic evolution in momentum space
 */

/**
 * Apply potential energy operator: ψ' = exp(-iV·Δt/ℏ) ψ
 */
static void quantum_apply_potential(QFEATensorField *tensor,
                                     int psi_real_ch,
                                     int psi_imag_ch,
                                     const float *potential,
                                     float dt,
                                     float particle_mass)
{
  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const int nx = grid->dims[0];
  const int ny = grid->dims[1];
  const int nz = grid->dims[2];

  const float factor = -dt / HBAR;

  for (int k = 0; k < nz; k++) {
    for (int j = 0; j < ny; j++) {
      for (int i = 0; i < nx; i++) {
        const size_t voxel_idx = i + nx * (j + ny * k);
        const float V = potential ? potential[voxel_idx] : 0.0f;

        /* Phase rotation: exp(-iVΔt/ℏ) = cos(VΔt/ℏ) - i·sin(VΔt/ℏ) */
        const float phase = factor * V;
        const float cos_phase = cosf(phase);
        const float sin_phase = sinf(phase);

        const float psi_r = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k);
        const float psi_i = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k);

        /* Complex multiplication: (cos - i·sin)(psi_r + i·psi_i) */
        const float new_psi_r = cos_phase * psi_r + sin_phase * psi_i;
        const float new_psi_i = cos_phase * psi_i - sin_phase * psi_r;

        QFEA_tensor_field_set_scalar(tensor, psi_real_ch, i, j, k, new_psi_r);
        QFEA_tensor_field_set_scalar(tensor, psi_imag_ch, i, j, k, new_psi_i);
      }
    }
  }
}

/**
 * Apply kinetic energy operator in momentum space.
 *
 * In momentum space: T̂ψ = (ℏ²k²/(2m))ψ
 * where k = (kx, ky, kz) is wave vector
 *
 * Time evolution: ψ' = exp(-iT̂·Δt/ℏ) ψ = exp(-iℏk²Δt/(2m)) ψ
 *
 * Note: In production, this would use FFT library (FFTW, cuFFT, etc.)
 * For now, we'll implement a simplified version
 */
static void quantum_apply_kinetic_fft(QFEATensorField *tensor,
                                       int psi_real_ch,
                                       int psi_imag_ch,
                                       float dt,
                                       float particle_mass)
{
  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float dx = grid->resolution;

  /* Wave vector spacing: Δk = 2π/(N·Δx) */
  const float dkx = 2.0f * M_PI / (grid->dims[0] * dx);
  const float dky = 2.0f * M_PI / (grid->dims[1] * dx);
  const float dkz = 2.0f * M_PI / (grid->dims[2] * dx);

  const float factor = -HBAR * dt / (2.0f * particle_mass);

  /* In production: perform 3D FFT to momentum space */
  /* For now, apply approximate kinetic operator in position space using finite differences */

  /* This is a placeholder - real implementation requires FFT */
  /* Here we approximate T̂ = -ℏ²∇²/(2m) using Laplacian */

  for (int k = 1; k < grid->dims[2] - 1; k++) {
    for (int j = 1; j < grid->dims[1] - 1; j++) {
      for (int i = 1; i < grid->dims[0] - 1; i++) {
        /* Get wavefunction and neighbors */
        const float psi_r = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k);
        const float psi_i = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k);

        /* Compute Laplacian: ∇²ψ */
        const float psi_r_ip1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i + 1, j, k);
        const float psi_r_im1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i - 1, j, k);
        const float psi_r_jp1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j + 1, k);
        const float psi_r_jm1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j - 1, k);
        const float psi_r_kp1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k + 1);
        const float psi_r_km1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k - 1);

        const float lap_r = (psi_r_ip1 + psi_r_im1 + psi_r_jp1 + psi_r_jm1 + psi_r_kp1 +
                             psi_r_km1 - 6.0f * psi_r) /
                            (dx * dx);

        const float psi_i_ip1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i + 1, j, k);
        const float psi_i_im1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i - 1, j, k);
        const float psi_i_jp1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j + 1, k);
        const float psi_i_jm1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j - 1, k);
        const float psi_i_kp1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k + 1);
        const float psi_i_km1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k - 1);

        const float lap_i = (psi_i_ip1 + psi_i_im1 + psi_i_jp1 + psi_i_jm1 + psi_i_kp1 +
                             psi_i_km1 - 6.0f * psi_i) /
                            (dx * dx);

        /* Apply kinetic operator: ψ += (-iℏ²/(2m)) ∇²ψ · Δt */
        /* -i multiplies real by 0 and imag by -1, then swaps them */
        const float kinetic_r = -factor * lap_i / HBAR;  /* Imaginary part contributes to real */
        const float kinetic_i = factor * lap_r / HBAR;   /* Real part contributes to imaginary */

        QFEA_tensor_field_set_scalar(tensor, psi_real_ch, i, j, k, psi_r + kinetic_r);
        QFEA_tensor_field_set_scalar(tensor, psi_imag_ch, i, j, k, psi_i + kinetic_i);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Schrödinger Step
 * \{ */

void QFEA_schrodinger_step(QFEATensorField *tensor,
                            const float *potential,
                            const QFEAQuantumConfig *config,
                            float dt)
{
  if (!tensor || !config) {
    return;
  }

  /* Assume channels 0-1 are psi_real, psi_imag */
  const int psi_real_ch = 0;
  const int psi_imag_ch = 1;

  const float mass = config->particle_mass > 0.0f ? config->particle_mass : ELECTRON_MASS;

  /* Split-operator method: */

  /* 1. Apply half-step kinetic evolution */
  quantum_apply_kinetic_fft(tensor, psi_real_ch, psi_imag_ch, dt * 0.5f, mass);

  /* 2. Apply full-step potential evolution */
  quantum_apply_potential(tensor, psi_real_ch, psi_imag_ch, potential, dt, mass);

  /* 3. Apply half-step kinetic evolution */
  quantum_apply_kinetic_fft(tensor, psi_real_ch, psi_imag_ch, dt * 0.5f, mass);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Wavefunction Analysis
 * \{ */

float QFEA_quantum_compute_probability_density(const QFEATensorField *tensor,
                                                int psi_real_ch,
                                                int psi_imag_ch,
                                                int i,
                                                int j,
                                                int k)
{
  const float psi_r = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k);
  const float psi_i = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k);

  /* |ψ|² = ψ*ψ = (Re(ψ))² + (Im(ψ))² */
  return psi_r * psi_r + psi_i * psi_i;
}

float QFEA_quantum_compute_normalization(const QFEATensorField *tensor,
                                          int psi_real_ch,
                                          int psi_imag_ch)
{
  if (!tensor) {
    return 0.0f;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float voxel_volume = grid->resolution * grid->resolution * grid->resolution;

  double total_probability = 0.0;

  for (int k = 0; k < grid->dims[2]; k++) {
    for (int j = 0; j < grid->dims[1]; j++) {
      for (int i = 0; i < grid->dims[0]; i++) {
        const float prob_density = QFEA_quantum_compute_probability_density(
            tensor, psi_real_ch, psi_imag_ch, i, j, k);
        total_probability += prob_density * voxel_volume;
      }
    }
  }

  return (float)total_probability;
}

void QFEA_quantum_normalize_wavefunction(QFEATensorField *tensor,
                                          int psi_real_ch,
                                          int psi_imag_ch)
{
  const float norm = QFEA_quantum_compute_normalization(tensor, psi_real_ch, psi_imag_ch);

  if (norm <= 0.0f) {
    return;
  }

  const float norm_factor = 1.0f / sqrtf(norm);
  const QFEAVoxelGrid *grid = tensor->voxel_grid;

  /* Multiply wavefunction by normalization factor */
  for (int k = 0; k < grid->dims[2]; k++) {
    for (int j = 0; j < grid->dims[1]; j++) {
      for (int i = 0; i < grid->dims[0]; i++) {
        float psi_r = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k);
        float psi_i = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k);

        QFEA_tensor_field_set_scalar(tensor, psi_real_ch, i, j, k, psi_r * norm_factor);
        QFEA_tensor_field_set_scalar(tensor, psi_imag_ch, i, j, k, psi_i * norm_factor);
      }
    }
  }
}

float QFEA_quantum_compute_expectation_energy(const QFEATensorField *tensor,
                                               int psi_real_ch,
                                               int psi_imag_ch,
                                               const float *potential,
                                               float particle_mass)
{
  if (!tensor) {
    return 0.0f;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float dx = grid->resolution;
  const float voxel_volume = dx * dx * dx;

  double total_energy = 0.0;

  for (int k = 1; k < grid->dims[2] - 1; k++) {
    for (int j = 1; j < grid->dims[1] - 1; j++) {
      for (int i = 1; i < grid->dims[0] - 1; i++) {
        const float psi_r = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k);
        const float psi_i = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k);

        /* Kinetic energy: ⟨T⟩ = -ℏ²/(2m) ⟨ψ|∇²|ψ⟩ */
        /* Compute Laplacian of psi */
        const float psi_r_ip1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i + 1, j, k);
        const float psi_r_im1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i - 1, j, k);
        const float psi_r_jp1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j + 1, k);
        const float psi_r_jm1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j - 1, k);
        const float psi_r_kp1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k + 1);
        const float psi_r_km1 = QFEA_tensor_field_get_scalar(tensor, psi_real_ch, i, j, k - 1);

        const float lap_psi_r = (psi_r_ip1 + psi_r_im1 + psi_r_jp1 + psi_r_jm1 + psi_r_kp1 +
                                 psi_r_km1 - 6.0f * psi_r) /
                                (dx * dx);

        const float psi_i_ip1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i + 1, j, k);
        const float psi_i_im1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i - 1, j, k);
        const float psi_i_jp1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j + 1, k);
        const float psi_i_jm1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j - 1, k);
        const float psi_i_kp1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k + 1);
        const float psi_i_km1 = QFEA_tensor_field_get_scalar(tensor, psi_imag_ch, i, j, k - 1);

        const float lap_psi_i = (psi_i_ip1 + psi_i_im1 + psi_i_jp1 + psi_i_jm1 + psi_i_kp1 +
                                 psi_i_km1 - 6.0f * psi_i) /
                                (dx * dx);

        /* ⟨ψ|∇²|ψ⟩ = ψ* · ∇²ψ = (psi_r - i·psi_i)(lap_r + i·lap_i) */
        const float kinetic_integrand = psi_r * lap_psi_r + psi_i * lap_psi_i;
        const float kinetic_contribution = -HBAR * HBAR / (2.0f * particle_mass) *
                                           kinetic_integrand;

        /* Potential energy: ⟨V⟩ = ⟨ψ|V|ψ⟩ = V·|ψ|² */
        const size_t voxel_idx = i + grid->dims[0] * (j + grid->dims[1] * k);
        const float V = potential ? potential[voxel_idx] : 0.0f;
        const float prob_density = psi_r * psi_r + psi_i * psi_i;
        const float potential_contribution = V * prob_density;

        total_energy += (kinetic_contribution + potential_contribution) * voxel_volume;
      }
    }
  }

  return (float)total_energy;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initial Conditions
 * \{ */

void QFEA_quantum_init_gaussian_wavepacket(QFEATensorField *tensor,
                                            int psi_real_ch,
                                            int psi_imag_ch,
                                            const float center[3],
                                            const float momentum[3],
                                            float width)
{
  if (!tensor) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;

  /* Gaussian wavepacket: ψ(r) = exp(ik·r) exp(-(r-r₀)²/(2σ²)) */

  for (int k = 0; k < grid->dims[2]; k++) {
    for (int j = 0; j < grid->dims[1]; j++) {
      for (int i = 0; i < grid->dims[0]; i++) {
        /* Position in real space */
        const float x = grid->origin[0] + (i + 0.5f) * grid->resolution;
        const float y = grid->origin[1] + (j + 0.5f) * grid->resolution;
        const float z = grid->origin[2] + (k + 0.5f) * grid->resolution;

        /* Distance from center */
        const float dx = x - center[0];
        const float dy = y - center[1];
        const float dz = z - center[2];
        const float r_sq = dx * dx + dy * dy + dz * dz;

        /* Gaussian envelope */
        const float envelope = expf(-r_sq / (2.0f * width * width));

        /* Plane wave phase: k·r/ℏ */
        const float phase = (momentum[0] * x + momentum[1] * y + momentum[2] * z) / HBAR;

        /* ψ = envelope · exp(i·phase) = envelope · (cos(phase) + i·sin(phase)) */
        const float psi_r = envelope * cosf(phase);
        const float psi_i = envelope * sinf(phase);

        QFEA_tensor_field_set_scalar(tensor, psi_real_ch, i, j, k, psi_r);
        QFEA_tensor_field_set_scalar(tensor, psi_imag_ch, i, j, k, psi_i);
      }
    }
  }

  /* Normalize wavefunction */
  QFEA_quantum_normalize_wavefunction(tensor, psi_real_ch, psi_imag_ch);
}

/** \} */
