/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA tensor field management API for multi-channel physical quantities.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Tensor Field Creation
 * \{ */

/**
 * Create new tensor field attached to voxel grid.
 *
 * \param voxel_grid: Spatial discretization
 * \param num_channels: Number of data channels
 * \param channels: Channel definitions
 * \return Tensor field
 */
QFEATensorField *QFEA_tensor_field_new(QFEAVoxelGrid *voxel_grid,
                                        int num_channels,
                                        QFEATensorChannel *channels);

/**
 * Free tensor field and associated data.
 */
void QFEA_tensor_field_free(QFEATensorField *tensor);

/**
 * Copy tensor field (deep copy of all data).
 */
QFEATensorField *QFEA_tensor_field_copy(const QFEATensorField *tensor);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Channel Management
 * \{ */

/**
 * Add channel to tensor field.
 *
 * \param tensor: Tensor field
 * \param name: Channel name (e.g., "Ex", "psi", "temperature")
 * \param unit: Physical unit (e.g., "V/m", "eV", "K")
 * \param channel_type: SCALAR, VECTOR3, TENSOR33
 * \param data_type: FLOAT32, FLOAT64, COMPLEX64, COMPLEX128
 * \return Channel index
 */
int QFEA_tensor_field_add_channel(QFEATensorField *tensor,
                                   const char *name,
                                   const char *unit,
                                   eQFEATensorChannelType channel_type,
                                   eQFEATensorDataType data_type);

/**
 * Remove channel from tensor field.
 */
void QFEA_tensor_field_remove_channel(QFEATensorField *tensor, int channel_index);

/**
 * Find channel index by name.
 *
 * \return Channel index or -1 if not found
 */
int QFEA_tensor_field_find_channel(const QFEATensorField *tensor, const char *name);

/**
 * Get channel definition.
 */
const QFEATensorChannel *QFEA_tensor_field_get_channel(const QFEATensorField *tensor,
                                                         int channel_index);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Access
 * \{ */

/**
 * Get scalar value at voxel.
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to sample
 * \param i, j, k: Voxel indices
 * \return Scalar value
 */
float QFEA_tensor_field_get_scalar(const QFEATensorField *tensor,
                                     int channel_index,
                                     int i,
                                     int j,
                                     int k);

/**
 * Set scalar value at voxel.
 */
void QFEA_tensor_field_set_scalar(QFEATensorField *tensor,
                                   int channel_index,
                                   int i,
                                   int j,
                                   int k,
                                   float value);

/**
 * Get vector value at voxel (for VECTOR3 channels).
 *
 * \param r_vector: Output vector [3]
 */
void QFEA_tensor_field_get_vector3(const QFEATensorField *tensor,
                                    int channel_index,
                                    int i,
                                    int j,
                                    int k,
                                    float r_vector[3]);

/**
 * Set vector value at voxel.
 */
void QFEA_tensor_field_set_vector3(QFEATensorField *tensor,
                                    int channel_index,
                                    int i,
                                    int j,
                                    int k,
                                    const float vector[3]);

/**
 * Get complex value at voxel (for COMPLEX64/128 channels).
 *
 * \param r_real: Output real part
 * \param r_imag: Output imaginary part
 */
void QFEA_tensor_field_get_complex(const QFEATensorField *tensor,
                                     int channel_index,
                                     int i,
                                     int j,
                                     int k,
                                     float *r_real,
                                     float *r_imag);

/**
 * Set complex value at voxel.
 */
void QFEA_tensor_field_set_complex(QFEATensorField *tensor,
                                    int channel_index,
                                    int i,
                                    int j,
                                    int k,
                                    float real,
                                    float imag);

/**
 * Sample field at arbitrary position (trilinear interpolation).
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to sample
 * \param position: World space position
 * \return Interpolated value
 */
float QFEA_tensor_field_sample(const QFEATensorField *tensor,
                                 int channel_index,
                                 const float position[3]);

/**
 * Sample vector field at position.
 */
void QFEA_tensor_field_sample_vector3(const QFEATensorField *tensor,
                                       int channel_index,
                                       const float position[3],
                                       float r_vector[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Field Operations
 * \{ */

/**
 * Fill channel with constant value.
 */
void QFEA_tensor_field_fill(QFEATensorField *tensor, int channel_index, float value);

/**
 * Fill with function of position.
 *
 * \param func: Function pointer: value = func(position, userdata)
 */
typedef float (*QFEATensorFunc)(const float position[3], void *userdata);

void QFEA_tensor_field_fill_function(QFEATensorField *tensor,
                                      int channel_index,
                                      QFEATensorFunc func,
                                      void *userdata);

/**
 * Copy one channel to another.
 */
void QFEA_tensor_field_copy_channel(QFEATensorField *tensor,
                                     int src_channel,
                                     int dst_channel);

/**
 * Arithmetic: dst = src1 + src2 (element-wise).
 */
void QFEA_tensor_field_add(QFEATensorField *tensor,
                            int src1_channel,
                            int src2_channel,
                            int dst_channel);

/**
 * Arithmetic: dst = src1 - src2.
 */
void QFEA_tensor_field_subtract(QFEATensorField *tensor,
                                 int src1_channel,
                                 int src2_channel,
                                 int dst_channel);

/**
 * Arithmetic: dst = src1 * src2.
 */
void QFEA_tensor_field_multiply(QFEATensorField *tensor,
                                 int src1_channel,
                                 int src2_channel,
                                 int dst_channel);

/**
 * Scale: dst = src * scale.
 */
void QFEA_tensor_field_scale(QFEATensorField *tensor,
                              int src_channel,
                              int dst_channel,
                              float scale);

/**
 * Compute magnitude of vector field: dst = sqrt(vx² + vy² + vz²).
 *
 * \param vx_channel, vy_channel, vz_channel: Vector components
 * \param dst_channel: Output scalar channel
 */
void QFEA_tensor_field_magnitude(QFEATensorField *tensor,
                                  int vx_channel,
                                  int vy_channel,
                                  int vz_channel,
                                  int dst_channel);

/**
 * Compute gradient of scalar field: ∇φ.
 *
 * Uses central differences for interior voxels, forward/backward at boundaries.
 *
 * \param scalar_channel: Input scalar field
 * \param grad_x_channel, grad_y_channel, grad_z_channel: Output gradient components
 */
void QFEA_tensor_field_gradient(QFEATensorField *tensor,
                                 int scalar_channel,
                                 int grad_x_channel,
                                 int grad_y_channel,
                                 int grad_z_channel);

/**
 * Compute divergence of vector field: ∇·V.
 *
 * \param vx_channel, vy_channel, vz_channel: Vector field components
 * \param div_channel: Output divergence (scalar)
 */
void QFEA_tensor_field_divergence(QFEATensorField *tensor,
                                   int vx_channel,
                                   int vy_channel,
                                   int vz_channel,
                                   int div_channel);

/**
 * Compute curl of vector field: ∇×V.
 *
 * \param vx_channel, vy_channel, vz_channel: Input vector field
 * \param curl_x_channel, curl_y_channel, curl_z_channel: Output curl components
 */
void QFEA_tensor_field_curl(QFEATensorField *tensor,
                             int vx_channel,
                             int vy_channel,
                             int vz_channel,
                             int curl_x_channel,
                             int curl_y_channel,
                             int curl_z_channel);

/**
 * Compute Laplacian: ∇²φ.
 *
 * Uses 7-point stencil (6 neighbors + center).
 *
 * \param src_channel: Input scalar field
 * \param dst_channel: Output Laplacian
 */
void QFEA_tensor_field_laplacian(QFEATensorField *tensor, int src_channel, int dst_channel);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Analysis
 * \{ */

/**
 * Compute statistics over entire field.
 */
typedef struct QFEATensorStats {
  float min;
  float max;
  float mean;
  float std_dev;
  float rms;
  int num_samples;
} QFEATensorStats;

void QFEA_tensor_field_compute_stats(const QFEATensorField *tensor,
                                      int channel_index,
                                      QFEATensorStats *r_stats);

/**
 * Integrate field over volume: ∫f dV.
 *
 * \param channel_index: Channel to integrate
 * \return Integral value
 */
float QFEA_tensor_field_integrate_volume(const QFEATensorField *tensor, int channel_index);

/**
 * Integrate over region.
 *
 * \param region_mask: Boolean mask (1=include, 0=exclude) per voxel
 */
float QFEA_tensor_field_integrate_region(const QFEATensorField *tensor,
                                          int channel_index,
                                          const char *region_mask);

/**
 * Compute total energy: E = 0.5 * ε₀ * ∫|E|² dV for EM field.
 *
 * \param ex_channel, ey_channel, ez_channel: Electric field components
 * \param epsilon: Permittivity per voxel
 * \return Total electric energy (Joules)
 */
float QFEA_tensor_field_compute_electric_energy(const QFEATensorField *tensor,
                                                 int ex_channel,
                                                 int ey_channel,
                                                 int ez_channel,
                                                 const float *epsilon);

/**
 * Compute magnetic energy: E = 0.5 * μ₀ * ∫|H|² dV.
 */
float QFEA_tensor_field_compute_magnetic_energy(const QFEATensorField *tensor,
                                                 int hx_channel,
                                                 int hy_channel,
                                                 int hz_channel,
                                                 const float *mu);

/**
 * Compute Poynting vector flux through surface: ∫(E×H)·dA.
 *
 * \param surface_voxels: Indices of voxels on surface
 * \param num_surface_voxels: Number of surface voxels
 * \param normal: Surface normal direction
 * \return Power flux (Watts)
 */
float QFEA_tensor_field_compute_poynting_flux(const QFEATensorField *tensor,
                                               int ex_channel,
                                               int ey_channel,
                                               int ez_channel,
                                               int hx_channel,
                                               int hy_channel,
                                               int hz_channel,
                                               const int (*surface_voxels)[3],
                                               int num_surface_voxels,
                                               const float normal[3]);

/**
 * Find voxel with maximum field value.
 *
 * \param channel_index: Channel to search
 * \param r_max_index: Output voxel indices [i, j, k]
 * \return Maximum value
 */
float QFEA_tensor_field_find_maximum(const QFEATensorField *tensor,
                                      int channel_index,
                                      int r_max_index[3]);

/**
 * Find voxel with minimum field value.
 */
float QFEA_tensor_field_find_minimum(const QFEATensorField *tensor,
                                      int channel_index,
                                      int r_min_index[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Synchronization
 * \{ */

/**
 * Upload tensor field to GPU as 3D texture.
 *
 * Creates GPU texture for shader sampling. Updates automatically
 * when CPU data changes.
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to upload (or -1 for all channels)
 */
void QFEA_tensor_field_sync_gpu(QFEATensorField *tensor, int channel_index);

/**
 * Download tensor field from GPU to CPU.
 *
 * Used after GPU compute to update CPU copy.
 */
void QFEA_tensor_field_download_gpu(QFEATensorField *tensor, int channel_index);

/**
 * Free GPU resources.
 */
void QFEA_tensor_field_gpu_free(QFEATensorField *tensor);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Time Evolution
 * \{ */

/**
 * Advance tensor field to next timestep.
 *
 * Wrapper that calls appropriate physics solver based on configuration.
 *
 * \param tensor: Tensor field to evolve
 * \param dt: Timestep (seconds)
 */
void QFEA_tensor_field_step(QFEATensorField *tensor, float dt);

/**
 * Reset tensor field to initial state.
 */
void QFEA_tensor_field_reset(QFEATensorField *tensor);

/** \} */

/* -------------------------------------------------------------------- */
/** \name I/O
 * \{ */

/**
 * Save tensor field to file.
 *
 * \param tensor: Tensor field
 * \param filepath: Output file path
 * \param compression: Use compression (LZ4)
 * \return true if successful
 */
bool QFEA_tensor_field_save(const QFEATensorField *tensor,
                              const char *filepath,
                              bool compression);

/**
 * Load tensor field from file.
 */
QFEATensorField *QFEA_tensor_field_load(const char *filepath);

/**
 * Export channel to raw binary array.
 *
 * \param channel_index: Channel to export
 * \param filepath: Output path
 * \return true if successful
 */
bool QFEA_tensor_field_export_raw(const QFEATensorField *tensor,
                                   int channel_index,
                                   const char *filepath);

/**
 * Export channel to VTK format for ParaView/VisIt.
 */
bool QFEA_tensor_field_export_vtk(const QFEATensorField *tensor,
                                   int channel_index,
                                   const char *filepath);

/** \} */

#ifdef __cplusplus
}
#endif
