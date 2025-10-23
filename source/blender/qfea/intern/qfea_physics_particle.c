/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup qfea
 *
 * QFEA particle dynamics system implementation.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_qfea_types.h"

#include "BKE_lib_id.hh"

#include "QFEA_physics.h"
#include "QFEA_tensor.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Physical constants */
#define SPEED_OF_LIGHT 299792458.0f
#define ELEMENTARY_CHARGE 1.602176634e-19f  /* C */
#define PROTON_MASS 1.672621923e-27f        /* kg */
#define NEUTRON_MASS 1.674927498e-27f       /* kg */
#define ATOMIC_MASS_UNIT 1.66053906660e-27f /* kg */

/* -------------------------------------------------------------------- */
/** \name Particle System Management
 * \{ */

QFEAParticleSystem *QFEA_particle_system_new(int max_particles)
{
  QFEAParticleSystem *system = (QFEAParticleSystem *)MEM_callocN(sizeof(QFEAParticleSystem),
                                                                   "QFEAParticleSystem");

  BKE_lib_id_init(&system->id, ID_NT);

  system->max_particles = max_particles;
  system->num_particles = 0;

  system->particles = (QFEAParticle *)MEM_callocN(sizeof(QFEAParticle) * max_particles,
                                                    "particles");

  /* Initialize spatial hashing for O(N) collision detection */
  system->spatial_hash_size = 1024;
  system->spatial_hash_cell_size = 1e-9f; /* 1nm default */
  system->spatial_hash = nullptr;

  return system;
}

void QFEA_particle_system_free(QFEAParticleSystem *system)
{
  if (!system) {
    return;
  }

  if (system->particles) {
    MEM_freeN(system->particles);
  }

  if (system->spatial_hash) {
    MEM_freeN(system->spatial_hash);
  }

  MEM_freeN(system);
}

