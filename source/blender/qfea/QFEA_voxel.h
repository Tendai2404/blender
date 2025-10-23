/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA voxel grid generation and management API.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BVHTree;
struct Mesh;
struct Object;

/* -------------------------------------------------------------------- */
/** \name Voxel Grid Creation
 * \{ */

/**
 * Create a new voxel grid.
 *
 * \param resolution: Voxel size in meters
 * \param dims: Grid dimensions [nx, ny, nz]
 * \param layout: Layout strategy (DENSE, SPARSE, OCTREE, BVH)
 * \return Newly allocated voxel grid
 */
QFEAVoxelGrid *QFEA_voxelgrid_new(float resolution, const int dims[3], eQFEAVoxelLayout layout);

/**
 * Free voxel grid and all associated data.
 */
void QFEA_voxelgrid_free(QFEAVoxelGrid *voxel_grid);

/**
 * Copy voxel grid.
 */
QFEAVoxelGrid *QFEA_voxelgrid_copy(const QFEAVoxelGrid *voxel_grid);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh to Voxel Conversion
 * \{ */

/**
 * Generate voxel grid from triangle mesh using BVH-accelerated solid fill.
 *
 * Algorithm:
 * 1. Build BVH acceleration structure from mesh triangles
 * 2. Initialize voxel grid based on mesh bounding box
 * 3. For each voxel: cast ray to determine inside/outside (even-odd test)
 * 4. Assign materials from closest mesh face
 * 5. Optionally apply adaptive refinement near surfaces
 *
 * \param mesh: Source triangle mesh
 * \param object: Object for transformation matrix
 * \param resolution: Voxel size in meters
 * \param layout: Storage layout (DENSE for small grids, SPARSE for hollow)
 * \param adaptive: Enable adaptive refinement
 * \param refinement_levels: Number of refinement levels (1-5 typical)
 * \return Voxelized representation
 */
QFEAVoxelGrid *QFEA_voxelgrid_from_mesh(const struct Mesh *mesh,
                                         const struct Object *object,
                                         float resolution,
                                         eQFEAVoxelLayout layout,
                                         bool adaptive,
                                         int refinement_levels);

/**
 * Generate voxels from mesh (simple wrapper for operator use).
 */
void QFEA_voxelgrid_generate_from_mesh(QFEAVoxelGrid *voxel_grid,
                                        const struct Mesh *mesh,
                                        const float obmat[4][4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Voxel Access
 * \{ */

/**
 * Get voxel material index at grid coordinates.
 *
 * \param i, j, k: Voxel indices
 * \return Material index (0 = empty, >0 = material ID)
 */
char QFEA_voxelgrid_get_material(const QFEAVoxelGrid *voxel_grid, int i, int j, int k);

/**
 * Set voxel material index.
 */
void QFEA_voxelgrid_set_material(QFEAVoxelGrid *voxel_grid, int i, int j, int k, char material_id);

/**
 * Get voxel occupancy (0=empty, 1=solid, 0-1=partial).
 */
float QFEA_voxelgrid_get_occupancy(const QFEAVoxelGrid *voxel_grid, int i, int j, int k);

/**
 * Set voxel occupancy.
 */
void QFEA_voxelgrid_set_occupancy(QFEAVoxelGrid *voxel_grid, int i, int j, int k, float occupancy);

/**
 * Convert voxel indices to world position.
 */
void QFEA_voxelgrid_index_to_position(const QFEAVoxelGrid *voxel_grid,
                                       int i,
                                       int j,
                                       int k,
                                       float r_pos[3]);

/**
 * Convert world position to voxel indices (with clamping).
 */
void QFEA_voxelgrid_position_to_index(const QFEAVoxelGrid *voxel_grid,
                                       const float pos[3],
                                       int r_indices[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Adaptive Refinement
 * \{ */

/**
 * Apply adaptive refinement based on surface proximity.
 *
 * Subdivides voxels near mesh surface into octree for higher resolution
 * while keeping coarse voxels in interior/exterior regions.
 *
 * \param voxel_grid: Grid to refine (must have OCTREE layout)
 * \param mesh: Reference mesh for surface detection
 * \param threshold_distance: Distance from surface to refine (in meters)
 * \param max_levels: Maximum octree depth
 */
void QFEA_voxelgrid_refine_adaptive(QFEAVoxelGrid *voxel_grid,
                                     const struct Mesh *mesh,
                                     float threshold_distance,
                                     int max_levels);

/**
 * Refine based on field gradients (for physics-driven adaptation).
 *
 * \param voxel_grid: Grid to refine
 * \param tensor_field: Field data for gradient computation
 * \param gradient_threshold: Refine where |∇field| > threshold
 */
void QFEA_voxelgrid_refine_by_gradient(QFEAVoxelGrid *voxel_grid,
                                        const struct QFEATensorField *tensor_field,
                                        float gradient_threshold);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Layout Conversion
 * \{ */

/**
 * Convert dense grid to sparse representation (if occupancy < 30%).
 *
 * Scans dense grid, extracts occupied voxels, builds spatial hash
 * for O(1) lookup. Frees dense array and switches to sparse layout.
 *
 * \return true if conversion successful (saves memory)
 */
bool QFEA_voxelgrid_dense_to_sparse(QFEAVoxelGrid *voxel_grid);

/**
 * Convert sparse grid to dense (for faster iteration).
 */
bool QFEA_voxelgrid_sparse_to_dense(QFEAVoxelGrid *voxel_grid);

/**
 * Convert to octree layout (enables adaptive refinement).
 */
bool QFEA_voxelgrid_to_octree(QFEAVoxelGrid *voxel_grid);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Statistics
 * \{ */

/**
 * Compute statistics about voxel grid.
 */
typedef struct QFEAVoxelStats {
  int total_voxels;
  int occupied_voxels;
  int empty_voxels;
  float occupancy_ratio;      /* occupied / total */
  size_t memory_bytes;
  int num_materials;
} QFEAVoxelStats;

void QFEA_voxelgrid_compute_stats(const QFEAVoxelGrid *voxel_grid, QFEAVoxelStats *r_stats);

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Synchronization
 * \{ */

/**
 * Upload voxel data to GPU for rendering.
 *
 * Creates vertex buffer with voxel positions and material indices.
 * Creates batch for instanced rendering.
 */
void QFEA_voxelgrid_sync_gpu(QFEAVoxelGrid *voxel_grid);

/**
 * Free GPU resources.
 */
void QFEA_voxelgrid_gpu_free(QFEAVoxelGrid *voxel_grid);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Compute bounding box from mesh.
 */
void QFEA_mesh_compute_bbox(const struct Mesh *mesh,
                             const float obmat[4][4],
                             float r_min[3],
                             float r_max[3]);

/**
 * Build BVH tree from mesh for fast ray-triangle intersection.
 */
struct BVHTree *QFEA_mesh_build_bvh(const struct Mesh *mesh);

/**
 * Ray-cast against voxel grid.
 *
 * \param origin: Ray origin
 * \param direction: Ray direction (normalized)
 * \param r_hit_index: Voxel indices of hit
 * \param r_hit_distance: Distance to hit
 * \return true if ray hit occupied voxel
 */
bool QFEA_voxelgrid_raycast(const QFEAVoxelGrid *voxel_grid,
                              const float origin[3],
                              const float direction[3],
                              int r_hit_index[3],
                              float *r_hit_distance);

/** \} */

#ifdef __cplusplus
}
#endif
