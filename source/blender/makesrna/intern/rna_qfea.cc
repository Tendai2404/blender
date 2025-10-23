/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 *
 * RNA definitions for QFEA quantum simulation system.
 */

#include <cstdlib>

#include "DNA_qfea_types.h"

#include "BLI_utildefines.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#ifdef RNA_RUNTIME

#  include "BKE_lib_id.hh"

#  include "QFEA_material.h"
#  include "QFEA_physics.h"
#  include "QFEA_tensor.h"
#  include "QFEA_voxel.h"

/* -------------------------------------------------------------------- */
/** \name Helper Functions
 * \{ */

static void rna_QFEAVoxelGrid_resolution_update(Main * /*bmain*/,
                                                 Scene * /*scene*/,
                                                 PointerRNA *ptr)
{
  QFEAVoxelGrid *voxel_grid = (QFEAVoxelGrid *)ptr->data;
  /* Trigger rebuild */
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, voxel_grid);
}

/** \} */

#endif /* RNA_RUNTIME */

/* -------------------------------------------------------------------- */
/** \name Voxel Grid RNA
 * \{ */

static void rna_def_qfea_voxelgrid(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem voxel_layout_items[] = {
      {QFEA_VOXEL_LAYOUT_DENSE, "DENSE", 0, "Dense", "Uniform dense array for maximum speed"},
      {QFEA_VOXEL_LAYOUT_SPARSE,
       "SPARSE",
       0,
       "Sparse",
       "Sparse storage for memory efficiency"},
      {QFEA_VOXEL_LAYOUT_OCTREE,
       "OCTREE",
       0,
       "Octree",
       "Adaptive octree for multi-resolution"},
      {QFEA_VOXEL_LAYOUT_BVH, "BVH", 0, "BVH", "Bounding volume hierarchy for ray tracing"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "QFEAVoxelGrid", "ID");
  RNA_def_struct_ui_text(srna, "QFEA Voxel Grid", "Spatial discretization for QFEA simulation");
  RNA_def_struct_ui_icon(srna, ICON_MESH_CUBE);

  /* Resolution */
  prop = RNA_def_property(srna, "resolution", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "resolution");
  RNA_def_property_range(prop, 1e-35f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Resolution", "Voxel size in meters (1e-35 Planck scale to 1m macroscopic)");
  RNA_def_property_update(prop, 0, "rna_QFEAVoxelGrid_resolution_update");

  /* Dimensions */
  prop = RNA_def_property(srna, "dimensions", PROP_INT, PROP_XYZ);
  RNA_def_property_int_sdna(prop, nullptr, "dims");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Dimensions", "Grid dimensions [nx, ny, nz]");

  /* Layout type */
  prop = RNA_def_property(srna, "layout", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "layout_type");
  RNA_def_property_enum_items(prop, voxel_layout_items);
  RNA_def_property_ui_text(prop, "Layout", "Storage layout type");

  /* Origin */
  prop = RNA_def_property(srna, "origin", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "origin");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Origin", "Grid origin in world coordinates");

  /* Bounds */
  prop = RNA_def_property(srna, "bounds_min", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "bounds_min");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Bounds Min", "Minimum bounds of grid");

  prop = RNA_def_property(srna, "bounds_max", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "bounds_max");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Bounds Max", "Maximum bounds of grid");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material RNA
 * \{ */

static void rna_def_qfea_material(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem material_category_items[] = {
      {QFEA_MATERIAL_CATEGORY_METAL, "METAL", 0, "Metal", "Metallic conductor"},
      {QFEA_MATERIAL_CATEGORY_SEMICONDUCTOR,
       "SEMICONDUCTOR",
       0,
       "Semiconductor",
       "Semiconductor material"},
      {QFEA_MATERIAL_CATEGORY_INSULATOR, "INSULATOR", 0, "Insulator", "Dielectric insulator"},
      {QFEA_MATERIAL_CATEGORY_MAGNETIC, "MAGNETIC", 0, "Magnetic", "Magnetic material"},
      {QFEA_MATERIAL_CATEGORY_SUPERCONDUCTOR,
       "SUPERCONDUCTOR",
       0,
       "Superconductor",
       "Superconducting material"},
      {QFEA_MATERIAL_CATEGORY_CUSTOM, "CUSTOM", 0, "Custom", "Custom user-defined material"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "QFEAMaterial", "ID");
  RNA_def_struct_ui_text(srna, "QFEA Material", "Material properties for QFEA simulation");
  RNA_def_struct_ui_icon(srna, ICON_MATERIAL);

  /* Name */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Name", "Material name");
  RNA_def_struct_name_property(srna, prop);

  /* Category */
  prop = RNA_def_property(srna, "category", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "category");
  RNA_def_property_enum_items(prop, material_category_items);
  RNA_def_property_ui_text(prop, "Category", "Material category");

  /* Electromagnetic properties */
  prop = RNA_def_property(srna, "epsilon_r", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "epsilon_r");
  RNA_def_property_range(prop, 1.0f, 1000.0f);
  RNA_def_property_ui_text(prop, "Relative Permittivity", "εᵣ (epsilon_r >= 1)");

  prop = RNA_def_property(srna, "mu_r", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "mu_r");
  RNA_def_property_range(prop, 0.001f, 1000.0f);
  RNA_def_property_ui_text(prop, "Relative Permeability", "μᵣ (mu_r > 0)");

  prop = RNA_def_property(srna, "sigma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sigma");
  RNA_def_property_range(prop, 0.0f, 1e8f);
  RNA_def_property_ui_text(prop, "Conductivity", "σ in S/m (0 for insulators, ~10⁷ for metals)");

  /* Physical properties */
  prop = RNA_def_property(srna, "density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "density");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_text(prop, "Density", "Mass density in kg/m³");

  prop = RNA_def_property(srna, "band_gap", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "band_gap");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "Band Gap", "Electronic band gap in eV (0 for metals)");

  /* Chemical formula */
  prop = RNA_def_property(srna, "formula", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "formula");
  RNA_def_property_ui_text(prop, "Formula", "Chemical formula (e.g., Cu, Si, GaAs)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tensor Field RNA
 * \{ */

static void rna_def_qfea_tensorfield(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "QFEATensorField", "ID");
  RNA_def_struct_ui_text(
      srna, "QFEA Tensor Field", "Multi-channel tensor field for simulation data");
  RNA_def_struct_ui_icon(srna, ICON_SURFACE_DATA);

  /* Number of channels */
  prop = RNA_def_property(srna, "num_channels", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "num_channels");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Channels", "Number of data channels");

  /* Voxel grid reference */
  prop = RNA_def_property(srna, "voxel_grid", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "QFEAVoxelGrid");
  RNA_def_property_pointer_sdna(prop, nullptr, "voxel_grid");
  RNA_def_property_ui_text(prop, "Voxel Grid", "Associated voxel grid");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Physics Module RNA
 * \{ */

static void rna_def_qfea_physics_module(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "QFEAPhysicsModule", "ID");
  RNA_def_struct_ui_text(
      srna, "QFEA Physics Module", "Physics solver configuration for QFEA simulation");
  RNA_def_struct_ui_icon(srna, ICON_PHYSICS);

  /* Enable flags */
  prop = RNA_def_property(srna, "em_enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "em_enabled", 1);
  RNA_def_property_ui_text(prop, "Electromagnetic", "Enable FDTD electromagnetic solver");

  prop = RNA_def_property(srna, "quantum_enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "quantum_enabled", 1);
  RNA_def_property_ui_text(prop, "Quantum", "Enable Schrödinger quantum solver");

  prop = RNA_def_property(srna, "particles_enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "particles_enabled", 1);
  RNA_def_property_ui_text(prop, "Particles", "Enable particle dynamics solver");

  /* EM configuration */
  prop = RNA_def_property(srna, "em_timestep", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "em_config.timestep");
  RNA_def_property_range(prop, 1e-18f, 1e-6f);
  RNA_def_property_ui_text(prop, "EM Timestep", "FDTD timestep in seconds (auto-computed if 0)");

  prop = RNA_def_property(srna, "em_courant_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "em_config.courant_factor");
  RNA_def_property_range(prop, 0.1f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Courant Factor", "CFL safety factor (0.95 recommended for stability)");

  /* Quantum configuration */
  prop = RNA_def_property(srna, "quantum_timestep", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "quantum_config.timestep");
  RNA_def_property_range(prop, 1e-20f, 1e-12f);
  RNA_def_property_ui_text(prop, "Quantum Timestep", "Schrödinger timestep in seconds");

  prop = RNA_def_property(srna, "particle_mass", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "quantum_config.particle_mass");
  RNA_def_property_range(prop, 9.109e-31f, 1.673e-27f);
  RNA_def_property_ui_text(
      prop, "Particle Mass", "Quantum particle mass in kg (electron: 9.109e-31)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void RNA_def_qfea(BlenderRNA *brna)
{
  rna_def_qfea_voxelgrid(brna);
  rna_def_qfea_material(brna);
  rna_def_qfea_tensorfield(brna);
  rna_def_qfea_physics_module(brna);
}

/** \} */
