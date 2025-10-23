/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA custom draw engine for real-time visualization of tensor fields.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DRWData;
struct GPUShader;
struct GPUTexture;
struct GPUBatch;
struct View3D;
struct RegionView3D;

/* -------------------------------------------------------------------- */
/** \name Draw Engine Registration
 * \{ */

/**
 * Register QFEA draw engine.
 *
 * Called during Blender startup to register custom draw engine
 * for QFEA visualization.
 */
void QFEA_draw_engine_register(void);

/**
 * Unregister QFEA draw engine.
 */
void QFEA_draw_engine_unregister(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visualization Modes
 * \{ */

/**
 * Visualization mode for tensor fields.
 */
typedef enum eQFEADrawMode {
  QFEA_DRAW_VOLUME = 0,        /* Volume rendering */
  QFEA_DRAW_SLICE = 1,         /* 2D slice */
  QFEA_DRAW_ISOSURFACE = 2,    /* Isosurface extraction */
  QFEA_DRAW_VECTORS = 3,       /* Vector field arrows */
  QFEA_DRAW_STREAMLINES = 4,   /* Streamlines/pathlines */
  QFEA_DRAW_PARTICLES = 5,     /* Particle visualization */
  QFEA_DRAW_VOXELS = 6,        /* Voxel grid visualization */
  QFEA_DRAW_ATOMS = 7,         /* Atomic structure */
} eQFEADrawMode;

/**
 * Draw settings.
 */
typedef struct QFEADrawSettings {
  eQFEADrawMode mode;

  /* Volume rendering */
  float density;               /* Volume density multiplier */
  float brightness;            /* Brightness adjustment */
  int num_samples;             /* Ray marching samples */
  char use_lighting;           /* Apply lighting to volume */

  /* Slice mode */
  int slice_axis;              /* 0=X, 1=Y, 2=Z */
  float slice_position;        /* Normalized position (0-1) */
  char slice_interpolation;    /* Interpolation mode */

  /* Isosurface */
  float isovalue;              /* Isosurface threshold */
  char smooth_normals;         /* Smooth shading */

  /* Vector field */
  float vector_scale;          /* Arrow scale factor */
  int vector_subsample;        /* Draw every Nth vector */
  char vector_color_mode;      /* Color by magnitude/direction */

  /* Streamlines */
  int num_streamlines;         /* Number of streamlines */
  float streamline_step;       /* Integration step size */
  int streamline_max_steps;    /* Maximum steps per streamline */
  int streamline_seed;         /* Random seed for placement */

  /* Colormap */
  char colormap_name[64];      /* "Viridis", "Plasma", "Jet", etc. */
  float colormap_min;          /* Minimum value for mapping */
  float colormap_max;          /* Maximum value for mapping */
  char colormap_autoscale;     /* Auto-scale to data range */
  char colormap_logarithmic;   /* Logarithmic mapping */

  /* Clipping */
  char use_clipping;
  float clip_min[3];           /* Minimum clip bounds */
  float clip_max[3];           /* Maximum clip bounds */

  /* Quality */
  int quality;                 /* 0=preview, 1=normal, 2=high */

  /* GPU cache */
  char gpu_cache_dirty;        /* Needs GPU upload */
} QFEADrawSettings;

/**
 * Create default draw settings.
 */
QFEADrawSettings *QFEA_draw_settings_new(void);

/**
 * Free draw settings.
 */
void QFEA_draw_settings_free(QFEADrawSettings *settings);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Rendering
 * \{ */

/**
 * Draw tensor field as volume.
 *
 * Uses GPU ray marching for real-time volume rendering.
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to visualize
 * \param settings: Draw settings
 * \param view3d: 3D viewport
 * \param rv3d: Region view data
 */
void QFEA_draw_volume(const QFEATensorField *tensor,
                       int channel_index,
                       const QFEADrawSettings *settings,
                       const struct View3D *view3d,
                       const struct RegionView3D *rv3d);

/**
 * Generate volume shader.
 *
 * Creates GPU shader for volume rendering with current colormap.
 *
 * \param settings: Draw settings
 * \return GPU shader
 */
struct GPUShader *QFEA_draw_volume_shader_get(const QFEADrawSettings *settings);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Slice Rendering
 * \{ */

/**
 * Draw 2D slice through tensor field.
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to visualize
 * \param settings: Draw settings
 */
void QFEA_draw_slice(const QFEATensorField *tensor,
                      int channel_index,
                      const QFEADrawSettings *settings);

/**
 * Draw multi-slice (XYZ planes simultaneously).
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to visualize
 * \param settings: Draw settings
 * \param x_pos, y_pos, z_pos: Slice positions (normalized 0-1)
 */
void QFEA_draw_multi_slice(const QFEATensorField *tensor,
                             int channel_index,
                             const QFEADrawSettings *settings,
                             float x_pos,
                             float y_pos,
                             float z_pos);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Isosurface Rendering
 * \{ */

/**
 * Draw isosurface.
 *
 * Uses marching cubes algorithm to extract surface.
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to visualize
 * \param settings: Draw settings
 */
void QFEA_draw_isosurface(const QFEATensorField *tensor,
                           int channel_index,
                           const QFEADrawSettings *settings);

/**
 * Extract isosurface mesh.
 *
 * Generates triangle mesh for isosurface.
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to extract
 * \param isovalue: Threshold value
 * \param r_vertices: Output vertices (allocated by function)
 * \param r_normals: Output normals (allocated by function)
 * \param r_indices: Output triangle indices (allocated by function)
 * \param r_num_vertices: Number of vertices
 * \param r_num_triangles: Number of triangles
 */
void QFEA_draw_extract_isosurface(const QFEATensorField *tensor,
                                    int channel_index,
                                    float isovalue,
                                    float (**r_vertices)[3],
                                    float (**r_normals)[3],
                                    int (**r_indices)[3],
                                    int *r_num_vertices,
                                    int *r_num_triangles);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector Field Rendering
 * \{ */

/**
 * Draw vector field as arrows.
 *
 * \param tensor: Tensor field
 * \param vx_channel, vy_channel, vz_channel: Vector component channels
 * \param settings: Draw settings
 */
void QFEA_draw_vector_field(const QFEATensorField *tensor,
                              int vx_channel,
                              int vy_channel,
                              int vz_channel,
                              const QFEADrawSettings *settings);

/**
 * Draw vector field as cones (better for EM fields).
 *
 * \param tensor: Tensor field
 * \param vx_channel, vy_channel, vz_channel: Vector component channels
 * \param settings: Draw settings
 */
void QFEA_draw_vector_field_cones(const QFEATensorField *tensor,
                                    int vx_channel,
                                    int vy_channel,
                                    int vz_channel,
                                    const QFEADrawSettings *settings);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Streamline Rendering
 * \{ */

/**
 * Draw streamlines through vector field.
 *
 * Integrates field lines using RK4 integration.
 *
 * \param tensor: Tensor field
 * \param vx_channel, vy_channel, vz_channel: Vector component channels
 * \param settings: Draw settings
 */
void QFEA_draw_streamlines(const QFEATensorField *tensor,
                             int vx_channel,
                             int vy_channel,
                             int vz_channel,
                             const QFEADrawSettings *settings);

/**
 * Generate streamline from seed point.
 *
 * \param tensor: Tensor field
 * \param vx_channel, vy_channel, vz_channel: Vector components
 * \param seed_position: Starting position
 * \param step_size: Integration step
 * \param max_steps: Maximum steps
 * \param r_positions: Output positions (allocated by function)
 * \param r_num_points: Number of points
 */
void QFEA_draw_generate_streamline(const QFEATensorField *tensor,
                                      int vx_channel,
                                      int vy_channel,
                                      int vz_channel,
                                      const float seed_position[3],
                                      float step_size,
                                      int max_steps,
                                      float (**r_positions)[3],
                                      int *r_num_points);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle Rendering
 * \{ */

/**
 * Draw particle system.
 *
 * \param particle_system: Particle system
 * \param settings: Draw settings
 */
void QFEA_draw_particles(const QFEAParticleSystem *particle_system,
                          const QFEADrawSettings *settings);

/**
 * Draw particle trajectories.
 *
 * \param particle_system: Particle system
 * \param trajectory_length: Number of history points to draw
 * \param settings: Draw settings
 */
void QFEA_draw_particle_trajectories(const QFEAParticleSystem *particle_system,
                                       int trajectory_length,
                                       const QFEADrawSettings *settings);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Voxel Grid Rendering
 * \{ */

/**
 * Draw voxel grid wireframe.
 *
 * \param voxel_grid: Voxel grid
 * \param settings: Draw settings
 */
void QFEA_draw_voxel_grid_wireframe(const QFEAVoxelGrid *voxel_grid,
                                      const QFEADrawSettings *settings);

/**
 * Draw voxel grid bounds.
 *
 * \param voxel_grid: Voxel grid
 */
void QFEA_draw_voxel_grid_bounds(const QFEAVoxelGrid *voxel_grid);

/**
 * Draw octree structure.
 *
 * Visualizes octree node hierarchy.
 *
 * \param voxel_grid: Voxel grid (must use octree layout)
 * \param max_depth: Maximum depth to draw (-1 for all)
 */
void QFEA_draw_octree_structure(const QFEAVoxelGrid *voxel_grid, int max_depth);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Atomic Structure Rendering
 * \{ */

/**
 * Draw atomic structure.
 *
 * Renders atoms as spheres and bonds as cylinders.
 *
 * \param atomic_structure: Atomic structure
 * \param repeat_x, repeat_y, repeat_z: Unit cell repetitions
 * \param settings: Draw settings
 */
void QFEA_draw_atomic_structure(const QFEAAtomicStructure *atomic_structure,
                                  int repeat_x,
                                  int repeat_y,
                                  int repeat_z,
                                  const QFEADrawSettings *settings);

/**
 * Draw unit cell lattice.
 *
 * \param atomic_structure: Atomic structure
 */
void QFEA_draw_unit_cell(const QFEAAtomicStructure *atomic_structure);

/**
 * Draw atoms with CPK coloring.
 *
 * \param positions: Atom positions
 * \param elements: Element IDs
 * \param count: Number of atoms
 * \param atom_scale: Scale factor for atom spheres
 */
void QFEA_draw_atoms_cpk(const float (*positions)[3],
                          const int *elements,
                          int count,
                          float atom_scale);

/**
 * Draw bonds between atoms.
 *
 * \param positions: Atom positions
 * \param bonds: Bond pairs (atom indices)
 * \param num_bonds: Number of bonds
 * \param bond_radius: Cylinder radius
 */
void QFEA_draw_bonds(const float (*positions)[3],
                      const int (*bonds)[2],
                      int num_bonds,
                      float bond_radius);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Colormaps
 * \{ */

/**
 * Built-in colormap types.
 */
typedef enum eQFEAColormap {
  QFEA_COLORMAP_VIRIDIS = 0,
  QFEA_COLORMAP_PLASMA = 1,
  QFEA_COLORMAP_INFERNO = 2,
  QFEA_COLORMAP_MAGMA = 3,
  QFEA_COLORMAP_TURBO = 4,
  QFEA_COLORMAP_JET = 5,
  QFEA_COLORMAP_HOT = 6,
  QFEA_COLORMAP_COOL = 7,
  QFEA_COLORMAP_RAINBOW = 8,
  QFEA_COLORMAP_GRAYSCALE = 9,
  QFEA_COLORMAP_SEISMIC = 10,      /* Blue-white-red */
  QFEA_COLORMAP_TWILIGHT = 11,
  QFEA_COLORMAP_CUSTOM = 12,
} eQFEAColormap;

/**
 * Get colormap texture.
 *
 * Returns 1D texture with colormap lookup.
 *
 * \param colormap: Colormap type
 * \return GPU texture (1D, 256 samples)
 */
struct GPUTexture *QFEA_draw_colormap_get_texture(eQFEAColormap colormap);

/**
 * Map scalar value to RGB color.
 *
 * \param value: Input value
 * \param min_value: Minimum for normalization
 * \param max_value: Maximum for normalization
 * \param colormap: Colormap type
 * \param r_color: Output RGB color [3]
 */
void QFEA_draw_colormap_map_value(float value,
                                   float min_value,
                                   float max_value,
                                   eQFEAColormap colormap,
                                   float r_color[3]);

/**
 * Create custom colormap from control points.
 *
 * \param control_points: Array of (position, R, G, B) tuples
 * \param num_points: Number of control points
 * \return GPU texture
 */
struct GPUTexture *QFEA_draw_colormap_create_custom(const float (*control_points)[4],
                                                       int num_points);

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Resource Management
 * \{ */

/**
 * Upload tensor field to GPU as 3D texture.
 *
 * \param tensor: Tensor field
 * \param channel_index: Channel to upload
 * \return GPU texture (3D)
 */
struct GPUTexture *QFEA_draw_upload_tensor_channel(const QFEATensorField *tensor,
                                                      int channel_index);

/**
 * Free GPU resources for tensor field.
 *
 * \param tensor: Tensor field
 */
void QFEA_draw_free_tensor_gpu(QFEATensorField *tensor);

/**
 * Create GPU batch for voxel grid.
 *
 * \param voxel_grid: Voxel grid
 * \return GPU batch
 */
struct GPUBatch *QFEA_draw_create_voxel_batch(const QFEAVoxelGrid *voxel_grid);

/**
 * Free all QFEA GPU resources.
 */
void QFEA_draw_free_all_gpu_resources(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Overlay Drawing
 * \{ */

/**
 * Draw field probe overlay.
 *
 * Shows probe position and current value in viewport.
 *
 * \param position: Probe position
 * \param value: Current value
 * \param label: Probe label
 */
void QFEA_draw_overlay_probe(const float position[3], float value, const char *label);

/**
 * Draw boundary condition overlay.
 *
 * Highlights regions with specific boundary conditions.
 *
 * \param voxel_grid: Voxel grid
 * \param bc: Boundary condition
 */
void QFEA_draw_overlay_boundary_condition(const QFEAVoxelGrid *voxel_grid,
                                            const QFEABoundaryCondition *bc);

/**
 * Draw energy source overlay.
 *
 * Shows source position and radiation pattern.
 *
 * \param voxel_grid: Voxel grid
 * \param source: Energy source
 */
void QFEA_draw_overlay_energy_source(const QFEAVoxelGrid *voxel_grid,
                                       const QFEAEnergySource *source);

/**
 * Draw colormap legend.
 *
 * Shows colormap scale bar in viewport corner.
 *
 * \param settings: Draw settings
 * \param min_value: Minimum value
 * \param max_value: Maximum value
 * \param unit: Unit label
 */
void QFEA_draw_overlay_colormap_legend(const QFEADrawSettings *settings,
                                         float min_value,
                                         float max_value,
                                         const char *unit);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation
 * \{ */

/**
 * Animate tensor field slice position.
 *
 * Smoothly interpolates slice position over time.
 *
 * \param settings: Draw settings
 * \param target_position: Target slice position
 * \param duration: Animation duration (seconds)
 */
void QFEA_draw_animate_slice_position(QFEADrawSettings *settings,
                                        float target_position,
                                        float duration);

/**
 * Animate colormap range.
 *
 * Smoothly adjusts colormap min/max.
 *
 * \param settings: Draw settings
 * \param target_min: Target minimum
 * \param target_max: Target maximum
 * \param duration: Animation duration
 */
void QFEA_draw_animate_colormap_range(QFEADrawSettings *settings,
                                        float target_min,
                                        float target_max,
                                        float duration);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Performance
 * \{ */

/**
 * Set level of detail for drawing.
 *
 * Reduces quality for faster interaction.
 *
 * \param settings: Draw settings
 * \param lod_level: 0=full quality, 1-3=reduced detail
 */
void QFEA_draw_set_lod(QFEADrawSettings *settings, int lod_level);

/**
 * Enable/disable GPU caching.
 *
 * \param enabled: Enable caching
 */
void QFEA_draw_set_gpu_caching(bool enabled);

/** \} */

#ifdef __cplusplus
}
#endif
