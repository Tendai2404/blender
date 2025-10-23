/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup qfea
 *
 * QFEA tensor field operations implementation.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_qfea_types.h"

#include "BKE_lib_id.hh"

#include "QFEA_tensor.h"
#include "QFEA_voxel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------- */
/** \name Helper Macros
 * \{ */

#define VOXEL_INDEX(tensor, i, j, k) \
  ((i) + (tensor)->voxel_grid->dims[0] * ((j) + (tensor)->voxel_grid->dims[1] * (k)))

#define CHANNEL_OFFSET(tensor, channel_idx, voxel_idx) \
  ((voxel_idx) * (tensor)->num_channels + (channel_idx))

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tensor Field Creation
 * \{ */

QFEATensorField *QFEA_tensor_field_new(QFEAVoxelGrid *voxel_grid,
                                        int num_channels,
                                        QFEATensorChannel *channels)
{
  if (!voxel_grid || num_channels <= 0) {
    return nullptr;
  }

  QFEATensorField *tensor = (QFEATensorField *)MEM_callocN(sizeof(QFEATensorField),
                                                             "QFEATensorField");

  BKE_lib_id_init(&tensor->id, ID_NT);

  tensor->voxel_grid = voxel_grid;
  tensor->num_channels = num_channels;

  /* Allocate channel definitions */
  tensor->channels = (QFEATensorChannel *)MEM_dupallocN(channels);

  /* Calculate total data size */
  const size_t total_voxels = (size_t)voxel_grid->dims[0] * voxel_grid->dims[1] *
                               voxel_grid->dims[2];

  size_t data_size = 0;
  for (int ch = 0; ch < num_channels; ch++) {
    size_t channel_elements = 1;

    switch (channels[ch].channel_type) {
      case QFEA_TENSOR_CHANNEL_SCALAR:
        channel_elements = 1;
        break;
      case QFEA_TENSOR_CHANNEL_VECTOR3:
        channel_elements = 3;
        break;
      case QFEA_TENSOR_CHANNEL_TENSOR33:
        channel_elements = 9;
        break;
    }

    switch (channels[ch].data_type) {
      case QFEA_TENSOR_DATA_FLOAT32:
        data_size += channel_elements * sizeof(float) * total_voxels;
        break;
      case QFEA_TENSOR_DATA_FLOAT64:
        data_size += channel_elements * sizeof(double) * total_voxels;
        break;
      case QFEA_TENSOR_DATA_COMPLEX64:
        data_size += channel_elements * sizeof(float) * 2 * total_voxels;
        break;
      case QFEA_TENSOR_DATA_COMPLEX128:
        data_size += channel_elements * sizeof(double) * 2 * total_voxels;
        break;
    }
  }

  /* Allocate data */
  tensor->data_cpu = MEM_callocN(data_size, "tensor field data");
  tensor->data_gpu = nullptr;

  return tensor;
}

void QFEA_tensor_field_free(QFEATensorField *tensor)
{
  if (!tensor) {
    return;
  }

  if (tensor->channels) {
    MEM_freeN(tensor->channels);
  }

  if (tensor->data_cpu) {
    MEM_freeN(tensor->data_cpu);
  }

  if (tensor->data_gpu) {
    /* GPU_texture_free(tensor->data_gpu); */
  }

  MEM_freeN(tensor);
}

QFEATensorField *QFEA_tensor_field_copy(const QFEATensorField *tensor)
{
  if (!tensor) {
    return nullptr;
  }

  QFEATensorField *copy = (QFEATensorField *)MEM_dupallocN(tensor);

  if (tensor->channels) {
    copy->channels = (QFEATensorChannel *)MEM_dupallocN(tensor->channels);
  }

  if (tensor->data_cpu) {
    /* Calculate data size */
    const size_t total_voxels = (size_t)tensor->voxel_grid->dims[0] *
                                 tensor->voxel_grid->dims[1] * tensor->voxel_grid->dims[2];
    const size_t data_size = total_voxels * tensor->num_channels * sizeof(float);
    copy->data_cpu = MEM_dupallocN(tensor->data_cpu);
  }

  copy->data_gpu = nullptr;  /* GPU resources not copied */

  return copy;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Access
 * \{ */

float QFEA_tensor_field_get_scalar(const QFEATensorField *tensor,
                                     int channel_index,
                                     int i,
                                     int j,
                                     int k)
{
  if (!tensor || channel_index < 0 || channel_index >= tensor->num_channels) {
    return 0.0f;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;

  if (i < 0 || i >= grid->dims[0] || j < 0 || j >= grid->dims[1] || k < 0 || k >= grid->dims[2])
  {
    return 0.0f;
  }

  const size_t voxel_idx = VOXEL_INDEX(tensor, i, j, k);
  const size_t offset = CHANNEL_OFFSET(tensor, channel_index, voxel_idx);

  const float *data = (const float *)tensor->data_cpu;
  return data[offset];
}

void QFEA_tensor_field_set_scalar(QFEATensorField *tensor,
                                   int channel_index,
                                   int i,
                                   int j,
                                   int k,
                                   float value)
{
  if (!tensor || channel_index < 0 || channel_index >= tensor->num_channels) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;

  if (i < 0 || i >= grid->dims[0] || j < 0 || j >= grid->dims[1] || k < 0 || k >= grid->dims[2])
  {
    return;
  }

  const size_t voxel_idx = VOXEL_INDEX(tensor, i, j, k);
  const size_t offset = CHANNEL_OFFSET(tensor, channel_index, voxel_idx);

  float *data = (float *)tensor->data_cpu;
  data[offset] = value;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Field Operations
 * \{ */

void QFEA_tensor_field_fill(QFEATensorField *tensor, int channel_index, float value)
{
  if (!tensor || channel_index < 0 || channel_index >= tensor->num_channels) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const size_t total_voxels = (size_t)grid->dims[0] * grid->dims[1] * grid->dims[2];

  float *data = (float *)tensor->data_cpu;

  for (size_t voxel_idx = 0; voxel_idx < total_voxels; voxel_idx++) {
    const size_t offset = CHANNEL_OFFSET(tensor, channel_index, voxel_idx);
    data[offset] = value;
  }
}

void QFEA_tensor_field_gradient(QFEATensorField *tensor,
                                 int scalar_channel,
                                 int grad_x_channel,
                                 int grad_y_channel,
                                 int grad_z_channel)
{
  if (!tensor) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float dx = grid->resolution;
  const float dy = grid->resolution;
  const float dz = grid->resolution;

  /* Central differences for interior voxels */
  for (int k = 1; k < grid->dims[2] - 1; k++) {
    for (int j = 1; j < grid->dims[1] - 1; j++) {
      for (int i = 1; i < grid->dims[0] - 1; i++) {
        /* ∂φ/∂x ≈ (φ(i+1) - φ(i-1)) / (2Δx) */
        const float phi_ip1 = QFEA_tensor_field_get_scalar(tensor, scalar_channel, i + 1, j, k);
        const float phi_im1 = QFEA_tensor_field_get_scalar(tensor, scalar_channel, i - 1, j, k);
        const float grad_x = (phi_ip1 - phi_im1) / (2.0f * dx);

        const float phi_jp1 = QFEA_tensor_field_get_scalar(tensor, scalar_channel, i, j + 1, k);
        const float phi_jm1 = QFEA_tensor_field_get_scalar(tensor, scalar_channel, i, j - 1, k);
        const float grad_y = (phi_jp1 - phi_jm1) / (2.0f * dy);

        const float phi_kp1 = QFEA_tensor_field_get_scalar(tensor, scalar_channel, i, j, k + 1);
        const float phi_km1 = QFEA_tensor_field_get_scalar(tensor, scalar_channel, i, j, k - 1);
        const float grad_z = (phi_kp1 - phi_km1) / (2.0f * dz);

        QFEA_tensor_field_set_scalar(tensor, grad_x_channel, i, j, k, grad_x);
        QFEA_tensor_field_set_scalar(tensor, grad_y_channel, i, j, k, grad_y);
        QFEA_tensor_field_set_scalar(tensor, grad_z_channel, i, j, k, grad_z);
      }
    }
  }
}

void QFEA_tensor_field_curl(QFEATensorField *tensor,
                             int vx_channel,
                             int vy_channel,
                             int vz_channel,
                             int curl_x_channel,
                             int curl_y_channel,
                             int curl_z_channel)
{
  if (!tensor) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float dx = grid->resolution;
  const float dy = grid->resolution;
  const float dz = grid->resolution;

  /* ∇×V = (∂Vz/∂y - ∂Vy/∂z, ∂Vx/∂z - ∂Vz/∂x, ∂Vy/∂x - ∂Vx/∂y) */

  for (int k = 1; k < grid->dims[2] - 1; k++) {
    for (int j = 1; j < grid->dims[1] - 1; j++) {
      for (int i = 1; i < grid->dims[0] - 1; i++) {
        /* ∂Vz/∂y */
        const float vz_jp1 = QFEA_tensor_field_get_scalar(tensor, vz_channel, i, j + 1, k);
        const float vz_jm1 = QFEA_tensor_field_get_scalar(tensor, vz_channel, i, j - 1, k);
        const float dVz_dy = (vz_jp1 - vz_jm1) / (2.0f * dy);

        /* ∂Vy/∂z */
        const float vy_kp1 = QFEA_tensor_field_get_scalar(tensor, vy_channel, i, j, k + 1);
        const float vy_km1 = QFEA_tensor_field_get_scalar(tensor, vy_channel, i, j, k - 1);
        const float dVy_dz = (vy_kp1 - vy_km1) / (2.0f * dz);

        /* curl_x = ∂Vz/∂y - ∂Vy/∂z */
        const float curl_x = dVz_dy - dVy_dz;

        /* ∂Vx/∂z */
        const float vx_kp1 = QFEA_tensor_field_get_scalar(tensor, vx_channel, i, j, k + 1);
        const float vx_km1 = QFEA_tensor_field_get_scalar(tensor, vx_channel, i, j, k - 1);
        const float dVx_dz = (vx_kp1 - vx_km1) / (2.0f * dz);

        /* ∂Vz/∂x */
        const float vz_ip1 = QFEA_tensor_field_get_scalar(tensor, vz_channel, i + 1, j, k);
        const float vz_im1 = QFEA_tensor_field_get_scalar(tensor, vz_channel, i - 1, j, k);
        const float dVz_dx = (vz_ip1 - vz_im1) / (2.0f * dx);

        /* curl_y = ∂Vx/∂z - ∂Vz/∂x */
        const float curl_y = dVx_dz - dVz_dx;

        /* ∂Vy/∂x */
        const float vy_ip1 = QFEA_tensor_field_get_scalar(tensor, vy_channel, i + 1, j, k);
        const float vy_im1 = QFEA_tensor_field_get_scalar(tensor, vy_channel, i - 1, j, k);
        const float dVy_dx = (vy_ip1 - vy_im1) / (2.0f * dx);

        /* ∂Vx/∂y */
        const float vx_jp1 = QFEA_tensor_field_get_scalar(tensor, vx_channel, i, j + 1, k);
        const float vx_jm1 = QFEA_tensor_field_get_scalar(tensor, vx_channel, i, j - 1, k);
        const float dVx_dy = (vx_jp1 - vx_jm1) / (2.0f * dy);

        /* curl_z = ∂Vy/∂x - ∂Vx/∂y */
        const float curl_z = dVy_dx - dVx_dy;

        QFEA_tensor_field_set_scalar(tensor, curl_x_channel, i, j, k, curl_x);
        QFEA_tensor_field_set_scalar(tensor, curl_y_channel, i, j, k, curl_y);
        QFEA_tensor_field_set_scalar(tensor, curl_z_channel, i, j, k, curl_z);
      }
    }
  }
}

void QFEA_tensor_field_divergence(QFEATensorField *tensor,
                                   int vx_channel,
                                   int vy_channel,
                                   int vz_channel,
                                   int div_channel)
{
  if (!tensor) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float dx = grid->resolution;
  const float dy = grid->resolution;
  const float dz = grid->resolution;

  /* ∇·V = ∂Vx/∂x + ∂Vy/∂y + ∂Vz/∂z */

  for (int k = 1; k < grid->dims[2] - 1; k++) {
    for (int j = 1; j < grid->dims[1] - 1; j++) {
      for (int i = 1; i < grid->dims[0] - 1; i++) {
        const float vx_ip1 = QFEA_tensor_field_get_scalar(tensor, vx_channel, i + 1, j, k);
        const float vx_im1 = QFEA_tensor_field_get_scalar(tensor, vx_channel, i - 1, j, k);
        const float dVx_dx = (vx_ip1 - vx_im1) / (2.0f * dx);

        const float vy_jp1 = QFEA_tensor_field_get_scalar(tensor, vy_channel, i, j + 1, k);
        const float vy_jm1 = QFEA_tensor_field_get_scalar(tensor, vy_channel, i, j - 1, k);
        const float dVy_dy = (vy_jp1 - vy_jm1) / (2.0f * dy);

        const float vz_kp1 = QFEA_tensor_field_get_scalar(tensor, vz_channel, i, j, k + 1);
        const float vz_km1 = QFEA_tensor_field_get_scalar(tensor, vz_channel, i, j, k - 1);
        const float dVz_dz = (vz_kp1 - vz_km1) / (2.0f * dz);

        const float div = dVx_dx + dVy_dy + dVz_dz;

        QFEA_tensor_field_set_scalar(tensor, div_channel, i, j, k, div);
      }
    }
  }
}

void QFEA_tensor_field_laplacian(QFEATensorField *tensor, int src_channel, int dst_channel)
{
  if (!tensor) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float dx = grid->resolution;
  const float dy = grid->resolution;
  const float dz = grid->resolution;

  /* 7-point stencil Laplacian: ∇²φ = (∂²φ/∂x² + ∂²φ/∂y² + ∂²φ/∂z²) */

  for (int k = 1; k < grid->dims[2] - 1; k++) {
    for (int j = 1; j < grid->dims[1] - 1; j++) {
      for (int i = 1; i < grid->dims[0] - 1; i++) {
        const float phi_c = QFEA_tensor_field_get_scalar(tensor, src_channel, i, j, k);

        const float phi_ip1 = QFEA_tensor_field_get_scalar(tensor, src_channel, i + 1, j, k);
        const float phi_im1 = QFEA_tensor_field_get_scalar(tensor, src_channel, i - 1, j, k);
        const float d2_dx2 = (phi_ip1 - 2.0f * phi_c + phi_im1) / (dx * dx);

        const float phi_jp1 = QFEA_tensor_field_get_scalar(tensor, src_channel, i, j + 1, k);
        const float phi_jm1 = QFEA_tensor_field_get_scalar(tensor, src_channel, i, j - 1, k);
        const float d2_dy2 = (phi_jp1 - 2.0f * phi_c + phi_jm1) / (dy * dy);

        const float phi_kp1 = QFEA_tensor_field_get_scalar(tensor, src_channel, i, j, k + 1);
        const float phi_km1 = QFEA_tensor_field_get_scalar(tensor, src_channel, i, j, k - 1);
        const float d2_dz2 = (phi_kp1 - 2.0f * phi_c + phi_km1) / (dz * dz);

        const float laplacian = d2_dx2 + d2_dy2 + d2_dz2;

        QFEA_tensor_field_set_scalar(tensor, dst_channel, i, j, k, laplacian);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Analysis
 * \{ */

void QFEA_tensor_field_compute_stats(const QFEATensorField *tensor,
                                      int channel_index,
                                      QFEATensorStats *r_stats)
{
  if (!tensor || !r_stats || channel_index < 0 || channel_index >= tensor->num_channels) {
    return;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;

  float min_val = FLT_MAX;
  float max_val = -FLT_MAX;
  double sum = 0.0;
  double sum_sq = 0.0;
  int num_samples = 0;

  for (int k = 0; k < grid->dims[2]; k++) {
    for (int j = 0; j < grid->dims[1]; j++) {
      for (int i = 0; i < grid->dims[0]; i++) {
        const float value = QFEA_tensor_field_get_scalar(tensor, channel_index, i, j, k);

        min_val = fminf(min_val, value);
        max_val = fmaxf(max_val, value);
        sum += value;
        sum_sq += value * value;
        num_samples++;
      }
    }
  }

  r_stats->min = min_val;
  r_stats->max = max_val;
  r_stats->num_samples = num_samples;

  if (num_samples > 0) {
    r_stats->mean = (float)(sum / num_samples);
    r_stats->rms = (float)sqrt(sum_sq / num_samples);

    const double variance = (sum_sq / num_samples) - (r_stats->mean * r_stats->mean);
    r_stats->std_dev = (float)sqrt(fmax(variance, 0.0));
  }
}

float QFEA_tensor_field_compute_electric_energy(const QFEATensorField *tensor,
                                                 int ex_channel,
                                                 int ey_channel,
                                                 int ez_channel,
                                                 const float *epsilon)
{
  if (!tensor) {
    return 0.0f;
  }

  const QFEAVoxelGrid *grid = tensor->voxel_grid;
  const float voxel_volume = grid->resolution * grid->resolution * grid->resolution;
  const float epsilon_0 = 8.854187817e-12f; /* F/m */

  double total_energy = 0.0;

  for (int k = 0; k < grid->dims[2]; k++) {
    for (int j = 0; j < grid->dims[1]; j++) {
      for (int i = 0; i < grid->dims[0]; i++) {
        const float ex = QFEA_tensor_field_get_scalar(tensor, ex_channel, i, j, k);
        const float ey = QFEA_tensor_field_get_scalar(tensor, ey_channel, i, j, k);
        const float ez = QFEA_tensor_field_get_scalar(tensor, ez_channel, i, j, k);

        const float e_squared = ex * ex + ey * ey + ez * ez;

        /* E = 0.5 * ε₀ * εᵣ * |E|² * V */
        const size_t voxel_idx = VOXEL_INDEX(tensor, i, j, k);
        const float eps_r = epsilon ? epsilon[voxel_idx] : 1.0f;

        total_energy += 0.5 * epsilon_0 * eps_r * e_squared * voxel_volume;
      }
    }
  }

  return (float)total_energy;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Synchronization
 * \{ */

void QFEA_tensor_field_sync_gpu(QFEATensorField *tensor, int channel_index)
{
  if (!tensor) {
    return;
  }

  /* Upload tensor field data to GPU as 3D texture */
  /* Implementation would use GPU_texture_create_3d() with channel data */
}

void QFEA_tensor_field_gpu_free(QFEATensorField *tensor)
{
  if (!tensor || !tensor->data_gpu) {
    return;
  }

  /* GPU_texture_free(tensor->data_gpu); */
  tensor->data_gpu = nullptr;
}

/** \} */
