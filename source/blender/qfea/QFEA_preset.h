/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA preset system for common simulation templates.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Preset Categories
 * \{ */

/**
 * Preset categories for organization.
 */
typedef enum eQFEAPresetCategory {
  QFEA_PRESET_CATEGORY_ELECTROMAGNETIC = 0,  /* EM simulations */
  QFEA_PRESET_CATEGORY_QUANTUM = 1,          /* Quantum mechanics */
  QFEA_PRESET_CATEGORY_PARTICLE = 2,         /* Particle dynamics */
  QFEA_PRESET_CATEGORY_THERMAL = 3,          /* Heat transfer */
  QFEA_PRESET_CATEGORY_MULTIPHYSICS = 4,     /* Coupled physics */
  QFEA_PRESET_CATEGORY_EDUCATION = 5,        /* Educational demos */
  QFEA_PRESET_CATEGORY_RESEARCH = 6,         /* Research templates */
  QFEA_PRESET_CATEGORY_CUSTOM = 7,           /* User-defined */
} eQFEAPresetCategory;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preset Definition
 * \{ */

/**
 * Simulation preset.
 */
typedef struct QFEAPreset {
  struct QFEAPreset *next, *prev;

  char name[64];                      /* Preset name */
  char description[512];              /* Description for UI */
  eQFEAPresetCategory category;       /* Category */

  /* Preview */
  struct ImBuf *preview_image;        /* Preview thumbnail */

  /* Configuration */
  QFEAPhysicsModule *physics;         /* Physics setup */
  ListBase materials;                 /* QFEAMaterial list */
  ListBase sources;                   /* Energy sources */
  ListBase boundary_conditions;       /* Boundary conditions */

  /* Voxel configuration */
  float suggested_resolution;         /* Recommended voxel size */
  int suggested_dimensions[3];        /* Recommended grid size */
  eQFEAVoxelLayout suggested_layout;  /* Storage layout */

  /* Compute configuration */
  char preferred_device[64];          /* "CPU", "CUDA", "OpenCL", "Metal" */
  int suggested_num_threads;          /* Thread count */

  /* Timeline configuration */
  float suggested_timestep;           /* Recommended dt */
  float suggested_duration;           /* Total simulation time */
  int suggested_output_interval;      /* Frames between outputs */

  /* Tags for search */
  char tags[256];                     /* Comma-separated tags */

  /* Metadata */
  char author[128];
  char version[32];
  double creation_time;
  char filepath[1024];                /* Path if loaded from file */
} QFEAPreset;

/**
 * Create new preset.
 *
 * \param name: Preset name
 * \param category: Category
 * \return Preset instance
 */
QFEAPreset *QFEA_preset_new(const char *name, eQFEAPresetCategory category);

/**
 * Free preset and all associated data.
 */
void QFEA_preset_free(QFEAPreset *preset);

/**
 * Copy preset (deep copy).
 */
QFEAPreset *QFEA_preset_copy(const QFEAPreset *preset);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preset Library
 * \{ */

/**
 * Initialize built-in preset library.
 *
 * Loads standard presets including:
 * - Dipole antenna radiation
 * - Waveguide modes
 * - Photonic crystal bandgap
 * - Quantum well energy levels
 * - Particle beam dynamics
 * - Thermal diffusion
 * - And many more...
 */
void QFEA_preset_library_init(void);

/**
 * Free preset library.
 */
void QFEA_preset_library_free(void);

/**
 * Get all presets in category.
 *
 * \param category: Category filter (-1 for all)
 * \param r_count: Number of presets found
 * \return Array of preset pointers
 */
QFEAPreset **QFEA_preset_library_get_category(eQFEAPresetCategory category, int *r_count);

/**
 * Get preset by name.
 *
 * \param name: Preset name
 * \return Preset pointer or NULL if not found
 */
QFEAPreset *QFEA_preset_library_get(const char *name);

/**
 * Search presets by tag or keyword.
 *
 * \param query: Search query
 * \param r_count: Number of matches
 * \return Array of matching presets
 */
QFEAPreset **QFEA_preset_library_search(const char *query, int *r_count);

/**
 * Add preset to library.
 *
 * \param preset: Preset to add (will be copied)
 */
void QFEA_preset_library_add(const QFEAPreset *preset);

/**
 * Remove preset from library.
 *
 * \param name: Preset name
 */
void QFEA_preset_library_remove(const char *name);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preset Application
 * \{ */

/**
 * Apply preset to scene.
 *
 * Sets up physics, materials, and recommended settings.
 *
 * \param preset: Preset to apply
 * \param voxel_grid: Target voxel grid
 * \param physics: Target physics module
 * \param use_suggested_settings: Use recommended resolution/timestep
 */
void QFEA_preset_apply(const QFEAPreset *preset,
                        QFEAVoxelGrid *voxel_grid,
                        QFEAPhysicsModule *physics,
                        bool use_suggested_settings);

/**
 * Create new scene from preset.
 *
 * Generates complete simulation setup from scratch.
 *
 * \param preset: Preset to instantiate
 * \param mesh: Optional geometry (NULL to use preset default)
 * \return true if successful
 */
bool QFEA_preset_create_scene(const QFEAPreset *preset, const struct Mesh *mesh);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Built-in Presets
 * \{ */

/**
 * Create dipole antenna radiation preset.
 *
 * Simulates electromagnetic radiation from dipole antenna.
 * Physics: FDTD
 * Materials: Air, PEC antenna
 * Sources: Voltage source at antenna center
 * BC: PML absorbing boundaries
 */
QFEAPreset *QFEA_preset_create_dipole_antenna(void);

/**
 * Create waveguide mode preset.
 *
 * Simulates TE/TM modes in rectangular waveguide.
 * Physics: FDTD
 * Materials: Air core, PEC walls
 * Sources: Plane wave input
 * BC: PEC walls, PML at ports
 */
QFEAPreset *QFEA_preset_create_waveguide(void);

/**
 * Create photonic crystal preset.
 *
 * Simulates bandgap in periodic dielectric structure.
 * Physics: FDTD
 * Materials: Si rods in air
 * Sources: Broadband pulse
 * BC: Periodic boundaries
 */
QFEAPreset *QFEA_preset_create_photonic_crystal(void);

/**
 * Create quantum well preset.
 *
 * Simulates particle in finite potential well.
 * Physics: Schrödinger equation
 * Materials: GaAs/AlGaAs heterostructure
 * Sources: Gaussian wavepacket
 * BC: Zero wavefunction at boundaries
 */
QFEAPreset *QFEA_preset_create_quantum_well(void);

/**
 * Create quantum harmonic oscillator preset.
 *
 * Simulates particle in parabolic potential.
 * Physics: Schrödinger equation
 * Materials: Single material
 * Sources: Ground state initialization
 */
QFEAPreset *QFEA_preset_create_harmonic_oscillator(void);

/**
 * Create hydrogen atom preset.
 *
 * Simulates electron orbitals in hydrogen.
 * Physics: Schrödinger equation with Coulomb potential
 * Materials: Vacuum
 * Sources: Orbital initialization (1s, 2s, 2p, etc.)
 */
QFEAPreset *QFEA_preset_create_hydrogen_atom(void);

/**
 * Create particle accelerator preset.
 *
 * Simulates charged particle dynamics in EM fields.
 * Physics: FDTD + Particle dynamics
 * Materials: Vacuum, PEC electrodes
 * Sources: Electron beam, RF cavity fields
 */
QFEAPreset *QFEA_preset_create_particle_accelerator(void);

/**
 * Create plasma simulation preset.
 *
 * Simulates particle-field coupling in plasma.
 * Physics: FDTD + Particle dynamics
 * Materials: Ionized gas
 * Sources: Particle injection, external fields
 */
QFEAPreset *QFEA_preset_create_plasma(void);

/**
 * Create nuclear fusion preset.
 *
 * Simulates fusion reactions (D-T, D-D, etc.).
 * Physics: Particle dynamics with nuclear forces
 * Materials: Deuterium, Tritium
 * Sources: High-energy particle beams
 */
QFEAPreset *QFEA_preset_create_fusion_reactor(void);

/**
 * Create thermal diffusion preset.
 *
 * Simulates heat transfer in solid.
 * Physics: Thermal solver
 * Materials: Metals, insulators
 * Sources: Heat sources, temperature BC
 */
QFEAPreset *QFEA_preset_create_thermal_diffusion(void);

/**
 * Create metamaterial preset.
 *
 * Simulates negative index metamaterial.
 * Physics: FDTD
 * Materials: Split-ring resonators, wire arrays
 * Sources: Plane wave
 * BC: Periodic boundaries
 */
QFEAPreset *QFEA_preset_create_metamaterial(void);

/**
 * Create optical cavity preset.
 *
 * Simulates resonant modes in optical cavity.
 * Physics: FDTD
 * Materials: Dielectric mirrors, air cavity
 * Sources: Broadband pulse
 * BC: PML boundaries
 */
QFEAPreset *QFEA_preset_create_optical_cavity(void);

/**
 * Create graphene bandstructure preset.
 *
 * Simulates electronic structure of graphene sheet.
 * Physics: Tight-binding Hamiltonian
 * Materials: Graphene lattice
 * Sources: Bloch wave initialization
 */
QFEAPreset *QFEA_preset_create_graphene_bandstructure(void);

/**
 * Create quantum tunneling preset.
 *
 * Simulates particle tunneling through potential barrier.
 * Physics: Schrödinger equation
 * Materials: Barrier potential
 * Sources: Gaussian wavepacket
 */
QFEAPreset *QFEA_preset_create_quantum_tunneling(void);

/**
 * Create Casimir effect preset.
 *
 * Simulates vacuum fluctuations between conducting plates.
 * Physics: Quantum field theory approximation
 * Materials: PEC plates, vacuum
 * Sources: Vacuum state
 */
QFEAPreset *QFEA_preset_create_casimir_effect(void);

/**
 * Create quantum dot preset.
 *
 * Simulates confined states in semiconductor quantum dot.
 * Physics: Schrödinger equation
 * Materials: InAs dot in GaAs matrix
 * Sources: Ground state
 */
QFEAPreset *QFEA_preset_create_quantum_dot(void);

/**
 * Create antenna array preset.
 *
 * Simulates phased array antenna radiation pattern.
 * Physics: FDTD
 * Materials: Air, PEC elements
 * Sources: Multiple voltage sources with phase control
 * BC: PML boundaries
 */
QFEAPreset *QFEA_preset_create_antenna_array(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name I/O
 * \{ */

/**
 * Save preset to file.
 *
 * \param preset: Preset to save
 * \param filepath: Output path (.qfpreset format)
 * \return true if successful
 */
bool QFEA_preset_save(const QFEAPreset *preset, const char *filepath);

/**
 * Load preset from file.
 *
 * \param filepath: Preset file path
 * \return Preset or NULL if load failed
 */
QFEAPreset *QFEA_preset_load(const char *filepath);

/**
 * Export preset to JSON.
 *
 * \param preset: Preset
 * \return JSON string (must be freed)
 */
char *QFEA_preset_export_json(const QFEAPreset *preset);

/**
 * Import preset from JSON.
 *
 * \param json: JSON string
 * \return Preset or NULL if parse failed
 */
QFEAPreset *QFEA_preset_import_json(const char *json);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Validation
 * \{ */

/**
 * Validate preset configuration.
 *
 * Checks for:
 * - Physics compatibility
 * - Material consistency
 * - Boundary condition validity
 * - Numerical stability (CFL condition, etc.)
 *
 * \param preset: Preset to validate
 * \param r_errors: Output error messages (allocated by function)
 * \param r_warnings: Output warnings (allocated by function)
 * \return true if valid
 */
bool QFEA_preset_validate(const QFEAPreset *preset, char **r_errors, char **r_warnings);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preset Templates
 * \{ */

/**
 * Create custom preset from current scene.
 *
 * \param name: Preset name
 * \param description: Description
 * \param voxel_grid: Current voxel grid
 * \param physics: Current physics module
 * \param category: Category
 * \return New preset
 */
QFEAPreset *QFEA_preset_from_scene(const char *name,
                                     const char *description,
                                     const QFEAVoxelGrid *voxel_grid,
                                     const QFEAPhysicsModule *physics,
                                     eQFEAPresetCategory category);

/**
 * Generate preset from natural language description (using LLM).
 *
 * \param llm_session: LLM session
 * \param description: Natural language description of desired simulation
 * \param r_preset: Output preset (allocated by function)
 * \return true if successful
 */
bool QFEA_preset_generate_from_llm(struct QFEALLMSession *llm_session,
                                     const char *description,
                                     QFEAPreset **r_preset);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preset Parametrization
 * \{ */

/**
 * Preset parameter.
 */
typedef struct QFEAPresetParameter {
  struct QFEAPresetParameter *next, *prev;

  char name[64];              /* Parameter name */
  char description[256];      /* Description for UI */

  /* Value */
  char type;                  /* 'f'=float, 'i'=int, 's'=string */
  void *value;                /* Current value */
  void *default_value;        /* Default value */

  /* Range (for numeric types) */
  void *min_value;
  void *max_value;

  /* Enum options (for string type) */
  char **enum_options;
  int num_enum_options;
} QFEAPresetParameter;

/**
 * Add parameter to preset.
 *
 * Makes preset parametric for user customization.
 *
 * \param preset: Preset
 * \param name: Parameter name
 * \param description: Description
 * \param type: Type ('f', 'i', 's')
 * \param default_value: Default value
 * \return Parameter pointer
 */
QFEAPresetParameter *QFEA_preset_add_parameter(QFEAPreset *preset,
                                                  const char *name,
                                                  const char *description,
                                                  char type,
                                                  const void *default_value);

/**
 * Set parameter value.
 *
 * \param preset: Preset
 * \param param_name: Parameter name
 * \param value: New value
 */
void QFEA_preset_set_parameter(QFEAPreset *preset, const char *param_name, const void *value);

/**
 * Get parameter value.
 *
 * \param preset: Preset
 * \param param_name: Parameter name
 * \return Value pointer or NULL if not found
 */
const void *QFEA_preset_get_parameter(const QFEAPreset *preset, const char *param_name);

/** \} */

#ifdef __cplusplus
}
#endif
