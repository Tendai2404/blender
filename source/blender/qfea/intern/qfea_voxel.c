/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup qfea
 *
 * QFEA voxel grid operations implementation.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_qfea_types.h"

#include "BKE_bvhutils.hh"
#include "BKE_customdata.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"

#include "QFEA_voxel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------- */
/** \name Voxel Grid Creation
 * \{ */

QFEAVoxelGrid *QFEA_voxelgrid_new(const float origin[3],
                                   const int dims[3],
                                   float resolution,
                                   eQFEAVoxelLayout layout)
{
  QFEAVoxelGrid *voxel_grid = (QFEAVoxelGrid *)MEM_callocN(sizeof(QFEAVoxelGrid),
                                                             "QFEAVoxelGrid");

  /* Initialize ID system */
  BKE_lib_id_init(&voxel_grid->id, ID_NT);

  copy_v3_v3(voxel_grid->origin, origin);
  copy_v3_v3_int(voxel_grid->dims, dims);
  voxel_grid->resolution = resolution;
  voxel_grid->layout_type = layout;

  /* Calculate bounds */
  voxel_grid->bounds_min[0] = origin[0];
  voxel_grid->bounds_min[1] = origin[1];
  voxel_grid->bounds_min[2] = origin[2];

  voxel_grid->bounds_max[0] = origin[0] + dims[0] * resolution;
  voxel_grid->bounds_max[1] = origin[1] + dims[1] * resolution;
  voxel_grid->bounds_max[2] = origin[2] + dims[2] * resolution;

  /* Allocate storage based on layout */
  const size_t total_voxels = (size_t)dims[0] * dims[1] * dims[2];

  switch (layout) {
    case QFEA_VOXEL_LAYOUT_DENSE:
      voxel_grid->occupancy_data = (float *)MEM_callocN(sizeof(float) * total_voxels,
                                                          "voxel occupancy");
      break;

    case QFEA_VOXEL_LAYOUT_SPARSE:
      /* Sparse storage allocated on demand */
      voxel_grid->sparse_indices = nullptr;
      voxel_grid->occupancy_data = nullptr;
      voxel_grid->num_sparse_voxels = 0;
      break;

    case QFEA_VOXEL_LAYOUT_OCTREE:
      /* Octree root allocated on demand */
      voxel_grid->octree_root = nullptr;
      voxel_grid->octree_max_depth = 0;
      break;

    case QFEA_VOXEL_LAYOUT_BVH:
      /* BVH built on demand */
      voxel_grid->bvh = nullptr;
      break;
  }

  return voxel_grid;
}

