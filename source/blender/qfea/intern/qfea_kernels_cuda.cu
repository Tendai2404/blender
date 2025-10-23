/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup qfea
 *
 * QFEA CUDA kernels for GPU-accelerated physics computation.
 */

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>

/* Physical constants */
#define SPEED_OF_LIGHT 299792458.0f
#define VACUUM_PERMITTIVITY 8.854187817e-12f
#define VACUUM_PERMEABILITY 1.256637062e-6f
#define HBAR 1.054571817e-34f

/* -------------------------------------------------------------------- */
/** \name FDTD Electromagnetic Kernels
 * \{ */

/**
 * CUDA kernel for FDTD H-field update.
 *
 * Updates magnetic field: ∂H/∂t = -(1/μ) ∇×E
 *
 * Grid: 3D with each thread handling one voxel
 * Staggered Yee grid: H-field updated at half time-steps
 */
__global__ void cuda_fdtd_update_h_field(float *ex,
                                          float *ey,
                                          float *ez,
                                          float *hx,
                                          float *hy,
                                          float *hz,
                                          const float *mu,
                                          float dt,
                                          float dx,
                                          float dy,
                                          float dz,
                                          int nx,
                                          int ny,
                                          int nz)
{
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  int j = blockIdx.y * blockDim.y + threadIdx.y;
  int k = blockIdx.z * blockDim.z + threadIdx.z;

  if (i >= nx || j >= ny - 1 || k >= nz - 1)
    return;

  const int idx = i + nx * (j + ny * k);

  const float mu_0 = VACUUM_PERMEABILITY;
  const float mu_r = mu ? mu[idx] : 1.0f;
  const float coef = dt / (mu_0 * mu_r);

  /* Update Hx: Hx -= (dt/μ)[(∂Ez/∂y) - (∂Ey/∂z)] */
  if (j < ny - 1 && k < nz - 1) {
    const int idx_jp1 = i + nx * ((j + 1) + ny * k);
    const int idx_kp1 = i + nx * (j + ny * (k + 1));

    const float dEz_dy = (ez[idx_jp1] - ez[idx]) / dy;
    const float dEy_dz = (ey[idx_kp1] - ey[idx]) / dz;

    hx[idx] -= coef * (dEz_dy - dEy_dz);
  }

  /* Update Hy: Hy -= (dt/μ)[(∂Ex/∂z) - (∂Ez/∂x)] */
  if (i < nx - 1 && k < nz - 1) {
    const int idx_ip1 = (i + 1) + nx * (j + ny * k);
    const int idx_kp1 = i + nx * (j + ny * (k + 1));

    const float dEx_dz = (ex[idx_kp1] - ex[idx]) / dz;
    const float dEz_dx = (ez[idx_ip1] - ez[idx]) / dx;

    hy[idx] -= coef * (dEx_dz - dEz_dx);
  }

  /* Update Hz: Hz -= (dt/μ)[(∂Ey/∂x) - (∂Ex/∂y)] */
  if (i < nx - 1 && j < ny - 1) {
    const int idx_ip1 = (i + 1) + nx * (j + ny * k);
    const int idx_jp1 = i + nx * ((j + 1) + ny * k);

    const float dEy_dx = (ey[idx_ip1] - ey[idx]) / dx;
    const float dEx_dy = (ex[idx_jp1] - ex[idx]) / dy;

    hz[idx] -= coef * (dEy_dx - dEx_dy);
  }
}

/**
 * CUDA kernel for FDTD E-field update.
 *
 * Updates electric field: ∂E/∂t = (1/ε)∇×H - (σ/ε)E
 *
 * Includes lossy media with conductivity σ
 */
