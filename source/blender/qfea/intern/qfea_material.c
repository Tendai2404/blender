/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup qfea
 *
 * QFEA material database and atomic structure management.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_qfea_types.h"

#include "BKE_lib_id.hh"

#include "QFEA_material.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------- */
/** \name Element Database
 * \{ */

/**
 * Periodic table element data.
 */
typedef struct ElementData {
  int atomic_number;
  const char *symbol;
  const char *name;
  float atomic_mass;      /* Atomic mass units (u) */
  float covalent_radius;  /* Angstroms */
  float cpk_color[3];     /* RGB color for visualization */
} ElementData;

static const ElementData g_element_table[] = {
    {1, "H", "Hydrogen", 1.008f, 0.31f, {1.0f, 1.0f, 1.0f}},
    {2, "He", "Helium", 4.003f, 0.28f, {0.85f, 1.0f, 1.0f}},
    {3, "Li", "Lithium", 6.941f, 1.28f, {0.8f, 0.5f, 1.0f}},
    {4, "Be", "Beryllium", 9.012f, 0.96f, {0.76f, 1.0f, 0.0f}},
    {5, "B", "Boron", 10.811f, 0.84f, {1.0f, 0.71f, 0.71f}},
    {6, "C", "Carbon", 12.011f, 0.76f, {0.56f, 0.56f, 0.56f}},
    {7, "N", "Nitrogen", 14.007f, 0.71f, {0.19f, 0.31f, 0.97f}},
    {8, "O", "Oxygen", 15.999f, 0.66f, {1.0f, 0.05f, 0.05f}},
    {9, "F", "Fluorine", 18.998f, 0.57f, {0.56f, 0.88f, 0.31f}},
    {10, "Ne", "Neon", 20.180f, 0.58f, {0.7f, 0.89f, 0.96f}},
    {11, "Na", "Sodium", 22.990f, 1.66f, {0.67f, 0.36f, 0.95f}},
    {12, "Mg", "Magnesium", 24.305f, 1.41f, {0.54f, 1.0f, 0.0f}},
    {13, "Al", "Aluminum", 26.982f, 1.21f, {0.75f, 0.65f, 0.65f}},
    {14, "Si", "Silicon", 28.086f, 1.11f, {0.94f, 0.78f, 0.63f}},
    {15, "P", "Phosphorus", 30.974f, 1.07f, {1.0f, 0.5f, 0.0f}},
    {16, "S", "Sulfur", 32.065f, 1.05f, {1.0f, 1.0f, 0.19f}},
    {17, "Cl", "Chlorine", 35.453f, 1.02f, {0.12f, 0.94f, 0.12f}},
    {18, "Ar", "Argon", 39.948f, 1.06f, {0.5f, 0.82f, 0.89f}},
    {19, "K", "Potassium", 39.098f, 2.03f, {0.56f, 0.25f, 0.83f}},
    {20, "Ca", "Calcium", 40.078f, 1.76f, {0.24f, 1.0f, 0.0f}},
    {29, "Cu", "Copper", 63.546f, 1.32f, {0.78f, 0.5f, 0.2f}},
    {30, "Zn", "Zinc", 65.38f, 1.22f, {0.49f, 0.5f, 0.69f}},
    {47, "Ag", "Silver", 107.868f, 1.45f, {0.75f, 0.75f, 0.75f}},
    {79, "Au", "Gold", 196.967f, 1.36f, {1.0f, 0.84f, 0.0f}},
    {82, "Pb", "Lead", 207.2f, 1.47f, {0.34f, 0.35f, 0.38f}},
};

static const int g_num_elements = sizeof(g_element_table) / sizeof(ElementData);

const char *QFEA_element_get_symbol(int atomic_number)
{
  for (int i = 0; i < g_num_elements; i++) {
    if (g_element_table[i].atomic_number == atomic_number) {
      return g_element_table[i].symbol;
    }
  }
  return "X";  /* Unknown */
}

int QFEA_element_get_atomic_number(const char *symbol)
{
  for (int i = 0; i < g_num_elements; i++) {
    if (STRCASEEQ(g_element_table[i].symbol, symbol)) {
      return g_element_table[i].atomic_number;
    }
  }
  return -1;
}

float QFEA_element_get_mass(int atomic_number)
{
  for (int i = 0; i < g_num_elements; i++) {
    if (g_element_table[i].atomic_number == atomic_number) {
      return g_element_table[i].atomic_mass;
    }
  }
  return 0.0f;
}