void QFEA_voxelgrid_free(QFEAVoxelGrid *voxel_grid)
{
  if (!voxel_grid) {
    return;
  }

  /* Free layout-specific data */
  if (voxel_grid->occupancy_data) {
    MEM_freeN(voxel_grid->occupancy_data);
  }

  if (voxel_grid->sparse_indices) {
    MEM_freeN(voxel_grid->sparse_indices);
  }

  if (voxel_grid->octree_root) {
    /* Recursive octree free would go here */
    MEM_freeN(voxel_grid->octree_root);
  }

  if (voxel_grid->bvh) {
    /* BVH_tree_free(voxel_grid->bvh); */
    voxel_grid->bvh = nullptr;
  }

  /* Free GPU resources */
  if (voxel_grid->gpu_buffer) {
    /* GPU_vertbuf_discard(voxel_grid->gpu_buffer); */
    voxel_grid->gpu_buffer = nullptr;
  }

  if (voxel_grid->gpu_batch) {
    /* GPU_batch_discard(voxel_grid->gpu_batch); */
    voxel_grid->gpu_batch = nullptr;
  }

  MEM_freeN(voxel_grid);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh to Voxel Conversion
 * \{ */

/**
 * Helper: Check if ray intersects voxel.
 */
static bool voxel_intersects_triangle(const float voxel_min[3],
                                       const float voxel_max[3],
                                       const float v0[3],
                                       const float v1[3],
                                       const float v2[3])
{
  /* Simple AABB-triangle intersection test */
  /* This is a placeholder - production code would use Separating Axis Theorem */

  float tri_min[3], tri_max[3];

  /* Compute triangle AABB */
  for (int i = 0; i < 3; i++) {
    tri_min[i] = fminf(fminf(v0[i], v1[i]), v2[i]);
    tri_max[i] = fmaxf(fmaxf(v0[i], v1[i]), v2[i]);
  }

  /* AABB overlap test */
  for (int i = 0; i < 3; i++) {
    if (tri_max[i] < voxel_min[i] || tri_min[i] > voxel_max[i]) {
      return false;
    }
  }

  return true;
}

QFEAVoxelGrid *QFEA_voxelgrid_from_mesh(const Mesh *mesh,
                                         const Object *object,
                                         float resolution,
                                         eQFEAVoxelLayout layout,
                                         bool adaptive,
                                         int refinement_levels)
{
  if (!mesh || mesh->verts_num == 0) {
    return nullptr;
  }

  /* Calculate mesh bounds in world space */
  float bounds_min[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
  float bounds_max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

  const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
  const float(*object_matrix)[4] = object ? object->object_to_world().ptr() : nullptr;

  for (int i = 0; i < mesh->verts_num; i++) {
    float world_pos[3];

    if (object_matrix) {
      mul_v3_m4v3(world_pos, object_matrix, positions[i]);
    }
    else {
      copy_v3_v3(world_pos, positions[i]);
    }

    minmax_v3v3_v3(bounds_min, bounds_max, world_pos);
  }

  /* Add padding */
  const float padding = resolution * 2.0f;
  for (int i = 0; i < 3; i++) {
    bounds_min[i] -= padding;
    bounds_max[i] += padding;
  }

  /* Calculate grid dimensions */
  int dims[3];
  for (int i = 0; i < 3; i++) {
    dims[i] = (int)ceilf((bounds_max[i] - bounds_min[i]) / resolution);
    dims[i] = max_ii(dims[i], 1);
  }

  /* Create voxel grid */
  QFEAVoxelGrid *voxel_grid = QFEA_voxelgrid_new(bounds_min, dims, resolution, layout);

  /* Build BVH for mesh */
  BVHTree *bvh = nullptr;
  BVHTreeFromMesh tree_data = {{nullptr}};

  /* BKE_bvhtree_from_mesh_get(&tree_data, mesh, BVHTREE_FROM_LOOPTRIS, 2); */
  /* bvh = tree_data.tree; */

  /* Voxelize using BVH-accelerated ray casting */
  const size_t total_voxels = (size_t)dims[0] * dims[1] * dims[2];

  if (layout == QFEA_VOXEL_LAYOUT_DENSE) {
    /* Dense voxelization */
    for (int k = 0; k < dims[2]; k++) {
      for (int j = 0; j < dims[1]; j++) {
        for (int i = 0; i < dims[0]; i++) {
          float voxel_center[3];
          voxel_center[0] = bounds_min[0] + (i + 0.5f) * resolution;
          voxel_center[1] = bounds_min[1] + (j + 0.5f) * resolution;
          voxel_center[2] = bounds_min[2] + (k + 0.5f) * resolution;

          /* Check if voxel center is inside mesh */
          /* This would use BVH ray casting in production */
          const size_t voxel_index = i + dims[0] * (j + dims[1] * k);

          /* Placeholder: mark as occupied if within bounds */
          voxel_grid->occupancy_data[voxel_index] = 0.0f;
        }
      }
    }
  }

  /* Adaptive refinement if requested */
  if (adaptive && refinement_levels > 0) {
    QFEA_voxelgrid_refine_adaptive(voxel_grid, mesh, resolution * 0.5f, refinement_levels);
  }

  return voxel_grid;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Adaptive Refinement
 * \{ */

void QFEA_voxelgrid_refine_adaptive(QFEAVoxelGrid *voxel_grid,
                                     const Mesh *mesh,
                                     float threshold_distance,
                                     int max_levels)
{
  if (!voxel_grid || !mesh) {
    return;
  }

  /* Convert to octree if not already */
  if (voxel_grid->layout_type != QFEA_VOXEL_LAYOUT_OCTREE) {
    QFEA_voxelgrid_dense_to_octree(voxel_grid);
  }

  /* Refine octree nodes near mesh surface */
  /* Implementation would recursively subdivide octree nodes
   * that are close to mesh surface based on threshold_distance */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Format Conversion
 * \{ */

bool QFEA_voxelgrid_dense_to_sparse(QFEAVoxelGrid *voxel_grid)
{
  if (!voxel_grid || voxel_grid->layout_type != QFEA_VOXEL_LAYOUT_DENSE) {
    return false;
  }

  const size_t total_voxels = (size_t)voxel_grid->dims[0] * voxel_grid->dims[1] *
                               voxel_grid->dims[2];

  /* Count non-zero voxels */
  size_t num_occupied = 0;
  for (size_t i = 0; i < total_voxels; i++) {
    if (voxel_grid->occupancy_data[i] > 0.0f) {
      num_occupied++;
    }
  }

  if (num_occupied == 0) {
    return false;
  }

  /* Allocate sparse storage */
  int *sparse_indices = (int *)MEM_mallocN(sizeof(int) * num_occupied, "sparse voxel indices");
  float *sparse_data = (float *)MEM_mallocN(sizeof(float) * num_occupied, "sparse voxel data");

  /* Copy occupied voxels */
  size_t sparse_index = 0;
  for (size_t i = 0; i < total_voxels; i++) {
    if (voxel_grid->occupancy_data[i] > 0.0f) {
      sparse_indices[sparse_index] = (int)i;
      sparse_data[sparse_index] = voxel_grid->occupancy_data[i];
      sparse_index++;
    }
  }

  /* Free dense storage */
  MEM_freeN(voxel_grid->occupancy_data);

  /* Update to sparse layout */
  voxel_grid->layout_type = QFEA_VOXEL_LAYOUT_SPARSE;
  voxel_grid->sparse_indices = sparse_indices;
  voxel_grid->occupancy_data = sparse_data;
  voxel_grid->num_sparse_voxels = (int)num_occupied;

  return true;
}

bool QFEA_voxelgrid_sparse_to_dense(QFEAVoxelGrid *voxel_grid)
{
  if (!voxel_grid || voxel_grid->layout_type != QFEA_VOXEL_LAYOUT_SPARSE) {
    return false;
  }

  const size_t total_voxels = (size_t)voxel_grid->dims[0] * voxel_grid->dims[1] *
                               voxel_grid->dims[2];

  /* Allocate dense storage */
  float *dense_data = (float *)MEM_callocN(sizeof(float) * total_voxels, "dense voxel data");

  /* Scatter sparse voxels into dense array */
  for (int i = 0; i < voxel_grid->num_sparse_voxels; i++) {
    const int voxel_index = voxel_grid->sparse_indices[i];
    dense_data[voxel_index] = voxel_grid->occupancy_data[i];
  }

  /* Free sparse storage */
  MEM_freeN(voxel_grid->sparse_indices);
  MEM_freeN(voxel_grid->occupancy_data);

  /* Update to dense layout */
  voxel_grid->layout_type = QFEA_VOXEL_LAYOUT_DENSE;
  voxel_grid->sparse_indices = nullptr;
  voxel_grid->occupancy_data = dense_data;
  voxel_grid->num_sparse_voxels = 0;

  return true;
}

bool QFEA_voxelgrid_dense_to_octree(QFEAVoxelGrid *voxel_grid)
{
  if (!voxel_grid || voxel_grid->layout_type != QFEA_VOXEL_LAYOUT_DENSE) {
    return false;
  }

  /* Build octree from dense data */
  /* Implementation would recursively build octree structure */

  voxel_grid->layout_type = QFEA_VOXEL_LAYOUT_OCTREE;
  return true;
}

bool QFEA_voxelgrid_octree_to_dense(QFEAVoxelGrid *voxel_grid)
{
  if (!voxel_grid || voxel_grid->layout_type != QFEA_VOXEL_LAYOUT_OCTREE) {
    return false;
  }

  /* Flatten octree to dense array */
  /* Implementation would traverse octree and write to dense array */

  voxel_grid->layout_type = QFEA_VOXEL_LAYOUT_DENSE;
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Voxel Access
 * \{ */

float QFEA_voxelgrid_get_occupancy(const QFEAVoxelGrid *voxel_grid, int i, int j, int k)
{
  if (!voxel_grid) {
    return 0.0f;
  }

  /* Bounds check */
  if (i < 0 || i >= voxel_grid->dims[0] || j < 0 || j >= voxel_grid->dims[1] || k < 0 ||
      k >= voxel_grid->dims[2])
  {
    return 0.0f;
  }

  const size_t voxel_index = i + voxel_grid->dims[0] * (j + voxel_grid->dims[1] * k);

  switch (voxel_grid->layout_type) {
    case QFEA_VOXEL_LAYOUT_DENSE:
      return voxel_grid->occupancy_data[voxel_index];

    case QFEA_VOXEL_LAYOUT_SPARSE: {
      /* Binary search in sparse indices */
      for (int idx = 0; idx < voxel_grid->num_sparse_voxels; idx++) {
        if (voxel_grid->sparse_indices[idx] == (int)voxel_index) {
          return voxel_grid->occupancy_data[idx];
        }
      }
      return 0.0f;
    }

    case QFEA_VOXEL_LAYOUT_OCTREE:
      /* Octree lookup would go here */
      return 0.0f;

    case QFEA_VOXEL_LAYOUT_BVH:
      /* BVH lookup would go here */
      return 0.0f;
  }

  return 0.0f;
}

void QFEA_voxelgrid_set_occupancy(QFEAVoxelGrid *voxel_grid, int i, int j, int k, float value)
{
  if (!voxel_grid) {
    return;
  }

  /* Bounds check */
  if (i < 0 || i >= voxel_grid->dims[0] || j < 0 || j >= voxel_grid->dims[1] || k < 0 ||
      k >= voxel_grid->dims[2])
  {
    return;
  }

  const size_t voxel_index = i + voxel_grid->dims[0] * (j + voxel_grid->dims[1] * k);

  switch (voxel_grid->layout_type) {
    case QFEA_VOXEL_LAYOUT_DENSE:
      voxel_grid->occupancy_data[voxel_index] = value;
      break;

    case QFEA_VOXEL_LAYOUT_SPARSE:
      /* Sparse write would require reallocation */
      break;

    case QFEA_VOXEL_LAYOUT_OCTREE:
      /* Octree write would go here */
      break;

    case QFEA_VOXEL_LAYOUT_BVH:
      /* BVH is read-only */
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Synchronization
 * \{ */

void QFEA_voxelgrid_sync_gpu(QFEAVoxelGrid *voxel_grid)
{
  if (!voxel_grid) {
    return;
  }

  /* Upload voxel data to GPU as 3D texture */
  /* Implementation would use GPU_texture_create_3d() */
}

void QFEA_voxelgrid_gpu_free(QFEAVoxelGrid *voxel_grid)
{
  if (!voxel_grid) {
    return;
  }

  if (voxel_grid->gpu_buffer) {
    /* GPU_vertbuf_discard(voxel_grid->gpu_buffer); */
    voxel_grid->gpu_buffer = nullptr;
  }

  if (voxel_grid->gpu_batch) {
    /* GPU_batch_discard(voxel_grid->gpu_batch); */
    voxel_grid->gpu_batch = nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory Statistics
 * \{ */

size_t QFEA_voxelgrid_get_memory_usage(const QFEAVoxelGrid *voxel_grid)
{
  if (!voxel_grid) {
    return 0;
  }

  size_t total_bytes = sizeof(QFEAVoxelGrid);

  const size_t total_voxels = (size_t)voxel_grid->dims[0] * voxel_grid->dims[1] *
                               voxel_grid->dims[2];

  switch (voxel_grid->layout_type) {
    case QFEA_VOXEL_LAYOUT_DENSE:
      total_bytes += sizeof(float) * total_voxels;
      break;

    case QFEA_VOXEL_LAYOUT_SPARSE:
      total_bytes += sizeof(int) * voxel_grid->num_sparse_voxels;
      total_bytes += sizeof(float) * voxel_grid->num_sparse_voxels;
      break;

    case QFEA_VOXEL_LAYOUT_OCTREE:
      /* Octree memory calculation would go here */
      break;

    case QFEA_VOXEL_LAYOUT_BVH:
      /* BVH memory calculation would go here */
      break;
  }

  return total_bytes;
}

/** \} */
