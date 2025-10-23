/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA physics solver APIs for electromagnetic, quantum, and particle simulations.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Physics Module Management
 * \{ */

/**
 * Create new physics module.
 */
QFEAPhysicsModule *QFEA_physics_new(void);

/**
 * Free physics module.
 */
void QFEA_physics_free(QFEAPhysicsModule *physics);

/**
 * Reset physics state to initial conditions.
 */
void QFEA_physics_reset(QFEAPhysicsModule *physics);

/** \} */

/* -------------------------------------------------------------------- */
/** \name FDTD Electromagnetic Solver
 * \{ */

/**
 * Initialize FDTD solver for electromagnetic simulation.
 *
 * Sets up Yee grid with staggered E and H fields:
 * - Ex at (i+1/2, j, k)
 * - Ey at (i, j+1/2, k)
 * - Ez at (i, j, k+1/2)
 * - Hx at (i, j+1/2, k+1/2)
 * - Hy at (i+1/2, j, k+1/2)
 * - Hz at (i+1/2, j+1/2, k)
 *
 * \param tensor: Tensor field with E and H channels
 * \param config: FDTD configuration
 */
void QFEA_fdtd_init(QFEATensorField *tensor, const QFEAEMConfig *config);

/**
 * Execute single FDTD timestep.
 *
 * Updates:
 * 1. H field from curl of E (half timestep)
 * 2. E field from curl of H (full timestep)
 * 3. Apply sources
 * 4. Apply boundary conditions (PEC, PMC, PML)
 *
 * \param tensor: Tensor field containing E and H
 * \param materials: Material properties per voxel
 * \param sources: Active energy sources
 * \param bcs: Boundary conditions
 * \param dt: Timestep (must satisfy CFL condition)
 */
void QFEA_fdtd_step(QFEATensorField *tensor,
                     const QFEAMaterial **materials,
                     const ListBase *sources,
                     const ListBase *bcs,
                     float dt);

/**
 * Compute CFL stability timestep.
 *
 * dt ≤ 1/(c√(1/dx² + 1/dy² + 1/dz²))
 *
 * \param voxel_grid: Spatial grid
 * \param materials: Material properties (for max wave speed)
 * \return Maximum stable timestep
 */
float QFEA_fdtd_compute_cfl_timestep(const QFEAVoxelGrid *voxel_grid,
                                      const QFEAMaterial **materials);

/**
 * Update E field from curl of H.
 *
 * Ex^(n+1) = Ex^n + (dt/ε) * (curl H)_x - (σ·dt/ε) * Ex^n
 */
void QFEA_fdtd_update_E(QFEATensorField *tensor,
                         const float *epsilon,
                         const float *mu,
                         const float *sigma,
                         float dt);

/**
 * Update H field from curl of E.
 *
 * Hx^(n+1/2) = Hx^(n-1/2) + (dt/μ) * (curl E)_x
 */
void QFEA_fdtd_update_H(QFEATensorField *tensor,
                         const float *epsilon,
                         const float *mu,
                         float dt);

/**
 * Apply perfectly matched layer (PML) absorbing boundary.
 *
 * Implements split-field PML to prevent reflections at domain boundaries.
 * Achieves < -60dB reflection (0.1% energy return).
 *
 * \param tensor: Tensor field
 * \param bc: PML boundary condition
 * \param thickness: Number of PML layers
 * \param attenuation: Attenuation coefficient
 */
void QFEA_fdtd_apply_pml(QFEATensorField *tensor,
                          const QFEABoundaryCondition *bc,
                          int thickness,
                          float attenuation);

/**
 * Apply perfect electric conductor boundary.
 *
 * Sets tangential E = 0 at surface.
 */
void QFEA_fdtd_apply_pec(QFEATensorField *tensor, const QFEABoundaryCondition *bc);

/**
 * Apply periodic boundary condition.
 *
 * Copies field values: E(x=0) = E(x=L).
 */
void QFEA_fdtd_apply_periodic(QFEATensorField *tensor, const QFEABoundaryCondition *bc);

/**
 * Apply energy source.
 *
 * \param source: Source definition (voltage, current, plane wave, etc.)
 * \param time: Current simulation time
 */
void QFEA_fdtd_apply_source(QFEATensorField *tensor, const QFEAEnergySource *source, float time);

/**
 * Compute material dispersion (Drude/Lorentz model).
 *
 * Updates auxiliary polarization fields for frequency-dependent materials.
 *
 * \param tensor: Tensor field
 * \param material: Material with dispersion model
 * \param dt: Timestep
 */
void QFEA_fdtd_update_dispersion(QFEATensorField *tensor,
                                  const QFEAMaterial *material,
                                  float dt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Schrödinger Quantum Solver
 * \{ */

/**
 * Initialize Schrödinger solver.
 *
 * Sets up split-operator method for time evolution:
 * ψ(t+dt) = exp(-iV dt/(2ℏ)) · F⁻¹[exp(-iK dt/ℏ)] · F · exp(-iV dt/(2ℏ)) · ψ(t)
 *
 * \param tensor: Tensor field with complex wavefunction channel
 * \param config: Quantum solver configuration
 */
void QFEA_schrodinger_init(QFEATensorField *tensor, const QFEAQuantumConfig *config);

/**
 * Execute single Schrödinger timestep (split-operator method).
 *
 * Steps:
 * 1. Apply potential operator (half step): ψ *= exp(-iV dt/(2ℏ))
 * 2. FFT to momentum space
 * 3. Apply kinetic operator: ψ̃ *= exp(-iK dt/ℏ) where K = ℏ²k²/(2m)
 * 4. IFFT back to position space
 * 5. Apply potential operator (half step)
 * 6. Normalize wavefunction
 *
 * \param tensor: Tensor field with wavefunction
 * \param potential: Potential energy V(x,y,z) per voxel
 * \param config: Solver configuration
 * \param dt: Timestep
 */
void QFEA_schrodinger_step(QFEATensorField *tensor,
                            const float *potential,
                            const QFEAQuantumConfig *config,
                            float dt);

/**
 * Apply potential operator: ψ *= exp(-iV dt/ℏ).
 */
void QFEA_schrodinger_apply_potential(QFEATensorField *tensor,
                                       const float *potential,
                                       float dt,
                                       float hbar);

/**
 * Apply kinetic operator in momentum space: ψ̃ *= exp(-iK dt/ℏ).
 */
void QFEA_schrodinger_apply_kinetic(QFEATensorField *tensor,
                                     const QFEAVoxelGrid *voxel_grid,
                                     float mass,
                                     float dt,
                                     float hbar);

/**
 * Normalize wavefunction: ∫|ψ|² dV = 1.
 */
void QFEA_schrodinger_normalize(QFEATensorField *tensor);

/**
 * Compute probability density: ρ = |ψ|².
 *
 * \param wavefunction_channel: Complex wavefunction
 * \param density_channel: Output probability density (real)
 */
void QFEA_schrodinger_compute_density(QFEATensorField *tensor,
                                       int wavefunction_channel,
                                       int density_channel);

/**
 * Compute expectation value of energy: <E> = <ψ|Ĥ|ψ>.
 *
 * \param tensor: Tensor field with wavefunction
 * \param potential: Potential energy per voxel
 * \param mass: Particle mass
 * \param hbar: Reduced Planck constant
 * \return Energy expectation value
 */
float QFEA_schrodinger_compute_energy(const QFEATensorField *tensor,
                                       const float *potential,
                                       float mass,
                                       float hbar);

/**
 * Compute coherence: C = |<ψ|ψ_initial>|².
 *
 * \param tensor: Current wavefunction
 * \param initial: Initial wavefunction
 * \return Coherence (0-1)
 */
float QFEA_schrodinger_compute_coherence(const QFEATensorField *tensor,
                                          const QFEATensorField *initial);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle Physics Solver
 * \{ */

/**
 * Particle container for tracking individual charged particles.
 */
typedef struct QFEAParticle {
  float position[3];    /* meters */
  float velocity[3];    /* m/s */
  float mass;           /* kg */
  float charge;         /* Coulombs */
  int element_id;       /* Atomic number */
  int mass_number;      /* A (nucleon count) */
  float energy;         /* Total energy (kinetic + rest) */
  float spin;           /* Intrinsic angular momentum */
  int id;               /* Unique identifier */
  bool active;          /* false if decayed/absorbed */
} QFEAParticle;

/**
 * Particle system container.
 */
typedef struct QFEAParticleSystem {
  QFEAParticle *particles;
  int num_particles;
  int max_particles;

  /* Spatial acceleration structure */
  struct SpatialHash *spatial_hash;

  /* Physics parameters */
  float coulomb_constant;
  float collision_threshold;

  /* Nuclear reactions */
  bool enable_fusion;
  bool enable_fission;
  struct NuclearDatabase *nuclear_db;

  /* Statistics */
  int num_collisions;
  int num_fusions;
  int num_fissions;
} QFEAParticleSystem;

/**
 * Create new particle system.
 */
QFEAParticleSystem *QFEA_particle_system_new(int max_particles);

/**
 * Free particle system.
 */
void QFEA_particle_system_free(QFEAParticleSystem *system);

/**
 * Add particle to system.
 *
 * \return Particle index
 */
int QFEA_particle_system_add(QFEAParticleSystem *system,
                              const float position[3],
                              const float velocity[3],
                              float mass,
                              float charge,
                              int element_id,
                              int mass_number);

/**
 * Remove particle from system.
 */
void QFEA_particle_system_remove(QFEAParticleSystem *system, int particle_index);

/**
 * Update particle positions and velocities using Lorentz force.
 *
 * Equation of motion: F = q(E + v×B)
 * Integration: Leapfrog method for energy conservation
 *
 * \param system: Particle system
 * \param tensor: Tensor field with E and H
 * \param dt: Timestep
 */
void QFEA_particle_system_update(QFEAParticleSystem *system,
                                  const QFEATensorField *tensor,
                                  float dt);

/**
 * Detect collisions between particles.
 *
 * Uses spatial hashing for O(N) performance.
 * Marks colliding particles for fusion/fission processing.
 */
void QFEA_particle_detect_collisions(QFEAParticleSystem *system);

/**
 * Process fusion reactions.
 *
 * Checks:
 * 1. Kinetic energy > Coulomb barrier
 * 2. Reaction exists in nuclear database
 * 3. Probabilistic reaction rate
 *
 * Creates product particles, removes reactants.
 */
void QFEA_particle_process_fusion(QFEAParticleSystem *system);

/**
 * Process fission reactions.
 *
 * For heavy nuclei above fission threshold energy.
 */
void QFEA_particle_process_fission(QFEAParticleSystem *system);

/**
 * Compute Coulomb barrier for two particles.
 *
 * V_barrier = k * Z1 * Z2 * e² / (r1 + r2)
 *
 * \param p1, p2: Particles
 * \return Barrier energy (eV)
 */
float QFEA_particle_coulomb_barrier(const QFEAParticle *p1, const QFEAParticle *p2);

/**
 * Render particles to voxel grid (for density field coupling).
 *
 * \param system: Particle system
 * \param tensor: Tensor field to write particle density
 * \param density_channel: Channel for particle density
 */
void QFEA_particle_render_to_grid(const QFEAParticleSystem *system,
                                   QFEATensorField *tensor,
                                   int density_channel);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Coupled Solvers
 * \{ */

/**
 * Execute coupled EM + Quantum simulation.
 *
 * Weak coupling:
 * 1. FDTD computes E, H fields
 * 2. Schrödinger uses E as external potential: V(x) = -e·E·x
 * 3. No back-reaction (ψ doesn't affect EM)
 *
 * \param tensor: Tensor field with E, H, ψ channels
 * \param physics: Physics module configuration
 * \param dt: Timestep
 */
void QFEA_coupled_em_quantum_step(QFEATensorField *tensor,
                                   const QFEAPhysicsModule *physics,
                                   float dt);

/**
 * Execute coupled EM + Particle simulation.
 *
 * 1. FDTD computes E, H fields
 * 2. Particles evolve via Lorentz force
 * 3. Particles can source EM field (charge density ρ, current J)
 *
 * \param tensor: EM field
 * \param particles: Particle system
 * \param physics: Configuration
 * \param dt: Timestep
 */
void QFEA_coupled_em_particle_step(QFEATensorField *tensor,
                                    QFEAParticleSystem *particles,
                                    const QFEAPhysicsModule *physics,
                                    float dt);

/**
 * Execute fully coupled EM + Quantum + Particle simulation.
 *
 * Strong coupling with iterative solve for consistency.
 */
void QFEA_coupled_full_step(QFEATensorField *tensor,
                             QFEAParticleSystem *particles,
                             const QFEAPhysicsModule *physics,
                             float dt,
                             int max_iterations,
                             float tolerance);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Nuclear Database
 * \{ */

/**
 * Nuclear reaction entry.
 */
typedef struct QFEANuclearReaction {
  /* Reactants */
  int reactant1_Z;     /* Atomic number */
  int reactant1_A;     /* Mass number */
  int reactant2_Z;
  int reactant2_A;

  /* Products */
  int num_products;
  struct {
    int Z;
    int A;
    float kinetic_energy; /* MeV, in CM frame */
  } products[4];        /* Max 4 products */

  /* Energetics */
  float Q_value;        /* Energy released (MeV) */
  float threshold_energy; /* Minimum kinetic energy (keV) */

  /* Cross-section (energy-dependent) */
  int num_cross_section_points;
  struct {
    float energy;       /* keV */
    float cross_section; /* barns */
  } *cross_section_data;
} QFEANuclearReaction;

/**
 * Load nuclear database from file.
 *
 * \param filepath: Database file path (JSON or binary)
 * \return Nuclear database
 */
struct NuclearDatabase *QFEA_nuclear_database_load(const char *filepath);

/**
 * Free nuclear database.
 */
void QFEA_nuclear_database_free(struct NuclearDatabase *db);

/**
 * Query reaction by reactants.
 *
 * \return Reaction pointer or NULL if not found
 */
const QFEANuclearReaction *QFEA_nuclear_database_query(const struct NuclearDatabase *db,
                                                        int Z1,
                                                        int A1,
                                                        int Z2,
                                                        int A2);

/**
 * Compute reaction cross-section at given energy.
 *
 * Interpolates from tabulated data.
 *
 * \param reaction: Nuclear reaction
 * \param energy: Kinetic energy (keV)
 * \return Cross-section (barns)
 */
float QFEA_nuclear_reaction_cross_section(const QFEANuclearReaction *reaction, float energy);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Validate timestep against CFL condition for all active solvers.
 *
 * \param physics: Physics module
 * \param voxel_grid: Spatial grid
 * \param materials: Material properties
 * \return Maximum stable timestep
 */
float QFEA_physics_validate_timestep(const QFEAPhysicsModule *physics,
                                      const QFEAVoxelGrid *voxel_grid,
                                      const QFEAMaterial **materials);

/**
 * Estimate memory requirements for simulation.
 *
 * \param voxel_grid: Spatial grid
 * \param physics: Physics configuration
 * \return Estimated memory (bytes)
 */
size_t QFEA_physics_estimate_memory(const QFEAVoxelGrid *voxel_grid,
                                     const QFEAPhysicsModule *physics);

/**
 * Estimate computation time for single timestep.
 *
 * Based on voxel count, active solvers, and hardware.
 *
 * \return Estimated time per step (seconds)
 */
float QFEA_physics_estimate_step_time(const QFEAVoxelGrid *voxel_grid,
                                       const QFEAPhysicsModule *physics);

/** \} */

#ifdef __cplusplus
}
#endif