float QFEA_element_get_covalent_radius(int atomic_number)
{
  for (int i = 0; i < g_num_elements; i++) {
    if (g_element_table[i].atomic_number == atomic_number) {
      return g_element_table[i].covalent_radius;
    }
  }
  return 1.0f;
}

void QFEA_element_get_color(int atomic_number, float r_color[3])
{
  for (int i = 0; i < g_num_elements; i++) {
    if (g_element_table[i].atomic_number == atomic_number) {
      copy_v3_v3(r_color, g_element_table[i].cpk_color);
      return;
    }
  }

  /* Default gray for unknown elements */
  r_color[0] = 0.5f;
  r_color[1] = 0.5f;
  r_color[2] = 0.5f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Database
 * \{ */

static ListBase g_material_database = {nullptr, nullptr};

void QFEA_material_database_init(void)
{
  BLI_listbase_clear(&g_material_database);

  /* Add built-in materials */

  /* Copper */
  {
    QFEAMaterial *mat = QFEA_material_new("Copper");
    STRNCPY(mat->formula, "Cu");
    mat->epsilon_r = 1.0f;
    mat->mu_r = 0.999994f;
    mat->sigma = 5.96e7f;  /* S/m at 20°C */
    mat->density = 8960.0f; /* kg/m³ */
    mat->band_gap = 0.0f;
    mat->category = QFEA_MATERIAL_CATEGORY_METAL;

    /* FCC crystal structure */
    mat->atomic_structure = QFEA_lattice_create_fcc(29, 3.615f);

    BLI_addtail(&g_material_database, mat);
  }

  /* Silicon */
  {
    QFEAMaterial *mat = QFEA_material_new("Silicon");
    STRNCPY(mat->formula, "Si");
    mat->epsilon_r = 11.68f;
    mat->mu_r = 1.0f;
    mat->sigma = 1e-3f;     /* Intrinsic, room temp */
    mat->density = 2329.0f;
    mat->band_gap = 1.12f;  /* eV at 300K */
    mat->category = QFEA_MATERIAL_CATEGORY_SEMICONDUCTOR;

    /* Diamond cubic structure */
    mat->atomic_structure = QFEA_lattice_create_diamond(14, 5.431f);

    BLI_addtail(&g_material_database, mat);
  }

  /* Vacuum */
  {
    QFEAMaterial *mat = QFEA_material_new("Vacuum");
    STRNCPY(mat->formula, "");
    mat->epsilon_r = 1.0f;
    mat->mu_r = 1.0f;
    mat->sigma = 0.0f;
    mat->density = 0.0f;
    mat->band_gap = 0.0f;
    mat->category = QFEA_MATERIAL_CATEGORY_INSULATOR;
    mat->atomic_structure = nullptr;

    BLI_addtail(&g_material_database, mat);
  }

  /* Air */
  {
    QFEAMaterial *mat = QFEA_material_new("Air");
    STRNCPY(mat->formula, "N2/O2");
    mat->epsilon_r = 1.00059f;
    mat->mu_r = 1.0f;
    mat->sigma = 0.0f;
    mat->density = 1.225f;
    mat->band_gap = 0.0f;
    mat->category = QFEA_MATERIAL_CATEGORY_INSULATOR;
    mat->atomic_structure = nullptr;

    BLI_addtail(&g_material_database, mat);
  }
}

void QFEA_material_database_free(void)
{
  LISTBASE_FOREACH_MUTABLE (QFEAMaterial *, mat, &g_material_database) {
    QFEA_material_free(mat);
  }
  BLI_listbase_clear(&g_material_database);
}

QFEAMaterial *QFEA_material_database_get(const char *name)
{
  LISTBASE_FOREACH (QFEAMaterial *, mat, &g_material_database) {
    if (STRCASEEQ(mat->name, name)) {
      return mat;
    }
  }
  return nullptr;
}

void QFEA_material_database_add(QFEAMaterial *material)
{
  if (!material) {
    return;
  }

  /* Check for duplicates */
  QFEAMaterial *existing = QFEA_material_database_get(material->name);
  if (existing) {
    return;  /* Already exists */
  }

  BLI_addtail(&g_material_database, material);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Creation
 * \{ */

QFEAMaterial *QFEA_material_new(const char *name)
{
  QFEAMaterial *material = (QFEAMaterial *)MEM_callocN(sizeof(QFEAMaterial), "QFEAMaterial");

  BKE_lib_id_init(&material->id, ID_NT);
  STRNCPY(material->name, name);

  /* Default properties */
  material->epsilon_r = 1.0f;
  material->mu_r = 1.0f;
  material->sigma = 0.0f;
  material->density = 1000.0f;
  material->band_gap = 0.0f;

  material->category = QFEA_MATERIAL_CATEGORY_CUSTOM;

  material->atomic_structure = nullptr;

  return material;
}

void QFEA_material_free(QFEAMaterial *material)
{
  if (!material) {
    return;
  }

  if (material->atomic_structure) {
    QFEA_atomic_structure_free(material->atomic_structure);
  }

  MEM_freeN(material);
}

QFEAMaterial *QFEA_material_copy(const QFEAMaterial *material)
{
  if (!material) {
    return nullptr;
  }

  QFEAMaterial *copy = QFEA_material_new(material->name);
  memcpy(copy, material, sizeof(QFEAMaterial));

  if (material->atomic_structure) {
    copy->atomic_structure = QFEA_atomic_structure_copy(material->atomic_structure);
  }

  return copy;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Atomic Structure
 * \{ */

QFEAAtomicStructure *QFEA_atomic_structure_new(eQFEACrystalSystem crystal_system,
                                                 float a,
                                                 float b,
                                                 float c,
                                                 float alpha,
                                                 float beta,
                                                 float gamma,
                                                 int element_id)
{
  QFEAAtomicStructure *structure = (QFEAAtomicStructure *)MEM_callocN(
      sizeof(QFEAAtomicStructure), "QFEAAtomicStructure");

  structure->crystal_system = crystal_system;
  structure->lattice_constants[0] = a;
  structure->lattice_constants[1] = b;
  structure->lattice_constants[2] = c;
  structure->lattice_angles[0] = alpha;
  structure->lattice_angles[1] = beta;
  structure->lattice_angles[2] = gamma;

  structure->space_group_number = 1;  /* P1 by default */

  /* Allocate atom positions based on crystal system */
  /* This would be implemented based on specific crystal geometry */

  return structure;
}

void QFEA_atomic_structure_free(QFEAAtomicStructure *structure)
{
  if (!structure) {
    return;
  }

  if (structure->atom_positions) {
    MEM_freeN(structure->atom_positions);
  }

  if (structure->symmetry_operations) {
    MEM_freeN(structure->symmetry_operations);
  }

  MEM_freeN(structure);
}

QFEAAtomicStructure *QFEA_atomic_structure_copy(const QFEAAtomicStructure *structure)
{
  if (!structure) {
    return nullptr;
  }

  QFEAAtomicStructure *copy = (QFEAAtomicStructure *)MEM_dupallocN(structure);

  if (structure->atom_positions && structure->num_atoms > 0) {
    copy->atom_positions = (QFEAAtomPosition *)MEM_dupallocN(structure->atom_positions);
  }

  if (structure->symmetry_operations && structure->num_symmetry_ops > 0) {
    copy->symmetry_operations = (float(*)[4][4])MEM_dupallocN(structure->symmetry_operations);
  }

  return copy;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice Primitives
 * \{ */

QFEAAtomicStructure *QFEA_lattice_create_fcc(int element_id, float lattice_constant)
{
  QFEAAtomicStructure *structure = QFEA_atomic_structure_new(
      QFEA_CRYSTAL_CUBIC, lattice_constant, lattice_constant, lattice_constant, 90.0f, 90.0f, 90.0f, element_id);

  structure->space_group_number = 225;  /* Fm-3m */

  /* FCC has 4 atoms per unit cell */
  structure->num_atoms = 4;
  structure->atom_positions = (QFEAAtomPosition *)MEM_callocN(
      sizeof(QFEAAtomPosition) * 4, "FCC atoms");

  /* Atom positions in fractional coordinates */
  structure->atom_positions[0].element_id = element_id;
  structure->atom_positions[0].frac_coords[0] = 0.0f;
  structure->atom_positions[0].frac_coords[1] = 0.0f;
  structure->atom_positions[0].frac_coords[2] = 0.0f;

  structure->atom_positions[1].element_id = element_id;
  structure->atom_positions[1].frac_coords[0] = 0.5f;
  structure->atom_positions[1].frac_coords[1] = 0.5f;
  structure->atom_positions[1].frac_coords[2] = 0.0f;

  structure->atom_positions[2].element_id = element_id;
  structure->atom_positions[2].frac_coords[0] = 0.5f;
  structure->atom_positions[2].frac_coords[1] = 0.0f;
  structure->atom_positions[2].frac_coords[2] = 0.5f;

  structure->atom_positions[3].element_id = element_id;
  structure->atom_positions[3].frac_coords[0] = 0.0f;
  structure->atom_positions[3].frac_coords[1] = 0.5f;
  structure->atom_positions[3].frac_coords[2] = 0.5f;

  return structure;
}

QFEAAtomicStructure *QFEA_lattice_create_bcc(int element_id, float lattice_constant)
{
  QFEAAtomicStructure *structure = QFEA_atomic_structure_new(
      QFEA_CRYSTAL_CUBIC, lattice_constant, lattice_constant, lattice_constant, 90.0f, 90.0f, 90.0f, element_id);

  structure->space_group_number = 229;  /* Im-3m */

  /* BCC has 2 atoms per unit cell */
  structure->num_atoms = 2;
  structure->atom_positions = (QFEAAtomPosition *)MEM_callocN(
      sizeof(QFEAAtomPosition) * 2, "BCC atoms");

  structure->atom_positions[0].element_id = element_id;
  structure->atom_positions[0].frac_coords[0] = 0.0f;
  structure->atom_positions[0].frac_coords[1] = 0.0f;
  structure->atom_positions[0].frac_coords[2] = 0.0f;

  structure->atom_positions[1].element_id = element_id;
  structure->atom_positions[1].frac_coords[0] = 0.5f;
  structure->atom_positions[1].frac_coords[1] = 0.5f;
  structure->atom_positions[1].frac_coords[2] = 0.5f;

  return structure;
}

QFEAAtomicStructure *QFEA_lattice_create_diamond(int element_id, float lattice_constant)
{
  QFEAAtomicStructure *structure = QFEA_atomic_structure_new(
      QFEA_CRYSTAL_CUBIC, lattice_constant, lattice_constant, lattice_constant, 90.0f, 90.0f, 90.0f, element_id);

  structure->space_group_number = 227;  /* Fd-3m */

  /* Diamond has 8 atoms per unit cell */
  structure->num_atoms = 8;
  structure->atom_positions = (QFEAAtomPosition *)MEM_callocN(
      sizeof(QFEAAtomPosition) * 8, "Diamond atoms");

  /* FCC base + tetrahedral sites */
  const float positions[8][3] = {
      {0.0f, 0.0f, 0.0f},
      {0.5f, 0.5f, 0.0f},
      {0.5f, 0.0f, 0.5f},
      {0.0f, 0.5f, 0.5f},
      {0.25f, 0.25f, 0.25f},
      {0.75f, 0.75f, 0.25f},
      {0.75f, 0.25f, 0.75f},
      {0.25f, 0.75f, 0.75f},
  };

  for (int i = 0; i < 8; i++) {
    structure->atom_positions[i].element_id = element_id;
    copy_v3_v3(structure->atom_positions[i].frac_coords, positions[i]);
  }

  return structure;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Validation
 * \{ */

bool QFEA_material_validate(const QFEAMaterial *material, char **r_error_message)
{
  if (!material) {
    if (r_error_message) {
      *r_error_message = BLI_strdup("Material is null");
    }
    return false;
  }

  /* Check epsilon_r >= 1 */
  if (material->epsilon_r < 1.0f) {
    if (r_error_message) {
      *r_error_message = BLI_strdup("Relative permittivity must be >= 1");
    }
    return false;
  }

  /* Check mu_r > 0 */
  if (material->mu_r <= 0.0f) {
    if (r_error_message) {
      *r_error_message = BLI_strdup("Relative permeability must be > 0");
    }
    return false;
  }

  /* Check sigma >= 0 */
  if (material->sigma < 0.0f) {
    if (r_error_message) {
      *r_error_message = BLI_strdup("Conductivity must be >= 0");
    }
    return false;
  }

  /* Check density >= 0 */
  if (material->density < 0.0f) {
    if (r_error_message) {
      *r_error_message = BLI_strdup("Density must be >= 0");
    }
    return false;
  }

  return true;
}

/** \} */
