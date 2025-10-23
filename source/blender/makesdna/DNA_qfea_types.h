/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * QFEA (Quantum Finite Element Analysis) data structures for native integration
 * into Blender's core architecture.
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_customdata_types.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct BVHTree;
struct GPUBatch;
struct GPUTexture;
struct GPUVertBuf;
struct Material;
struct Mesh;
struct Object;

/* -------------------------------------------------------------------- */
/** \name QFEA Enums and Constants
 * \{ */

/** Voxel grid layout strategies */
typedef enum eQFEAVoxelLayout {
  QFEA_VOXEL_LAYOUT_DENSE = 0,   /* Simple 3D array, fast access */
  QFEA_VOXEL_LAYOUT_SPARSE = 1,  /* Only store occupied voxels */
  QFEA_VOXEL_LAYOUT_OCTREE = 2,  /* Hierarchical adaptive refinement */
  QFEA_VOXEL_LAYOUT_BVH = 3,     /* Bounding volume hierarchy */
} eQFEAVoxelLayout;

/** Crystal system types for atomic structures */
typedef enum eQFEACrystalSystem {
  QFEA_CRYSTAL_CUBIC = 0,
  QFEA_CRYSTAL_FCC = 1,
  QFEA_CRYSTAL_BCC = 2,
  QFEA_CRYSTAL_HCP = 3,
  QFEA_CRYSTAL_DIAMOND = 4,
  QFEA_CRYSTAL_GRAPHENE = 5,
  QFEA_CRYSTAL_MONOCLINIC = 6,
  QFEA_CRYSTAL_TRICLINIC = 7,
  QFEA_CRYSTAL_CUSTOM = 8,
} eQFEACrystalSystem;

/** Tensor field data types */
typedef enum eQFEATensorDataType {
  QFEA_DTYPE_FLOAT32 = 0,
  QFEA_DTYPE_FLOAT64 = 1,
  QFEA_DTYPE_COMPLEX64 = 2,
  QFEA_DTYPE_COMPLEX128 = 3,
} eQFEATensorDataType;

/** Tensor channel types */
typedef enum eQFEATensorChannelType {
  QFEA_CHANNEL_SCALAR = 0,
  QFEA_CHANNEL_VECTOR3 = 1,
  QFEA_CHANNEL_TENSOR33 = 2,
} eQFEATensorChannelType;

/** Physics solver types */
typedef enum eQFEASolverType {
  QFEA_SOLVER_FDTD_YEE = 0,        /* Electromagnetic FDTD */
  QFEA_SOLVER_SPLIT_OPERATOR = 1,  /* Quantum split-operator */
  QFEA_SOLVER_FEM = 2,              /* Finite element method */
  QFEA_SOLVER_PARTICLE = 3,         /* Particle tracer */
} eQFEASolverType;

/** Boundary condition types */
typedef enum eQFEABoundaryType {
  QFEA_BC_PEC = 0,       /* Perfect electric conductor */
  QFEA_BC_PMC = 1,       /* Perfect magnetic conductor */
  QFEA_BC_PML = 2,       /* Perfectly matched layer */
  QFEA_BC_PERIODIC = 3,  /* Periodic boundary */
  QFEA_BC_DIRICHLET = 4, /* Fixed value */
  QFEA_BC_NEUMANN = 5,   /* Fixed derivative */
} eQFEABoundaryType;

/** Energy source types */
typedef enum eQFEASourceType {
  QFEA_SOURCE_VOLTAGE = 0,
  QFEA_SOURCE_CURRENT = 1,
  QFEA_SOURCE_PLANE_WAVE = 2,
  QFEA_SOURCE_POINT = 3,
  QFEA_SOURCE_FIELD = 4,
  QFEA_SOURCE_EXPRESSION = 5,
} eQFEASourceType;

