/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA node system API for visual programming workflows.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bNode;
struct bNodeTree;

/* -------------------------------------------------------------------- */
/** \name Node Tree Types
 * \{ */

/**
 * QFEA node tree type identifier.
 */
#define QFEA_NODE_TREE_TYPE "QFEANodeTree"

/**
 * Node socket type identifiers.
 */
enum {
  QFEA_SOCKET_VOXELGRID = 1000,
  QFEA_SOCKET_TENSORFIELD = 1001,
  QFEA_SOCKET_MATERIAL = 1002,
  QFEA_SOCKET_SOURCE = 1003,
  QFEA_SOCKET_BC = 1004,
  QFEA_SOCKET_PHYSICS = 1005,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Categories
 * \{ */

/**
 * Node type identifiers.
 */
enum {
  /* Geometry nodes (1000-1099) */
  QFEA_NODE_VOXELIZE = 1000,
  QFEA_NODE_VOXEL_BOOLEAN = 1001,
  QFEA_NODE_VOXEL_MERGE = 1002,
  QFEA_NODE_VOXEL_TO_MESH = 1003,
  QFEA_NODE_ADAPTIVE_REFINE = 1004,

  /* Material nodes (1100-1199) */
  QFEA_NODE_ASSIGN_MATERIAL = 1100,
  QFEA_NODE_MATERIAL_MIX = 1101,
  QFEA_NODE_LOAD_MATERIAL = 1102,
  QFEA_NODE_MATERIAL_GRADIENT = 1103,

  /* Tensor nodes (1200-1299) */
  QFEA_NODE_CREATE_TENSOR = 1200,
  QFEA_NODE_TENSOR_MATH = 1201,
  QFEA_NODE_TENSOR_COUPLING = 1202,
  QFEA_NODE_TENSOR_TRANSFORM = 1203,
  QFEA_NODE_TENSOR_REDUCE = 1204,

  /* Physics nodes (1300-1399) */
  QFEA_NODE_FDTD_SOLVER = 1300,
  QFEA_NODE_SCHRODINGER_SOLVER = 1301,
  QFEA_NODE_PARTICLE_TRACER = 1302,
  QFEA_NODE_THERMAL_SOLVER = 1303,

  /* Source nodes (1400-1499) */
  QFEA_NODE_VOLTAGE_SOURCE = 1400,
  QFEA_NODE_CURRENT_SOURCE = 1401,
  QFEA_NODE_PLANE_WAVE = 1402,
  QFEA_NODE_POINT_SOURCE = 1403,

  /* BC nodes (1500-1599) */
  QFEA_NODE_PEC_BC = 1500,
  QFEA_NODE_PMC_BC = 1501,
  QFEA_NODE_PML_BC = 1502,
  QFEA_NODE_PERIODIC_BC = 1503,

  /* Compute nodes (1600-1699) */
  QFEA_NODE_DEVICE_SELECTOR = 1600,
  QFEA_NODE_TILE_MANAGER = 1601,
  QFEA_NODE_WORKLOAD_SPLITTER = 1602,
  QFEA_NODE_CHECKPOINT = 1603,

  /* Analysis nodes (1700-1799) */
  QFEA_NODE_FIELD_PROBE = 1700,
  QFEA_NODE_ENERGY_INTEGRATOR = 1701,
  QFEA_NODE_FLUX_CALCULATOR = 1702,
  QFEA_NODE_FFT_ANALYZER = 1703,

  /* Visualization nodes (1800-1899) */
  QFEA_NODE_COLORMAP = 1800,
  QFEA_NODE_VECTOR_FIELD = 1801,
  QFEA_NODE_STREAMLINES = 1802,
  QFEA_NODE_ISOSURFACE = 1803,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Registration
 * \{ */

/**
 * Register all QFEA node types.
 *
 * Called during Blender startup to make nodes available in editor.
 */
void QFEA_register_node_types(void);

/**
 * Unregister QFEA node types.
 */
void QFEA_unregister_node_types(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Execution
 * \{ */

/**
 * Execute QFEA node tree.
 *
 * Performs topological sort and evaluates nodes in dependency order.
 *
 * \param ntree: QFEA node tree
 * \return true if execution successful
 */
bool QFEA_node_tree_execute(struct bNodeTree *ntree);

/**
 * Execute single node.
 *
 * \param node: Node to execute
 * \return true if successful
 */
bool QFEA_node_execute(struct bNode *node);

/** \} */

#ifdef __cplusplus
}
#endif
