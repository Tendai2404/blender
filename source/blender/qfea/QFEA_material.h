/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA material database and atomic structure management API.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Material Database Management
 * \{ */

/**
 * Initialize material database with built-in materials.
 *
 * Loads comprehensive database of validated materials including:
 * - Metals (Cu, Au, Al, Ag, Fe, etc.)
 * - Semiconductors (Si, Ge, GaAs, InP, etc.)
 * - Insulators (SiO2, Al2O3, vacuum, air)
 * - 2D Materials (graphene, MoS2, hBN)
 * - Superconductors (YBCO, NbTi, MgB2)
 */
void QFEA_material_database_init(void);

/**
 * Free material database.
 */
void QFEA_material_database_free(void);

/**
 * Get material by name (case-insensitive).
 *
 * \param name: Material name (e.g., "Copper", "Silicon")
 * \return Material pointer or NULL if not found
 */
QFEAMaterial *QFEA_material_database_get(const char *name);

/**
 * Get material by chemical formula.
 *
 * \param formula: Chemical formula (e.g., "Cu", "SiO2")
 * \return Material pointer or NULL if not found
 */
QFEAMaterial *QFEA_material_database_get_by_formula(const char *formula);

/**
 * Search materials by category.
 *
 * \param category: Category name ("Metal", "Semiconductor", etc.)
 * \param r_count: Number of materials found
 * \return Array of material pointers
 */
QFEAMaterial **QFEA_material_database_search_category(const char *category, int *r_count);

/**
 * Search materials by property range.
 *
 * \param property: Property name ("band_gap", "sigma", etc.)
 * \param min_value: Minimum value
 * \param max_value: Maximum value
 * \param r_count: Number of materials found
 * \return Array of material pointers
 */
QFEAMaterial **QFEA_material_database_search_property(const char *property,
                                                       float min_value,
                                                       float max_value,
                                                       int *r_count);

/**
 * Add custom material to database.
 */
void QFEA_material_database_add(QFEAMaterial *material);

/**
 * Remove material from database.
 */
void QFEA_material_database_remove(const char *name);

/**
 * Get total number of materials in database.
 */
int QFEA_material_database_count(void);

/**
 * List all material names.
 *
 * \param r_count: Number of materials
 * \return Array of material name strings (must be freed)
 */
char **QFEA_material_database_list_names(int *r_count);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Creation
 * \{ */

/**
 * Create new material with default properties.
 */
QFEAMaterial *QFEA_material_new(const char *name);

/**
 * Free material and all associated data.
 */
void QFEA_material_free(QFEAMaterial *material);

/**
 * Copy material.
 */
QFEAMaterial *QFEA_material_copy(const QFEAMaterial *material);

/** \} */

/* -------------------------------------------------------------------- */
/** \name CSV Import/Export
 * \{ */

/**
 * Import materials from CSV file.
 *
 * CSV format:
 * name,formula,epsilon_r,mu_r,sigma,density,band_gap,crystal,a,b,c,alpha,beta,gamma,atoms
 *
 * Atoms field format: "Element:x,y,z;Element:x,y,z;..."
 * Example: "Cu:0,0,0;Cu:0.5,0.5,0;Cu:0.5,0,0.5;Cu:0,0.5,0.5"
 *
 * \param filepath: Path to CSV file
 * \param skip_header: Skip first line
 * \param delimiter: Field delimiter (usually ',')
 * \param r_imported: Number of materials successfully imported
 * \param r_failed: Number of materials that failed validation
 * \param r_error_log: Error messages (must be freed)
 * \return true if import successful
 */
bool QFEA_material_import_csv(const char *filepath,
                               bool skip_header,
                               char delimiter,
                               int *r_imported,
                               int *r_failed,
                               char **r_error_log);

/**
 * Export materials to CSV file.
 *
 * \param filepath: Output CSV path
 * \param materials: Array of materials to export
 * \param count: Number of materials
 * \return true if export successful
 */
bool QFEA_material_export_csv(const char *filepath,
                               QFEAMaterial **materials,
                               int count);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Atomic Structure
 * \{ */

/**
 * Create atomic structure from crystal system and lattice parameters.
 *
 * \param crystal_system: Crystal type (FCC, BCC, HCP, etc.)
 * \param a, b, c: Lattice constants (Angstroms)
 * \param alpha, beta, gamma: Lattice angles (degrees)
 * \param element_id: Atomic number for all atoms
 * \return Atomic structure
 */
QFEAAtomicStructure *QFEA_atomic_structure_new(eQFEACrystalSystem crystal_system,
                                                 float a,
                                                 float b,
                                                 float c,
                                                 float alpha,
                                                 float beta,
                                                 float gamma,
                                                 int element_id);

/**
 * Create custom atomic structure from atom positions.
 *
 * \param atoms: Array of atom positions
 * \param num_atoms: Number of atoms in unit cell
 * \param a, b, c: Lattice constants
 * \param alpha, beta, gamma: Lattice angles
 * \return Atomic structure
 */
QFEAAtomicStructure *QFEA_atomic_structure_custom(QFEAAtomPosition *atoms,
                                                    int num_atoms,
                                                    float a,
                                                    float b,
                                                    float c,
                                                    float alpha,
                                                    float beta,
                                                    float gamma);

/**
 * Free atomic structure.
 */
void QFEA_atomic_structure_free(QFEAAtomicStructure *structure);

/**
 * Copy atomic structure.
 */
QFEAAtomicStructure *QFEA_atomic_structure_copy(const QFEAAtomicStructure *structure);

/**
 * Generate symmetry operations from space group.
 *
 * Uses International Tables of Crystallography to generate all
 * symmetry operations for the given space group number.
 *
 * \param structure: Structure to populate with symmetry ops
 */
void QFEA_atomic_structure_generate_symmetry(QFEAAtomicStructure *structure);

/**
 * Apply symmetry operations to base atoms to generate full unit cell.
 *
 * \param structure: Structure with base atoms
 * \param tolerance: Distance tolerance for duplicate detection (Angstroms)
 */
void QFEA_atomic_structure_apply_symmetry(QFEAAtomicStructure *structure, float tolerance);

/**
 * Convert fractional coordinates to Cartesian.
 *
 * \param structure: Atomic structure with lattice parameters
 * \param frac: Fractional coordinates (0-1)
 * \param r_cart: Output Cartesian coordinates (Angstroms)
 */
void QFEA_atomic_structure_frac_to_cart(const QFEAAtomicStructure *structure,
                                         const float frac[3],
                                         float r_cart[3]);

/**
 * Convert Cartesian coordinates to fractional.
 *
 * \param structure: Atomic structure with lattice parameters
 * \param cart: Cartesian coordinates (Angstroms)
 * \param r_frac: Output fractional coordinates
 */
void QFEA_atomic_structure_cart_to_frac(const QFEAAtomicStructure *structure,
                                         const float cart[3],
                                         float r_frac[3]);

/**
 * Calculate density from atomic structure.
 *
 * Uses atomic masses and unit cell volume.
 *
 * \param structure: Atomic structure
 * \return Density in kg/m³
 */
float QFEA_atomic_structure_calculate_density(const QFEAAtomicStructure *structure);

/**
 * Calculate average coordination number.
 *
 * \param structure: Atomic structure
 * \param cutoff_distance: Maximum bond distance (Angstroms)
 * \return Average coordination number
 */
float QFEA_atomic_structure_calculate_coordination(const QFEAAtomicStructure *structure,
                                                     float cutoff_distance);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice Generation
 * \{ */

/**
 * Generate atom positions for repeated unit cells.
 *
 * Creates 3D array of atoms by tiling the unit cell.
 *
 * \param structure: Unit cell structure
 * \param repeat_x, repeat_y, repeat_z: Number of repetitions
 * \param r_positions: Output atom positions (Cartesian, Angstroms)
 * \param r_elements: Output element IDs
 * \param r_count: Total number of atoms
 */
void QFEA_lattice_generate_atoms(const QFEAAtomicStructure *structure,
                                  int repeat_x,
                                  int repeat_y,
                                  int repeat_z,
                                  float (**r_positions)[3],
                                  int **r_elements,
                                  int *r_count);

/**
 * Generate bonds between atoms based on distance.
 *
 * Uses spatial hashing for O(N) performance.
 *
 * \param positions: Atom positions (Angstroms)
 * \param elements: Element IDs
 * \param count: Number of atoms
 * \param threshold: Maximum bond distance (Angstroms)
 * \param r_bonds: Output bond pairs (atom indices)
 * \param r_bond_count: Number of bonds
 */
void QFEA_lattice_generate_bonds(const float (*positions)[3],
                                  const int *elements,
                                  int count,
                                  float threshold,
                                  int (**r_bonds)[2],
                                  int *r_bond_count);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Atomic Primitives
 * \{ */

/**
 * Create FCC (face-centered cubic) structure.
 *
 * \param element_id: Atomic number
 * \param lattice_constant: Lattice parameter a (Angstroms)
 * \return FCC structure with 4 atoms per unit cell
 */
QFEAAtomicStructure *QFEA_lattice_create_fcc(int element_id, float lattice_constant);

/**
 * Create BCC (body-centered cubic) structure.
 */
QFEAAtomicStructure *QFEA_lattice_create_bcc(int element_id, float lattice_constant);

/**
 * Create HCP (hexagonal close-packed) structure.
 */
QFEAAtomicStructure *QFEA_lattice_create_hcp(int element_id, float a, float c);

/**
 * Create diamond structure (like Silicon, Carbon).
 */
QFEAAtomicStructure *QFEA_lattice_create_diamond(int element_id, float lattice_constant);

/**
 * Create graphene sheet structure.
 *
 * \param lattice_constant: C-C distance (typically 1.42 Angstroms)
 * \return 2D hexagonal structure
 */
QFEAAtomicStructure *QFEA_lattice_create_graphene(float lattice_constant);

/**
 * Create simple cubic structure.
 */
QFEAAtomicStructure *QFEA_lattice_create_simple_cubic(int element_id, float lattice_constant);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Defects and Doping
 * \{ */

/**
 * Create vacancy defect at specified atom.
 *
 * \param positions: Atom positions (modified in place)
 * \param elements: Element IDs (modified in place)
 * \param count: Number of atoms (decremented)
 * \param vacancy_index: Index of atom to remove
 */
void QFEA_lattice_create_vacancy(float (**positions)[3],
                                  int **elements,
                                  int *count,
                                  int vacancy_index);

/**
 * Create interstitial defect at position.
 *
 * \param positions: Atom positions (reallocated)
 * \param elements: Element IDs (reallocated)
 * \param count: Number of atoms (incremented)
 * \param position: Position for interstitial atom
 * \param element_id: Element to insert
 */
void QFEA_lattice_create_interstitial(float (**positions)[3],
                                        int **elements,
                                        int *count,
                                        const float position[3],
                                        int element_id);

/**
 * Create substitutional defect (replace atom with different element).
 *
 * \param elements: Element IDs (modified in place)
 * \param substitution_index: Index of atom to substitute
 * \param new_element_id: Replacement element
 */
void QFEA_lattice_create_substitution(int *elements,
                                        int substitution_index,
                                        int new_element_id);

/**
 * Apply uniform doping to structure.
 *
 * Randomly substitutes host atoms with dopant atoms.
 *
 * \param elements: Element IDs (modified in place)
 * \param count: Number of atoms
 * \param host_element: Element to replace
 * \param dopant_element: Replacement element
 * \param concentration: Doping concentration (atoms/cm³ or percentage)
 * \param use_percentage: If true, concentration is percentage (0-100)
 * \param seed: Random seed for reproducibility
 */
void QFEA_lattice_apply_doping(int *elements,
                                int count,
                                int host_element,
                                int dopant_element,
                                float concentration,
                                bool use_percentage,
                                unsigned int seed);

/**
 * Apply doping with spatial gradient.
 *
 * \param positions: Atom positions
 * \param elements: Element IDs (modified in place)
 * \param count: Number of atoms
 * \param host_element: Element to replace
 * \param dopant_element: Replacement element
 * \param concentration_function: Function pointer for concentration(position)
 * \param seed: Random seed
 */
typedef float (*QFEAConcentrationFunc)(const float position[3], void *userdata);

void QFEA_lattice_apply_doping_gradient(const float (*positions)[3],
                                         int *elements,
                                         int count,
                                         int host_element,
                                         int dopant_element,
                                         QFEAConcentrationFunc concentration_func,
                                         void *userdata,
                                         unsigned int seed);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Get element name from atomic number.
 *
 * \param atomic_number: Z (1-118)
 * \return Element symbol (e.g., "H", "He", "C")
 */
const char *QFEA_element_get_symbol(int atomic_number);

/**
 * Get atomic number from element symbol.
 *
 * \param symbol: Element symbol (case-insensitive)
 * \return Atomic number or -1 if invalid
 */
int QFEA_element_get_atomic_number(const char *symbol);

/**
 * Get atomic mass in atomic mass units (u).
 *
 * \param atomic_number: Z
 * \return Atomic mass (u)
 */
float QFEA_element_get_mass(int atomic_number);

/**
 * Get covalent radius in Angstroms.
 *
 * \param atomic_number: Z
 * \return Covalent radius (Å)
 */
float QFEA_element_get_covalent_radius(int atomic_number);

/**
 * Get CPK color for element (for visualization).
 *
 * \param atomic_number: Z
 * \param r_color: Output RGB color (0-1)
 */
void QFEA_element_get_color(int atomic_number, float r_color[3]);

/**
 * Validate material properties.
 *
 * Checks physical constraints (epsilon_r >= 1, sigma >= 0, etc.)
 *
 * \param material: Material to validate
 * \param r_error_message: Output error message if validation fails
 * \return true if valid
 */
bool QFEA_material_validate(const QFEAMaterial *material, char **r_error_message);

/** \} */

#ifdef __cplusplus
}
#endif