/** Study mode types */
typedef enum eQFEAStudyMode {
  QFEA_MODE_DESIGN = 0,
  QFEA_MODE_SIMULATION = 1,
  QFEA_MODE_ANALYSIS = 2,
  QFEA_MODE_PARAMETRIC = 3,
  QFEA_MODE_OPTIMIZATION = 4,
  QFEA_MODE_VALIDATION = 5,
  QFEA_MODE_COMPARE = 6,
  QFEA_MODE_REALTIME = 7,
} eQFEAStudyMode;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Atomic Structure
 * \{ */

/** Single atom position in unit cell */
typedef struct QFEAAtomPosition {
  int element_id;        /* Atomic number (Z) */
  float position[3];     /* Fractional coordinates (0-1) */
  float occupancy;       /* Partial occupancy (0-1) */
  char orbital_config[32]; /* Electron configuration string */
  char _pad[4];
} QFEAAtomPosition;

/** Crystallographic atomic structure */
typedef struct QFEAAtomicStructure {
  /** Crystal system type */
  char crystal_system;
  char _pad1[3];

  /** Unit cell parameters (in Angstroms) */
  float lattice_constants[3]; /* a, b, c */
  float lattice_angles[3];    /* alpha, beta, gamma (radians) */

  /** Space group */
  int space_group_number;      /* 1-230 */
  char space_group_symbol[32]; /* Hermann-Mauguin notation */

  /** Atomic positions */
  int num_atoms_per_cell;
  struct QFEAAtomPosition *atom_positions;

  /** Symmetry operations (4x4 matrices) */
  int num_symmetry_ops;
  float (*symmetry_operations)[4][4];

  /** Visualization settings */
  float atom_radii_scale;
  float bond_threshold;

  /** Computed properties */
  float density_calculated;
  float coordination_number;

  char _pad2[4];
} QFEAAtomicStructure;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Properties
 * \{ */

/** Frequency-dependent optical data point */
typedef struct QFEAOpticalData {
  float frequency;    /* Hz */
  float n_real;       /* Refractive index (real part) */
  float n_imag;       /* Refractive index (imaginary part) */
  float absorption;   /* Absorption coefficient (1/m) */
} QFEAOpticalData;

/** Temperature-dependent thermal data point */
typedef struct QFEAThermalData {
  float temperature;         /* Kelvin */
  float thermal_conductivity; /* W/(m·K) */
  float specific_heat;        /* J/(kg·K) */
  float thermal_expansion;    /* 1/K */
} QFEAThermalData;

/** Comprehensive material properties */
typedef struct QFEAMaterial {
  ID id; /* Inherits from Blender ID system */

  /** Basic identification */
  char formula[64];     /* Chemical formula */
  char category[64];    /* Metal, Semiconductor, Insulator, etc. */

  /** Electromagnetic properties */
  float epsilon_r;      /* Relative permittivity */
  float mu_r;           /* Relative permeability */
  float sigma;          /* Conductivity (S/m) */
  float dielectric_loss_tangent;

  /** Thermal properties */
  float thermal_conductivity;  /* W/(m·K) */
  float specific_heat;          /* J/(kg·K) */
  float thermal_expansion;      /* 1/K */
  float melting_point;          /* K */

  /** Mechanical properties */
  float rho;             /* Density (kg/m³) */
  float youngs_modulus;  /* Pa */
  float poisson_ratio;
  float tensile_strength; /* Pa */
  float hardness;
  float bulk_modulus;    /* Pa */
  float shear_modulus;   /* Pa */

  /** Electronic properties */
  float band_gap;           /* eV */
  float electron_affinity;  /* eV */
  float work_function;      /* eV */
  float fermi_level;        /* eV */
  char band_structure_type; /* METAL=0, SEMICONDUCTOR=1, INSULATOR=2 */
  char _pad1[3];

  /** Magnetic properties */
  float magnetic_susceptibility;
  float curie_temperature;  /* K */
  char magnetic_type;       /* DIAMAGNETIC=0, PARAMAGNETIC=1, FERROMAGNETIC=2 */
  char _pad2[3];

  /** Nuclear properties */
  float neutron_cross_section; /* barns */
  float fission_threshold;     /* MeV */
  float fusion_threshold;      /* keV */

  /** Chemical properties */
  float electronegativity;
  int oxidation_states[8];
  float ionization_energies[8];

  /** Frequency-dependent properties */
  int num_optical_points;
  struct QFEAOpticalData *optical_data;

  /** Temperature-dependent properties */
  int num_thermal_points;
  struct QFEAThermalData *thermal_data;

  /** Atomic structure */
  struct QFEAAtomicStructure *atomic_structure;

  /** Metadata */
  char data_source[256];
  float confidence;      /* 0-1, data quality measure */
  double timestamp_imported;
  char notes[1024];

  char _pad3[4];
} QFEAMaterial;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Voxel Grid
 * \{ */

/** Octree node for adaptive voxelization */
typedef struct QFEAOctreeNode {
  float center[3];
  float size;

  /* Children (NULL if leaf) */
  struct QFEAOctreeNode *children[8];

  /* Data (for leaf nodes) */
  char material_index;
  char is_leaf;
  char _pad[6];
} QFEAOctreeNode;

/** Voxel grid - spatial discretization container */
typedef struct QFEAVoxelGrid {
  ID id; /* Standard Blender ID block */

  /** Spatial configuration */
  float resolution;     /* Voxel size in meters (1e-35 to 1.0) */
  int dims[3];          /* Grid dimensions [nx, ny, nz] */
  float bbox_min[3];    /* Bounding box minimum */
  float bbox_max[3];    /* Bounding box maximum */
  float transform_matrix[4][4]; /* World space transformation */

  /** Layout strategy */
  char layout_type;     /* eQFEAVoxelLayout */
  char _pad1[3];
  int total_voxels;     /* Total active voxels */
  int memory_footprint_bytes;

  /** Dense storage (for DENSE layout) */
  float *occupancy_data;      /* [nx*ny*nz] - 0=empty, 1=solid, 0-1=partial */
  char *material_indices;     /* [nx*ny*nz] - material ID per voxel */

  /** Sparse storage (for SPARSE layout) */
  int num_active_voxels;
  int *sparse_indices;        /* [num_active*3] - (x,y,z) coordinates */
  char *sparse_materials;     /* [num_active] - material per active voxel */

  /** Octree storage (for OCTREE layout) */
  struct QFEAOctreeNode *octree_root;
  int octree_max_depth;
  int octree_min_node_size;

  /** BVH storage (for BVH layout) */
  struct BVHTree *bvh;

  /** GPU acceleration */
  struct GPUVertBuf *gpu_buffer;
  struct GPUBatch *gpu_batch;

  /** Material assignments */
  ListBase material_regions; /* QFEAMaterialRegion */

  /** Adaptive refinement settings */
  char adaptive_enabled;
  char _pad2[3];
  int refinement_levels;

  /** Memory management */
  struct CustomData vdata;
  int totlayer;

  char _pad3[4];
} QFEAVoxelGrid;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tensor Field
 * \{ */

/** Single tensor channel definition */
typedef struct QFEATensorChannel {
  char name[64];          /* Channel name (e.g., "Ex", "psi") */
  char unit[32];          /* Physical unit (e.g., "V/m", "eV") */
  char channel_type;      /* eQFEATensorChannelType */
  char data_type;         /* eQFEATensorDataType */
  char _pad[6];
  int num_components;     /* 1 for scalar, 3 for vector, 9 for tensor */
} QFEATensorChannel;

/** Multi-dimensional physical field data */
typedef struct QFEATensorField {
  ID id;

  /** Schema definition */
  int num_channels;
  struct QFEATensorChannel *channels;

  /** Reference to spatial grid */
  struct QFEAVoxelGrid *voxel_grid;

  /** Data storage */
  void *data_cpu;          /* Host memory */
  struct GPUTexture *data_gpu; /* GPU 3D texture */
  int data_type;           /* eQFEATensorDataType */
  int total_elements;      /* nx * ny * nz * num_channels */

  /** Time evolution */
  int current_frame;
  float current_time;
  double timestamp;

  /** Physics coupling */
  struct QFEAPhysicsModule *physics;

  /** Cache management */
  struct QFEACache *cache;

  /** Distributed storage */
  struct QFEADistributedData *dist_data;

  char _pad[4];
} QFEATensorField;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Physics Module
 * \{ */

/** Electromagnetic solver configuration */
typedef struct QFEAEMConfig {
  char solver_type;     /* FDTD_YEE, etc. */
  char _pad[3];
  float dt;             /* Timestep (seconds) */
  float cfl_factor;     /* CFL stability factor (0-1) */

  /** PML boundary settings */
  int pml_thickness;    /* Number of PML layers */
  float pml_attenuation; /* Attenuation factor */

  /** Dispersion modeling */
  char enable_dispersion;
  char dispersion_model;  /* DRUDE=0, LORENTZ=1 */
  char _pad2[6];
} QFEAEMConfig;

/** Quantum mechanics solver configuration */
typedef struct QFEAQuantumConfig {
  char solver_type;     /* SPLIT_OPERATOR=0, FINITE_DIFF=1 */
  char _pad[3];
  float dt;
  float hbar;           /* Reduced Planck constant */
  float mass;           /* Particle mass (kg) */

  /** Split-operator settings */
  char use_fft;
  char normalize_wavefunction;
  char _pad2[2];
  int normalization_interval; /* Renormalize every N steps */
} QFEAQuantumConfig;

/** Particle physics configuration */
typedef struct QFEAParticleConfig {
  float dt;
  int max_particles;
  float collision_threshold; /* Distance for collision detection (m) */

  /** Nuclear reactions */
  char enable_fusion;
  char enable_fission;
  char _pad[6];

  struct NuclearDatabase *nuclear_db;
} QFEAParticleConfig;

/** Energy source definition */
typedef struct QFEAEnergySource {
  struct QFEAEnergySource *next, *prev;

  char name[64];
  char source_type;     /* eQFEASourceType */
  char _pad[3];

  float amplitude;
  float frequency;      /* Hz */
  float phase;          /* radians */

  /** Time-dependent expression */
  char expression[256]; /* Mathematical expression (e.g., "sin(2*pi*f*t)") */

  /** Spatial definition */
  char geometry_type[32]; /* POINT, FACE, VOLUME, etc. */
  int *geometry_indices;   /* Voxel/face indices */
  int num_indices;

  /** Waveform */
  char waveform[32];    /* SINE, SQUARE, GAUSSIAN, CUSTOM */

  char _pad2[4];
} QFEAEnergySource;

/** Boundary condition definition */
typedef struct QFEABoundaryCondition {
  struct QFEABoundaryCondition *next, *prev;

  char name[64];
  char bc_type;         /* eQFEABoundaryType */
  char _pad[3];

  /** Faces to apply BC */
  char faces[64];       /* "X_MIN", "X_MAX", "ALL", etc. */

  /** Parameters (BC-specific) */
  float params[8];

  /** PML-specific */
  int thickness;
  float attenuation;

  char _pad2[4];
} QFEABoundaryCondition;

/** Physics module - solver orchestration */
typedef struct QFEAPhysicsModule {
  ID id;

  /** Active solvers */
  char em_enabled;
  char quantum_enabled;
  char particles_enabled;
  char thermal_enabled;
  char mechanical_enabled;
  char _pad1[3];

  /** Solver configurations */
  struct QFEAEMConfig em_config;
  struct QFEAQuantumConfig quantum_config;
  struct QFEAParticleConfig particle_config;

  /** Global timestep */
  float dt;
  float t_end;
  float t_current;

  /** Coupling */
  char coupling_mode;   /* NONE=0, WEAK=1, STRONG=2, FULL=3 */
  char _pad2[3];

  /** Material database */
  ListBase materials;   /* QFEAMaterial */

  /** Sources and boundary conditions */
  ListBase energy_sources;        /* QFEAEnergySource */
  ListBase boundary_conditions;   /* QFEABoundaryCondition */

  char _pad3[4];
} QFEAPhysicsModule;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simulation State
 * \{ */

/** Simulation execution state */
typedef struct QFEASimulation {
  /** Time control */
  float time_step;
  float time_end;
  float time_current;

  /** Progress tracking */
  int current_frame;
  int total_frames;
  float progress;       /* 0-1 */

  /** State flags */
  char is_running;
  char is_paused;
  char sync_with_timeline;
  char adaptive_timestep;

  /** Performance metrics */
  double elapsed_time;
  float steps_per_second;

  /** Checkpoint */
  char checkpoint_enabled;
  char _pad[3];
  int checkpoint_interval; /* Frames between checkpoints */
  char checkpoint_path[1024];

  char _pad2[4];
} QFEASimulation;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute Infrastructure
 * \{ */

/** Compute device description */
typedef struct QFEAComputeDevice {
  char name[256];
  char device_type[32];  /* CPU, CUDA, OPENCL, METAL */

  int id;
  size_t memory_bytes;
  int compute_units;

  char is_available;
  char _pad[3];
  float utilization;    /* 0-1 */
} QFEAComputeDevice;

/** Spatial tile for domain decomposition */
typedef struct QFEAComputeTile {
  int start[3];         /* Voxel start indices */
  int end[3];           /* Voxel end indices */
  int halo_width;       /* Ghost cell width */

  int device_id;        /* Assigned device */
  int priority;         /* Scheduling priority */
  float estimated_time; /* Load balancing metric */
} QFEAComputeTile;

/** Compute session - orchestrates distributed execution */
typedef struct QFEAComputeSession {
  ID id;

  /** Device management */
  struct QFEAComputeDevice *devices;
  int num_devices;
  int num_local_devices;
  int num_remote_devices;

  /** Tiling strategy */
  char tiling_strategy[32]; /* BRICKS, SLABS, ADAPTIVE */
  int tile_size[3];
  int num_tiles;
  struct QFEAComputeTile *tiles;

  /** Network */
  char network_enabled;
  char _pad[3];
  int master_port;
  ListBase remote_nodes; /* QFEARemoteNode */

  /** Performance */
  double total_compute_time;
  double total_communication_time;

  char _pad2[4];
} QFEAComputeSession;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Timeline & Analysis
 * \{ */

/** Timeline track - data source for visualization */
typedef struct QFEATimelineTrack {
  struct QFEATimelineTrack *next, *prev;

  char name[64];
  char data_source[128]; /* "tensor.Ex", "analysis.energy", etc. */

  char sample_mode[32];  /* VOXEL, REGION, GLOBAL */
  int sample_voxel[3];   /* For VOXEL mode */
  char sample_region[64]; /* For REGION mode */
  char aggregation[32];   /* MEAN, MAX, MIN, SUM, RMS */

  /** Display */
  float color[4];
  char visible;
  char auto_scale;
  char _pad[2];
  float y_scale;
  float y_offset;
  float manual_min, manual_max;

  /** Data (runtime, not saved) */
  int num_samples;
  float *time_values;
  float *data_values;

  /** Statistics */
  float min_value, max_value, mean_value;

  char _pad2[4];
} QFEATimelineTrack;

/** Timeline container */
typedef struct QFEATimeline {
  ID id;

  ListBase tracks; /* QFEATimelineTrack */

  /** Time range */
  double time_start;
  double time_end;
  double time_current;

  /** View settings */
  double view_time_start;
  double view_time_end;
  float view_value_min;
  float view_value_max;

  /** Cursor */
  double cursor_time;
  char cursor_snap_to_frames;
  char _pad[7];

  /** Playback */
  char is_playing;
  char _pad2[3];
  float playback_speed;

  /** Update tracking */
  char needs_update;
  char _pad3[3];
  int last_frame_updated;

  char _pad4[4];
} QFEATimeline;

/** \} */

/* -------------------------------------------------------------------- */
/** \name LLM Integration
 * \{ */

/** LLM conversation message */
typedef struct QFEALLMMessage {
  struct QFEALLMMessage *next, *prev;

  char role[16];        /* user, assistant, system */
  char content[8192];   /* Message text */
  double timestamp;

  /** Attachments (for future expansion) */
  int num_attachments;
  char _pad[4];
} QFEALLMMessage;

/** LLM assistant configuration */
typedef struct QFEALLM {
  ID id;

  /** API configuration */
  char api_endpoint[512];
  char api_key[256];
  char model_name[64];

  /** Connection state */
  char is_connected;
  char _pad1[3];
  int max_tokens;
  float temperature;

  /** Capabilities */
  char can_control_simulation;
  char can_analyze_results;
  char can_suggest_parameters;
  char can_generate_code;

  /** Conversation history */
  ListBase conversation_history; /* QFEALLMMessage */
  int max_history_messages;

  /** System prompt */
  char system_prompt[4096];

  /** Statistics */
  int total_requests;
  int total_tokens_used;
  float total_cost;

  char _pad2[4];
} QFEALLM;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main QFEA Properties
 * \{ */

/** Global QFEA properties attached to Scene */
typedef struct QFEAProperties {
  /** Study mode */
  char study_mode; /* eQFEAStudyMode */
  char _pad1[3];

  /** Voxelization */
  struct QFEAVoxelGrid *voxel_grid;
  char voxels_generated;
  char _pad2[3];
  int voxel_count;

  /** Tensor field */
  struct QFEATensorField *tensor_field;

  /** Physics */
  struct QFEAPhysicsModule *physics_module;

  /** Simulation state */
  struct QFEASimulation *simulation;

  /** Compute */
  struct QFEAComputeSession *compute_session;

  /** Timeline */
  struct QFEATimeline *timeline;

  /** LLM assistant */
  struct QFEALLM *llm;

  /** Visualization settings */
  int display_channel;
  char colormap[32];
  char show_vectors;
  char show_streamlines;
  char show_energy_overlay;
  char _pad3[3];
  float vector_scale;

  /** Material database */
  ListBase custom_materials; /* QFEAMaterial */

  char _pad4[4];
} QFEAProperties;

/** \} */

#ifdef __cplusplus
}
#endif