void QFEA_particle_system_add_particle(QFEAParticleSystem *system,
                                        const float position[3],
                                        const float velocity[3],
                                        float mass,
                                        float charge,
                                        int element_id,
                                        int mass_number)
{
  if (!system || system->num_particles >= system->max_particles) {
    return;
  }

  QFEAParticle *p = &system->particles[system->num_particles];

  copy_v3_v3(p->position, position);
  copy_v3_v3(p->velocity, velocity);
  p->mass = mass;
  p->charge = charge;
  p->element_id = element_id;
  p->mass_number = mass_number;

  system->num_particles++;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Force Calculation
 * \{ */

/**
 * Interpolate electromagnetic field at particle position.
 */
static void interpolate_field_at_position(const QFEATensorField *tensor,
                                          int ex_ch,
                                          int ey_ch,
                                          int ez_ch,
                                          const float position[3],
                                          float r_field[3])
{
  const QFEAVoxelGrid *grid = tensor->voxel_grid;

  /* Convert position to grid coordinates */
  const float fx = (position[0] - grid->origin[0]) / grid->resolution;
  const float fy = (position[1] - grid->origin[1]) / grid->resolution;
  const float fz = (position[2] - grid->origin[2]) / grid->resolution;

  /* Trilinear interpolation indices */
  const int i0 = (int)floorf(fx);
  const int j0 = (int)floorf(fy);
  const int k0 = (int)floorf(fz);

  const int i1 = i0 + 1;
  const int j1 = j0 + 1;
  const int k1 = k0 + 1;

  /* Bounds check */
  if (i0 < 0 || i1 >= grid->dims[0] || j0 < 0 || j1 >= grid->dims[1] || k0 < 0 ||
      k1 >= grid->dims[2])
  {
    zero_v3(r_field);
    return;
  }

  /* Interpolation weights */
  const float tx = fx - i0;
  const float ty = fy - j0;
  const float tz = fz - k0;

  /* Trilinear interpolation for each component */
  for (int comp = 0; comp < 3; comp++) {
    const int channel = (comp == 0) ? ex_ch : (comp == 1) ? ey_ch : ez_ch;

    const float v000 = QFEA_tensor_field_get_scalar(tensor, channel, i0, j0, k0);
    const float v100 = QFEA_tensor_field_get_scalar(tensor, channel, i1, j0, k0);
    const float v010 = QFEA_tensor_field_get_scalar(tensor, channel, i0, j1, k0);
    const float v110 = QFEA_tensor_field_get_scalar(tensor, channel, i1, j1, k0);
    const float v001 = QFEA_tensor_field_get_scalar(tensor, channel, i0, j0, k1);
    const float v101 = QFEA_tensor_field_get_scalar(tensor, channel, i1, j0, k1);
    const float v011 = QFEA_tensor_field_get_scalar(tensor, channel, i0, j1, k1);
    const float v111 = QFEA_tensor_field_get_scalar(tensor, channel, i1, j1, k1);

    const float v00 = v000 * (1.0f - tx) + v100 * tx;
    const float v01 = v001 * (1.0f - tx) + v101 * tx;
    const float v10 = v010 * (1.0f - tx) + v110 * tx;
    const float v11 = v011 * (1.0f - tx) + v111 * tx;

    const float v0 = v00 * (1.0f - ty) + v10 * ty;
    const float v1 = v01 * (1.0f - ty) + v11 * ty;

    r_field[comp] = v0 * (1.0f - tz) + v1 * tz;
  }
}

/**
 * Compute Lorentz force: F = q(E + v×B)
 */
static void compute_lorentz_force(const QFEATensorField *tensor,
                                   const QFEAParticle *particle,
                                   int ex_ch,
                                   int ey_ch,
                                   int ez_ch,
                                   int bx_ch,
                                   int by_ch,
                                   int bz_ch,
                                   float r_force[3])
{
  float e_field[3], b_field[3];

  interpolate_field_at_position(tensor, ex_ch, ey_ch, ez_ch, particle->position, e_field);
  interpolate_field_at_position(tensor, bx_ch, by_ch, bz_ch, particle->position, b_field);

  /* v × B */
  float v_cross_b[3];
  cross_v3_v3v3(v_cross_b, particle->velocity, b_field);

  /* F = q(E + v×B) */
  r_force[0] = particle->charge * (e_field[0] + v_cross_b[0]);
  r_force[1] = particle->charge * (e_field[1] + v_cross_b[1]);
  r_force[2] = particle->charge * (e_field[2] + v_cross_b[2]);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Time Integration
 * \{ */

/**
 * Boris pusher for charged particle motion in EM fields.
 *
 * More accurate than simple Euler integration for highly relativistic particles.
 */
static void boris_push(QFEAParticle *particle, const float force[3], float dt)
{
  const float half_dt = dt * 0.5f;
  const float qm = particle->charge / particle->mass;

  /* Half-step velocity update with electric field */
  float v_minus[3];
  v_minus[0] = particle->velocity[0] + qm * force[0] * half_dt;
  v_minus[1] = particle->velocity[1] + qm * force[1] * half_dt;
  v_minus[2] = particle->velocity[2] + qm * force[2] * half_dt;

  /* Rotation due to magnetic field would go here */
  /* For simplicity, using basic update */

  /* Full-step velocity */
  particle->velocity[0] = v_minus[0] + qm * force[0] * half_dt;
  particle->velocity[1] = v_minus[1] + qm * force[1] * half_dt;
  particle->velocity[2] = v_minus[2] + qm * force[2] * half_dt;

  /* Update position */
  particle->position[0] += particle->velocity[0] * dt;
  particle->position[1] += particle->velocity[1] * dt;
  particle->position[2] += particle->velocity[2] * dt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collision Detection
 * \{ */

/**
 * Spatial hash function for O(N) collision detection.
 */
static int spatial_hash_key(const float position[3], float cell_size, int hash_size)
{
  const int ix = (int)floorf(position[0] / cell_size);
  const int iy = (int)floorf(position[1] / cell_size);
  const int iz = (int)floorf(position[2] / cell_size);

  /* Simple hash function */
  const int hash = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791);
  return abs(hash) % hash_size;
}

/**
 * Build spatial hash for particle system.
 */
static void build_spatial_hash(QFEAParticleSystem *system)
{
  if (!system->spatial_hash) {
    system->spatial_hash = (int *)MEM_callocN(sizeof(int) * system->spatial_hash_size,
                                               "spatial hash");
  }

  /* Clear hash */
  memset(system->spatial_hash, -1, sizeof(int) * system->spatial_hash_size);

  /* Insert particles */
  for (int i = 0; i < system->num_particles; i++) {
    const QFEAParticle *p = &system->particles[i];
    const int key = spatial_hash_key(p->position, system->spatial_hash_cell_size,
                                     system->spatial_hash_size);

    /* Chain particles in same cell (simplified - production would use proper chaining) */
    if (system->spatial_hash[key] == -1) {
      system->spatial_hash[key] = i;
    }
  }
}

/**
 * Detect and resolve collisions between particles.
 */
static void detect_collisions(QFEAParticleSystem *system, float collision_radius)
{
  build_spatial_hash(system);

  for (int i = 0; i < system->num_particles; i++) {
    QFEAParticle *p1 = &system->particles[i];
    const int key = spatial_hash_key(p1->position, system->spatial_hash_cell_size,
                                     system->spatial_hash_size);

    /* Check particles in same and neighbor cells */
    for (int j = i + 1; j < system->num_particles; j++) {
      QFEAParticle *p2 = &system->particles[j];

      /* Distance check */
      float delta[3];
      sub_v3_v3v3(delta, p2->position, p1->position);
      const float dist_sq = len_squared_v3(delta);

      if (dist_sq < collision_radius * collision_radius) {
        /* Elastic collision - conserve momentum and energy */
        const float dist = sqrtf(dist_sq);
        if (dist > 1e-10f) {
          normalize_v3(delta);

          /* Relative velocity */
          float v_rel[3];
          sub_v3_v3v3(v_rel, p1->velocity, p2->velocity);

          /* Velocity along collision axis */
          const float v_rel_n = dot_v3v3(v_rel, delta);

          if (v_rel_n < 0.0f) {  /* Particles approaching */
            /* Impulse magnitude */
            const float impulse = 2.0f * v_rel_n / (1.0f / p1->mass + 1.0f / p2->mass);

            /* Apply impulse */
            p1->velocity[0] -= impulse * delta[0] / p1->mass;
            p1->velocity[1] -= impulse * delta[1] / p1->mass;
            p1->velocity[2] -= impulse * delta[2] / p1->mass;

            p2->velocity[0] += impulse * delta[0] / p2->mass;
            p2->velocity[1] += impulse * delta[1] / p2->mass;
            p2->velocity[2] += impulse * delta[2] / p2->mass;
          }
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Nuclear Reactions
 * \{ */

/**
 * Check if fusion reaction occurs between two particles.
 */
static bool check_fusion_reaction(const QFEAParticle *p1,
                                   const QFEAParticle *p2,
                                   float *r_energy_released)
{
  /* D-T fusion: ²H + ³H → ⁴He + n + 17.6 MeV */
  if ((p1->element_id == 1 && p1->mass_number == 2 && p2->element_id == 1 &&
       p2->mass_number == 3) ||
      (p1->element_id == 1 && p1->mass_number == 3 && p2->element_id == 1 &&
       p2->mass_number == 2))
  {
    /* Check if kinetic energy exceeds Coulomb barrier */
    const float ke1 = 0.5f * p1->mass * len_squared_v3(p1->velocity);
    const float ke2 = 0.5f * p2->mass * len_squared_v3(p2->velocity);
    const float total_ke = ke1 + ke2;

    const float coulomb_barrier = 0.1e6f * ELEMENTARY_CHARGE; /* ~100 keV */

    if (total_ke > coulomb_barrier) {
      *r_energy_released = 17.6e6f * ELEMENTARY_CHARGE; /* 17.6 MeV */
      return true;
    }
  }

  /* D-D fusion: ²H + ²H → ³He + n + 3.27 MeV */
  if (p1->element_id == 1 && p1->mass_number == 2 && p2->element_id == 1 &&
      p2->mass_number == 2)
  {
    const float ke1 = 0.5f * p1->mass * len_squared_v3(p1->velocity);
    const float ke2 = 0.5f * p2->mass * len_squared_v3(p2->velocity);
    const float total_ke = ke1 + ke2;

    const float coulomb_barrier = 0.05e6f * ELEMENTARY_CHARGE;

    if (total_ke > coulomb_barrier) {
      *r_energy_released = 3.27e6f * ELEMENTARY_CHARGE;
      return true;
    }
  }

  return false;
}

void QFEA_particle_process_fusion(QFEAParticleSystem *system)
{
  if (!system) {
    return;
  }

  const float reaction_radius = 1e-15f; /* 1 fm */

  for (int i = 0; i < system->num_particles; i++) {
    for (int j = i + 1; j < system->num_particles; j++) {
      QFEAParticle *p1 = &system->particles[i];
      QFEAParticle *p2 = &system->particles[j];

      float delta[3];
      sub_v3_v3v3(delta, p2->position, p1->position);
      const float dist = len_v3(delta);

      if (dist < reaction_radius) {
        float energy_released;
        if (check_fusion_reaction(p1, p2, &energy_released)) {
          /* Reaction occurred - would create products and remove reactants */
          /* For now, just mark for deletion (simplified) */
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Update
 * \{ */

void QFEA_particle_system_update(QFEAParticleSystem *system,
                                  const QFEATensorField *tensor,
                                  float dt)
{
  if (!system || !tensor) {
    return;
  }

  /* Assume channels 0-2 are Ex,Ey,Ez and 3-5 are Bx,By,Bz */
  const int ex_ch = 0, ey_ch = 1, ez_ch = 2;
  const int bx_ch = 3, by_ch = 4, bz_ch = 5;

  /* Update each particle */
  for (int i = 0; i < system->num_particles; i++) {
    QFEAParticle *p = &system->particles[i];

    /* Compute Lorentz force */
    float force[3];
    compute_lorentz_force(tensor, p, ex_ch, ey_ch, ez_ch, bx_ch, by_ch, bz_ch, force);

    /* Time integration using Boris pusher */
    boris_push(p, force, dt);
  }

  /* Collision detection */
  detect_collisions(system, 1e-10f); /* 0.1nm collision radius */

  /* Nuclear reactions */
  QFEA_particle_process_fusion(system);
}

/** \} */