__global__ void cuda_fdtd_update_e_field(float *ex,
                                          float *ey,
                                          float *ez,
                                          const float *hx,
                                          const float *hy,
                                          const float *hz,
                                          const float *epsilon,
                                          const float *sigma,
                                          float dt,
                                          float dx,
                                          float dy,
                                          float dz,
                                          int nx,
                                          int ny,
                                          int nz)
{
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  int j = blockIdx.y * blockDim.y + threadIdx.y;
  int k = blockIdx.z * blockDim.z + threadIdx.z;

  if (i >= nx || j >= ny || k >= nz)
    return;

  const int idx = i + nx * (j + ny * k);

  const float eps_0 = VACUUM_PERMITTIVITY;
  const float eps_r = epsilon ? epsilon[idx] : 1.0f;
  const float sig = sigma ? sigma[idx] : 0.0f;

  const float coef = dt / (eps_0 * eps_r);
  const float loss_factor = 1.0f - (sig * dt) / (eps_0 * eps_r);

  /* Update Ex: Ex = loss*Ex + (dt/ε)[(∂Hz/∂y) - (∂Hy/∂z)] */
  if (i > 0 && j > 0 && k > 0) {
    const int idx_jm1 = i + nx * ((j - 1) + ny * k);
    const int idx_km1 = i + nx * (j + ny * (k - 1));

    const float dHz_dy = (hz[idx] - hz[idx_jm1]) / dy;
    const float dHy_dz = (hy[idx] - hy[idx_km1]) / dz;

    ex[idx] = loss_factor * ex[idx] + coef * (dHz_dy - dHy_dz);
  }

  /* Update Ey: Ey = loss*Ey + (dt/ε)[(∂Hx/∂z) - (∂Hz/∂x)] */
  if (i > 0 && j > 0 && k > 0) {
    const int idx_im1 = (i - 1) + nx * (j + ny * k);
    const int idx_km1 = i + nx * (j + ny * (k - 1));

    const float dHx_dz = (hx[idx] - hx[idx_km1]) / dz;
    const float dHz_dx = (hz[idx] - hz[idx_im1]) / dx;

    ey[idx] = loss_factor * ey[idx] + coef * (dHx_dz - dHz_dx);
  }

  /* Update Ez: Ez = loss*Ez + (dt/ε)[(∂Hy/∂x) - (∂Hx/∂y)] */
  if (i > 0 && j > 0 && k > 0) {
    const int idx_im1 = (i - 1) + nx * (j + ny * k);
    const int idx_jm1 = i + nx * ((j - 1) + ny * k);

    const float dHy_dx = (hy[idx] - hy[idx_im1]) / dx;
    const float dHx_dy = (hx[idx] - hx[idx_jm1]) / dy;

    ez[idx] = loss_factor * ez[idx] + coef * (dHy_dx - dHx_dy);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Quantum Mechanics Kernels
 * \{ */

/**
 * CUDA kernel for potential operator application.
 *
 * Applies: ψ' = exp(-iV·dt/ℏ) ψ
 *
 * Complex wavefunction: psi = psi_real + i*psi_imag
 */
__global__ void cuda_quantum_apply_potential(float *psi_real,
                                              float *psi_imag,
                                              const float *potential,
                                              float dt,
                                              float mass,
                                              int nx,
                                              int ny,
                                              int nz)
{
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  int j = blockIdx.y * blockDim.y + threadIdx.y;
  int k = blockIdx.z * blockDim.z + threadIdx.z;

  if (i >= nx || j >= ny || k >= nz)
    return;

  const int idx = i + nx * (j + ny * k);

  const float V = potential ? potential[idx] : 0.0f;
  const float phase = -dt * V / HBAR;

  const float cos_phase = cosf(phase);
  const float sin_phase = sinf(phase);

  const float psi_r = psi_real[idx];
  const float psi_i = psi_imag[idx];

  /* Complex multiplication: (cos - i*sin)(psi_r + i*psi_i) */
  psi_real[idx] = cos_phase * psi_r + sin_phase * psi_i;
  psi_imag[idx] = cos_phase * psi_i - sin_phase * psi_r;
}

/**
 * CUDA kernel for kinetic operator (Laplacian approximation).
 *
 * Applies: ψ' = ψ + (-iℏ²/(2m))∇²ψ·dt
 *
 * In production, this would use cuFFT for exact kinetic operator
 */
__global__ void cuda_quantum_apply_kinetic(float *psi_real,
                                            float *psi_imag,
                                            float dt,
                                            float mass,
                                            float dx,
                                            float dy,
                                            float dz,
                                            int nx,
                                            int ny,
                                            int nz)
{
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  int j = blockIdx.y * blockDim.y + threadIdx.y;
  int k = blockIdx.z * blockDim.z + threadIdx.z;

  if (i <= 0 || i >= nx - 1 || j <= 0 || j >= ny - 1 || k <= 0 || k >= nz - 1)
    return;

  const int idx = i + nx * (j + ny * k);

  const float psi_r = psi_real[idx];
  const float psi_i = psi_imag[idx];

  /* Compute Laplacian: ∇²ψ */
  const float psi_r_ip1 = psi_real[(i + 1) + nx * (j + ny * k)];
  const float psi_r_im1 = psi_real[(i - 1) + nx * (j + ny * k)];
  const float psi_r_jp1 = psi_real[i + nx * ((j + 1) + ny * k)];
  const float psi_r_jm1 = psi_real[i + nx * ((j - 1) + ny * k)];
  const float psi_r_kp1 = psi_real[i + nx * (j + ny * (k + 1))];
  const float psi_r_km1 = psi_real[i + nx * (j + ny * (k - 1))];

  const float lap_r = (psi_r_ip1 + psi_r_im1 + psi_r_jp1 + psi_r_jm1 + psi_r_kp1 + psi_r_km1 -
                       6.0f * psi_r) /
                      (dx * dx);

  const float psi_i_ip1 = psi_imag[(i + 1) + nx * (j + ny * k)];
  const float psi_i_im1 = psi_imag[(i - 1) + nx * (j + ny * k)];
  const float psi_i_jp1 = psi_imag[i + nx * ((j + 1) + ny * k)];
  const float psi_i_jm1 = psi_imag[i + nx * ((j - 1) + ny * k)];
  const float psi_i_kp1 = psi_imag[i + nx * (j + ny * (k + 1))];
  const float psi_i_km1 = psi_imag[i + nx * (j + ny * (k - 1))];

  const float lap_i = (psi_i_ip1 + psi_i_im1 + psi_i_jp1 + psi_i_jm1 + psi_i_kp1 + psi_i_km1 -
                       6.0f * psi_i) /
                      (dx * dx);

  /* Apply kinetic operator: (-iℏ²/(2m))∇²ψ */
  const float factor = -HBAR * dt / (2.0f * mass);
  const float kinetic_r = -factor * lap_i / HBAR;
  const float kinetic_i = factor * lap_r / HBAR;

  psi_real[idx] += kinetic_r;
  psi_imag[idx] += kinetic_i;
}

/**
 * CUDA kernel for wavefunction normalization.
 *
 * Computes ∫|ψ|²dV and scales ψ so integral = 1
 */
__global__ void cuda_quantum_normalize(float *psi_real,
                                        float *psi_imag,
                                        float *norm_sum,
                                        float voxel_volume,
                                        int total_voxels,
                                        bool compute_norm)
{
  int idx = blockIdx.x * blockDim.x + threadIdx.x;

  if (idx >= total_voxels)
    return;

  const float psi_r = psi_real[idx];
  const float psi_i = psi_imag[idx];
  const float prob_density = psi_r * psi_r + psi_i * psi_i;

  if (compute_norm) {
    /* Reduction to compute total norm */
    atomicAdd(norm_sum, prob_density * voxel_volume);
  }
  else {
    /* Apply normalization factor */
    const float norm_factor = 1.0f / sqrtf(*norm_sum);
    psi_real[idx] = psi_r * norm_factor;
    psi_imag[idx] = psi_i * norm_factor;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle Dynamics Kernels
 * \{ */

/**
 * Particle structure (device-side).
 */
struct CUDAParticle {
  float position[3];
  float velocity[3];
  float mass;
  float charge;
  int element_id;
  int mass_number;
};

/**
 * CUDA kernel for Lorentz force particle update.
 *
 * Updates particle positions/velocities using F = q(E + v×B)
 */
__global__ void cuda_particle_lorentz_push(struct CUDAParticle *particles,
                                            const float *ex,
                                            const float *ey,
                                            const float *ez,
                                            const float *bx,
                                            const float *by,
                                            const float *bz,
                                            float dt,
                                            int num_particles,
                                            int nx,
                                            int ny,
                                            int nz,
                                            float dx,
                                            float origin_x,
                                            float origin_y,
                                            float origin_z)
{
  int particle_idx = blockIdx.x * blockDim.x + threadIdx.x;

  if (particle_idx >= num_particles)
    return;

  struct CUDAParticle *p = &particles[particle_idx];

  /* Convert position to grid coordinates */
  const float fx = (p->position[0] - origin_x) / dx;
  const float fy = (p->position[1] - origin_y) / dx;
  const float fz = (p->position[2] - origin_z) / dx;

  const int i = (int)floorf(fx);
  const int j = (int)floorf(fy);
  const int k = (int)floorf(fz);

  /* Bounds check */
  if (i < 0 || i >= nx - 1 || j < 0 || j >= ny - 1 || k < 0 || k >= nz - 1)
    return;

  /* Trilinear interpolation for E and B fields */
  const int idx = i + nx * (j + ny * k);

  /* Simplified: just use nearest grid point (production would interpolate) */
  const float e_field[3] = {ex[idx], ey[idx], ez[idx]};
  const float b_field[3] = {bx[idx], by[idx], bz[idx]};

  /* v × B */
  const float v_cross_b[3] = {p->velocity[1] * b_field[2] - p->velocity[2] * b_field[1],
                               p->velocity[2] * b_field[0] - p->velocity[0] * b_field[2],
                               p->velocity[0] * b_field[1] - p->velocity[1] * b_field[0]};

  /* Lorentz force: F = q(E + v×B) */
  const float force[3] = {p->charge * (e_field[0] + v_cross_b[0]),
                          p->charge * (e_field[1] + v_cross_b[1]),
                          p->charge * (e_field[2] + v_cross_b[2])};

  /* Update velocity: v += (F/m) * dt */
  const float inv_mass = 1.0f / p->mass;
  p->velocity[0] += force[0] * inv_mass * dt;
  p->velocity[1] += force[1] * inv_mass * dt;
  p->velocity[2] += force[2] * inv_mass * dt;

  /* Update position: x += v * dt */
  p->position[0] += p->velocity[0] * dt;
  p->position[1] += p->velocity[1] * dt;
  p->position[2] += p->velocity[2] * dt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility Kernels
 * \{ */

/**
 * CUDA kernel for tensor field operation: add two fields.
 */
__global__ void cuda_tensor_add(float *result,
                                 const float *field1,
                                 const float *field2,
                                 int total_elements)
{
  int idx = blockIdx.x * blockDim.x + threadIdx.x;

  if (idx >= total_elements)
    return;

  result[idx] = field1[idx] + field2[idx];
}

/**
 * CUDA kernel for tensor field operation: multiply by scalar.
 */
__global__ void cuda_tensor_scale(float *field, float scale, int total_elements)
{
  int idx = blockIdx.x * blockDim.x + threadIdx.x;

  if (idx >= total_elements)
    return;

  field[idx] *= scale;
}

/**
 * CUDA kernel for computing field energy.
 */
__global__ void cuda_compute_field_energy(const float *ex,
                                           const float *ey,
                                           const float *ez,
                                           const float *epsilon,
                                           float *energy_sum,
                                           float voxel_volume,
                                           int total_voxels)
{
  int idx = blockIdx.x * blockDim.x + threadIdx.x;

  if (idx >= total_voxels)
    return;

  const float eps_r = epsilon ? epsilon[idx] : 1.0f;
  const float e_squared = ex[idx] * ex[idx] + ey[idx] * ey[idx] + ez[idx] * ez[idx];

  const float energy = 0.5f * VACUUM_PERMITTIVITY * eps_r * e_squared * voxel_volume;

  atomicAdd(energy_sum, energy);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Host Interface Functions
 * \{ */

extern "C" {

/**
 * Launch FDTD H-field update kernel.
 */
void cuda_launch_fdtd_h_update(float *d_ex,
                                float *d_ey,
                                float *d_ez,
                                float *d_hx,
                                float *d_hy,
                                float *d_hz,
                                const float *d_mu,
                                float dt,
                                float dx,
                                int nx,
                                int ny,
                                int nz,
                                cudaStream_t stream)
{
  dim3 block(8, 8, 8);
  dim3 grid((nx + block.x - 1) / block.x, (ny + block.y - 1) / block.y,
            (nz + block.z - 1) / block.z);

  cuda_fdtd_update_h_field<<<grid, block, 0, stream>>>(
      d_ex, d_ey, d_ez, d_hx, d_hy, d_hz, d_mu, dt, dx, dx, dx, nx, ny, nz);
}

/**
 * Launch FDTD E-field update kernel.
 */
void cuda_launch_fdtd_e_update(float *d_ex,
                                float *d_ey,
                                float *d_ez,
                                const float *d_hx,
                                const float *d_hy,
                                const float *d_hz,
                                const float *d_epsilon,
                                const float *d_sigma,
                                float dt,
                                float dx,
                                int nx,
                                int ny,
                                int nz,
                                cudaStream_t stream)
{
  dim3 block(8, 8, 8);
  dim3 grid((nx + block.x - 1) / block.x, (ny + block.y - 1) / block.y,
            (nz + block.z - 1) / block.z);

  cuda_fdtd_update_e_field<<<grid, block, 0, stream>>>(d_ex,
                                                         d_ey,
                                                         d_ez,
                                                         d_hx,
                                                         d_hy,
                                                         d_hz,
                                                         d_epsilon,
                                                         d_sigma,
                                                         dt,
                                                         dx,
                                                         dx,
                                                         dx,
                                                         nx,
                                                         ny,
                                                         nz);
}

} /* extern "C" */

/** \} */
