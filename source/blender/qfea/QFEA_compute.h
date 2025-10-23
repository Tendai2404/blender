/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA compute orchestration API for distributed multi-device execution.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Compute Session Management
 * \{ */

/**
 * Create new compute session.
 *
 * Enumerates available devices (CPU, CUDA, OpenCL, Metal) and initializes
 * infrastructure for distributed computation.
 */
QFEAComputeSession *QFEA_compute_session_new(void);

/**
 * Free compute session and all associated resources.
 */
void QFEA_compute_session_free(QFEAComputeSession *session);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Device Management
 * \{ */

/**
 * Enumerate all available compute devices.
 *
 * Discovers:
 * - CPU cores
 * - CUDA GPUs (if CUDA toolkit available)
 * - OpenCL devices (AMD, Intel, NVIDIA via OpenCL)
 * - Metal devices (Apple Silicon/AMD on macOS)
 *
 * \param session: Compute session
 * \return Number of devices found
 */
int QFEA_compute_enumerate_devices(QFEAComputeSession *session);

/**
 * Get device information.
 *
 * \param session: Compute session
 * \param device_id: Device index
 * \return Device info structure or NULL if invalid
 */
const QFEAComputeDevice *QFEA_compute_get_device(const QFEAComputeSession *session,
                                                   int device_id);

/**
 * Check if two GPUs support peer-to-peer memory access.
 *
 * If true, enables direct GPU-to-GPU copy without staging through host.
 *
 * \param session: Compute session
 * \param device_id1, device_id2: Device indices
 * \return true if P2P supported
 */
bool QFEA_compute_devices_can_access_peer(const QFEAComputeSession *session,
                                           int device_id1,
                                           int device_id2);

/**
 * Enable peer-to-peer access between GPUs.
 *
 * Must call before using direct GPU-GPU transfers.
 */
bool QFEA_compute_enable_peer_access(QFEAComputeSession *session,
                                      int device_id1,
                                      int device_id2);

/**
 * Query device utilization (0-1).
 *
 * \param session: Compute session
 * \param device_id: Device index
 * \return Current utilization (0=idle, 1=fully utilized)
 */
float QFEA_compute_get_device_utilization(const QFEAComputeSession *session, int device_id);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tiling and Domain Decomposition
 * \{ */

/**
 * Create spatial tiles for distributed computation.
 *
 * Divides voxel grid into manageable chunks based on strategy:
 * - BRICKS: Uniform 3D blocks
 * - SLABS: Z-axis slices
 * - ADAPTIVE: Size based on workload estimation
 *
 * \param session: Compute session
 * \param voxel_grid: Spatial grid to partition
 * \param strategy: Tiling strategy ("BRICKS", "SLABS", "ADAPTIVE")
 * \param tile_size: Desired tile dimensions (for BRICKS)
 * \return Number of tiles created
 */
int QFEA_compute_create_tiles(QFEAComputeSession *session,
                               const QFEAVoxelGrid *voxel_grid,
                               const char *strategy,
                               const int tile_size[3]);

/**
 * Assign tiles to compute devices.
 *
 * Strategies:
 * - AUTO: Balance based on device performance and memory
 * - MANUAL: User-specified device affinity
 * - ROUND_ROBIN: Distribute evenly
 *
 * \param session: Compute session
 * \param strategy: Assignment strategy
 * \param device_affinity: Optional manual assignment (tile_id -> device_id)
 */
void QFEA_compute_assign_tiles(QFEAComputeSession *session,
                                const char *strategy,
                                const int *device_affinity);

/**
 * Get tile assignment for device.
 *
 * \param session: Compute session
 * \param device_id: Device index
 * \param r_tile_indices: Output array of tile indices
 * \return Number of tiles assigned to device
 */
int QFEA_compute_get_device_tiles(const QFEAComputeSession *session,
                                   int device_id,
                                   int **r_tile_indices);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Distributed Execution
 * \{ */

/**
 * Execute voxelization across all devices.
 *
 * Parallelizes mesh-to-voxel conversion by assigning spatial regions to devices.
 *
 * \param session: Compute session
 * \param voxel_grid: Output voxel grid
 * \param mesh: Input mesh
 * \param resolution: Voxel size
 */
void QFEA_compute_distribute_voxelize(QFEAComputeSession *session,
                                       QFEAVoxelGrid *voxel_grid,
                                       const struct Mesh *mesh,
                                       float resolution);

/**
 * Execute physics timestep across all devices.
 *
 * Distributes computation based on tile assignment:
 * 1. Each device processes its tiles
 * 2. Exchange halo data at tile boundaries
 * 3. Synchronize before next step
 *
 * \param session: Compute session
 * \param tensor: Tensor field to evolve
 * \param physics: Physics configuration
 * \param dt: Timestep
 */
void QFEA_compute_distribute_physics_step(QFEAComputeSession *session,
                                           QFEATensorField *tensor,
                                           const QFEAPhysicsModule *physics,
                                           float dt);

/**
 * Gather results from all devices to master.
 *
 * Collects tensor data from all devices and assembles into single array.
 *
 * \param session: Compute session
 * \param tensor: Tensor field to update
 */
void QFEA_compute_gather_results(QFEAComputeSession *session, QFEATensorField *tensor);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Halo Exchange
 * \{ */

/**
 * Exchange boundary data between neighboring tiles.
 *
 * Ghost cell exchange for stencil operations:
 * 1. Pack edge voxels into send buffer
 * 2. Send to neighbor device
 * 3. Receive from neighbor
 * 4. Unpack into halo region
 *
 * \param session: Compute session
 * \param tensor: Tensor field
 * \param halo_width: Number of ghost cells (typically 1-2)
 */
void QFEA_compute_exchange_halos(QFEAComputeSession *session,
                                  QFEATensorField *tensor,
                                  int halo_width);

/**
 * Asynchronous halo exchange (non-blocking).
 *
 * Returns immediately, allowing overlap of communication and computation.
 *
 * \return Request handle for later synchronization
 */
struct QFEAComputeRequest *QFEA_compute_exchange_halos_async(QFEAComputeSession *session,
                                                               QFEATensorField *tensor,
                                                               int halo_width);

/**
 * Wait for asynchronous operation to complete.
 */
void QFEA_compute_request_wait(struct QFEAComputeRequest *request);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load Balancing
 * \{ */

/**
 * Measure performance of current tile assignment.
 *
 * Tracks time per tile for load imbalance detection.
 *
 * \param session: Compute session
 */
void QFEA_compute_measure_performance(QFEAComputeSession *session);

/**
 * Rebalance tile assignment to minimize idle time.
 *
 * Redistributes tiles based on measured performance:
 * - Split slow tiles
 * - Merge fast tiles
 * - Reassign to faster devices
 *
 * \param session: Compute session
 * \return true if rebalancing performed
 */
bool QFEA_compute_rebalance(QFEAComputeSession *session);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Network Communication
 * \{ */

/**
 * Remote compute node (for cluster computing).
 */
typedef struct QFEARemoteNode {
  struct QFEARemoteNode *next, *prev;

  char hostname[256];
  int port;
  int socket_fd;  /* TCP socket */

  int num_devices;
  QFEAComputeDevice *devices;

  bool is_connected;
  double last_heartbeat;

  /* Statistics */
  size_t bytes_sent;
  size_t bytes_received;
  double total_compute_time;
} QFEARemoteNode;

/**
 * Add remote compute node to session.
 *
 * Connects to remote QFEA compute server via TCP.
 *
 * \param session: Compute session
 * \param hostname: Remote host address
 * \param port: TCP port (default 9000)
 * \return Remote node pointer or NULL if connection failed
 */
QFEARemoteNode *QFEA_compute_add_remote_node(QFEAComputeSession *session,
                                               const char *hostname,
                                               int port);

/**
 * Remove remote node.
 */
void QFEA_compute_remove_remote_node(QFEAComputeSession *session, QFEARemoteNode *node);

/**
 * Send tile data to remote node.
 *
 * \param node: Remote node
 * \param tile: Tile to send
 * \param tensor: Tensor field data
 * \param compression: Use LZ4/ZSTD compression
 */
void QFEA_compute_send_tile(QFEARemoteNode *node,
                              const QFEAComputeTile *tile,
                              const QFEATensorField *tensor,
                              bool compression);

/**
 * Receive results from remote node.
 */
void QFEA_compute_receive_tile_results(QFEARemoteNode *node,
                                         QFEAComputeTile *tile,
                                         QFEATensorField *tensor);

/**
 * Barrier synchronization across all nodes.
 *
 * Blocks until all nodes reach barrier.
 */
void QFEA_compute_barrier_all(QFEAComputeSession *session);

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU-Specific Functions
 * \{ */

/**
 * Launch CUDA kernel on device.
 *
 * \param session: Compute session
 * \param device_id: CUDA device
 * \param kernel_name: Kernel function name
 * \param grid: Grid dimensions
 * \param block: Block dimensions
 * \param args: Kernel arguments
 * \param num_args: Number of arguments
 */
void QFEA_compute_launch_cuda_kernel(QFEAComputeSession *session,
                                      int device_id,
                                      const char *kernel_name,
                                      const int grid[3],
                                      const int block[3],
                                      void **args,
                                      int num_args);

/**
 * Launch OpenCL kernel on device.
 */
void QFEA_compute_launch_opencl_kernel(QFEAComputeSession *session,
                                        int device_id,
                                        const char *kernel_name,
                                        const size_t global_work_size[3],
                                        const size_t local_work_size[3],
                                        void **args,
                                        int num_args);

/**
 * Allocate GPU memory.
 *
 * \param session: Compute session
 * \param device_id: Device index
 * \param size: Bytes to allocate
 * \return Device pointer
 */
void *QFEA_compute_device_alloc(QFEAComputeSession *session, int device_id, size_t size);

/**
 * Free GPU memory.
 */
void QFEA_compute_device_free(QFEAComputeSession *session, int device_id, void *device_ptr);

/**
 * Copy host to device.
 */
void QFEA_compute_memcpy_h2d(QFEAComputeSession *session,
                              int device_id,
                              void *device_ptr,
                              const void *host_ptr,
                              size_t size);

/**
 * Copy device to host.
 */
void QFEA_compute_memcpy_d2h(QFEAComputeSession *session,
                              int device_id,
                              void *host_ptr,
                              const void *device_ptr,
                              size_t size);

/**
 * Copy device to device (peer-to-peer if available).
 */
void QFEA_compute_memcpy_d2d(QFEAComputeSession *session,
                              int src_device,
                              const void *src_ptr,
                              int dst_device,
                              void *dst_ptr,
                              size_t size);

/**
 * Synchronize device (wait for all operations to complete).
 */
void QFEA_compute_device_synchronize(QFEAComputeSession *session, int device_id);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Performance Monitoring
 * \{ */

/**
 * Performance statistics.
 */
typedef struct QFEAComputeStats {
  double total_time;
  double compute_time;
  double communication_time;
  double idle_time;

  size_t memory_allocated_host;
  size_t memory_allocated_device;

  int num_kernel_launches;
  int num_data_transfers;

  /* Per-device breakdown */
  float device_utilization[16];  /* One per device */
  double device_time[16];
} QFEAComputeStats;

/**
 * Query performance statistics.
 */
void QFEA_compute_get_stats(const QFEAComputeSession *session, QFEAComputeStats *r_stats);

/**
 * Reset performance counters.
 */
void QFEA_compute_reset_stats(QFEAComputeSession *session);

/**
 * Print performance report to console.
 */
void QFEA_compute_print_stats(const QFEAComputeSession *session);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Estimate computation time for current configuration.
 *
 * Based on voxel count, active solvers, and device performance.
 *
 * \param session: Compute session
 * \param voxel_grid: Spatial grid
 * \param physics: Physics configuration
 * \param num_timesteps: Number of steps to simulate
 * \return Estimated time (seconds)
 */
float QFEA_compute_estimate_runtime(const QFEAComputeSession *session,
                                     const QFEAVoxelGrid *voxel_grid,
                                     const QFEAPhysicsModule *physics,
                                     int num_timesteps);

/**
 * Check if configuration exceeds available memory.
 *
 * \return true if out of memory, false if fits
 */
bool QFEA_compute_check_memory_overflow(const QFEAComputeSession *session,
                                         const QFEAVoxelGrid *voxel_grid,
                                         const QFEAPhysicsModule *physics);

/** \} */

#ifdef __cplusplus
}
#endif
