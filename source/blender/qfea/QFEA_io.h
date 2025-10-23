/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA file I/O API for .qfea HDF5 format.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name File Format
 * \{ */

/**
 * .qfea file format specification:
 *
 * HDF5 hierarchical structure:
 *
 * /metadata
 *   - version (string)
 *   - blender_version (string)
 *   - creation_time (double)
 *   - author (string)
 *   - description (string)
 *
 * /voxelgrid
 *   - resolution (float)
 *   - dimensions (int[3])
 *   - layout_type (int)
 *   - occupancy (compressed array)
 *   - origin (float[3])
 *   - bounds_min (float[3])
 *   - bounds_max (float[3])
 *
 * /tensorfield
 *   - num_channels (int)
 *   /channels
 *     /channel_0
 *       - name (string)
 *       - unit (string)
 *       - channel_type (int)
 *       - data_type (int)
 *       - data (compressed array)
 *     /channel_1
 *       ...
 *
 * /materials
 *   /material_0
 *     - name (string)
 *     - epsilon_r, mu_r, sigma, etc.
 *     /atomic_structure
 *       - crystal_system (int)
 *       - lattice_constants (float[3])
 *       - atom_positions (array)
 *   /material_1
 *     ...
 *
 * /physics
 *   - em_enabled (bool)
 *   - quantum_enabled (bool)
 *   - particles_enabled (bool)
 *   /em_config
 *     - timestep, courant_factor, etc.
 *   /quantum_config
 *     - ...
 *   /sources
 *     ...
 *   /boundary_conditions
 *     ...
 *
 * /timeline
 *   - start_time (float)
 *   - end_time (float)
 *   /sources
 *     /source_0
 *       - name (string)
 *       - type (int)
 *       - sample_times (array)
 *       - data (compressed array)
 *     ...
 *
 * /compute
 *   - num_devices (int)
 *   /devices
 *     ...
 *   /statistics
 *     ...
 */

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Operations
 * \{ */

/**
 * File handle for .qfea format.
 */
typedef struct QFEAFile {
  void *hdf5_file;           /* HDF5 file handle */
  char filepath[1024];       /* Full file path */
  char mode;                 /* 'r'=read, 'w'=write, 'a'=append */

  /* Cached metadata */
  char version[32];
  char blender_version[32];
  double creation_time;

  /* Compression settings */
  char use_compression;
  int compression_level;     /* 1-9 for gzip, zstd */
  char compression_codec[32]; /* "gzip", "lz4", "zstd" */
} QFEAFile;

/**
 * Open .qfea file for reading.
 *
 * \param filepath: File path
 * \return File handle or NULL if failed
 */
QFEAFile *QFEA_file_open_read(const char *filepath);

/**
 * Open .qfea file for writing.
 *
 * \param filepath: Output path
 * \param compression: Enable compression
 * \param compression_codec: Compression type ("gzip", "lz4", "zstd")
 * \param compression_level: Compression level (1-9)
 * \return File handle or NULL if failed
 */
QFEAFile *QFEA_file_open_write(const char *filepath,
                                 bool compression,
                                 const char *compression_codec,
                                 int compression_level);

/**
 * Close file.
 *
 * Flushes all pending writes and releases resources.
 */
void QFEA_file_close(QFEAFile *file);

/**
 * Flush pending writes to disk.
 */
void QFEA_file_flush(QFEAFile *file);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Writing
 * \{ */

/**
 * Write complete simulation state to file.
 *
 * Writes all data: voxel grid, tensor fields, materials, physics config,
 * timeline data, and compute statistics.
 *
 * \param file: File handle (write mode)
 * \param voxel_grid: Voxel grid
 * \param tensor: Tensor field
 * \param physics: Physics configuration
 * \param timeline: Timeline data (optional)
 * \return true if successful
 */
bool QFEA_file_write_all(QFEAFile *file,
                          const QFEAVoxelGrid *voxel_grid,
                          const QFEATensorField *tensor,
                          const QFEAPhysicsModule *physics,
                          const QFEATimeline *timeline);

/**
 * Write voxel grid to file.
 *
 * \param file: File handle
 * \param voxel_grid: Voxel grid
 * \return true if successful
 */
bool QFEA_file_write_voxelgrid(QFEAFile *file, const QFEAVoxelGrid *voxel_grid);

/**
 * Write tensor field to file.
 *
 * \param file: File handle
 * \param tensor: Tensor field
 * \param channel_indices: Array of channel indices to write (NULL = all)
 * \param num_channels: Number of channels in array
 * \return true if successful
 */
bool QFEA_file_write_tensorfield(QFEAFile *file,
                                   const QFEATensorField *tensor,
                                   const int *channel_indices,
                                   int num_channels);

/**
 * Write materials to file.
 *
 * \param file: File handle
 * \param materials: Material list
 * \return true if successful
 */
bool QFEA_file_write_materials(QFEAFile *file, const ListBase *materials);

/**
 * Write physics configuration to file.
 *
 * \param file: File handle
 * \param physics: Physics module
 * \return true if successful
 */
bool QFEA_file_write_physics(QFEAFile *file, const QFEAPhysicsModule *physics);

/**
 * Write timeline to file.
 *
 * \param file: File handle
 * \param timeline: Timeline
 * \return true if successful
 */
bool QFEA_file_write_timeline(QFEAFile *file, const QFEATimeline *timeline);

/**
 * Write metadata to file.
 *
 * \param file: File handle
 * \param author: Author name
 * \param description: File description
 */
void QFEA_file_write_metadata(QFEAFile *file, const char *author, const char *description);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reading
 * \{ */

/**
 * Read complete simulation state from file.
 *
 * Loads all data into newly allocated structures.
 *
 * \param file: File handle (read mode)
 * \param r_voxel_grid: Output voxel grid
 * \param r_tensor: Output tensor field
 * \param r_physics: Output physics configuration
 * \param r_timeline: Output timeline (can be NULL)
 * \return true if successful
 */
bool QFEA_file_read_all(QFEAFile *file,
                         QFEAVoxelGrid **r_voxel_grid,
                         QFEATensorField **r_tensor,
                         QFEAPhysicsModule **r_physics,
                         QFEATimeline **r_timeline);

/**
 * Read voxel grid from file.
 *
 * \param file: File handle
 * \return Voxel grid or NULL if failed
 */
QFEAVoxelGrid *QFEA_file_read_voxelgrid(QFEAFile *file);

/**
 * Read tensor field from file.
 *
 * \param file: File handle
 * \param channel_indices: Channels to load (NULL = all)
 * \param num_channels: Number of channels in array
 * \return Tensor field or NULL if failed
 */
QFEATensorField *QFEA_file_read_tensorfield(QFEAFile *file,
                                              const int *channel_indices,
                                              int num_channels);

/**
 * Read materials from file.
 *
 * \param file: File handle
 * \param r_materials: Output material list
 * \return Number of materials loaded
 */
int QFEA_file_read_materials(QFEAFile *file, ListBase *r_materials);

/**
 * Read physics configuration from file.
 *
 * \param file: File handle
 * \return Physics module or NULL if failed
 */
QFEAPhysicsModule *QFEA_file_read_physics(QFEAFile *file);

/**
 * Read timeline from file.
 *
 * \param file: File handle
 * \return Timeline or NULL if failed
 */
QFEATimeline *QFEA_file_read_timeline(QFEAFile *file);

/**
 * Read metadata from file.
 *
 * \param file: File handle
 * \param r_author: Output author (allocated by function)
 * \param r_description: Output description (allocated by function)
 * \param r_creation_time: Output creation timestamp
 */
void QFEA_file_read_metadata(QFEAFile *file,
                               char **r_author,
                               char **r_description,
                               double *r_creation_time);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Streaming
 * \{ */

/**
 * Stream writer for large datasets.
 *
 * Allows writing tensor field frame-by-frame without loading entire
 * dataset into memory.
 */
typedef struct QFEAStreamWriter {
  QFEAFile *file;
  char dataset_path[256];    /* HDF5 dataset path */

  int current_frame;
  int total_frames;

  int dims[3];               /* Spatial dimensions */
  int num_channels;
} QFEAStreamWriter;

/**
 * Create stream writer for tensor field.
 *
 * \param file: File handle
 * \param dataset_path: HDF5 path (e.g., "/timeline/source_0/data")
 * \param dims: Spatial dimensions
 * \param num_channels: Number of channels
 * \param total_frames: Total number of frames to write
 * \return Stream writer handle
 */
QFEAStreamWriter *QFEA_stream_writer_new(QFEAFile *file,
                                           const char *dataset_path,
                                           const int dims[3],
                                           int num_channels,
                                           int total_frames);

/**
 * Write next frame to stream.
 *
 * \param writer: Stream writer
 * \param data: Frame data (dims[0] * dims[1] * dims[2] * num_channels)
 */
void QFEA_stream_writer_write_frame(QFEAStreamWriter *writer, const float *data);

/**
 * Close stream writer.
 */
void QFEA_stream_writer_close(QFEAStreamWriter *writer);

/**
 * Stream reader for large datasets.
 */
typedef struct QFEAStreamReader {
  QFEAFile *file;
  char dataset_path[256];

  int current_frame;
  int total_frames;

  int dims[3];
  int num_channels;
} QFEAStreamReader;

/**
 * Create stream reader for tensor field.
 *
 * \param file: File handle
 * \param dataset_path: HDF5 path
 * \return Stream reader handle
 */
QFEAStreamReader *QFEA_stream_reader_new(QFEAFile *file, const char *dataset_path);

/**
 * Read next frame from stream.
 *
 * \param reader: Stream reader
 * \param r_data: Output data buffer (allocated by caller)
 * \return true if successful
 */
bool QFEA_stream_reader_read_frame(QFEAStreamReader *reader, float *r_data);

/**
 * Seek to specific frame.
 *
 * \param reader: Stream reader
 * \param frame_index: Frame to seek to
 */
void QFEA_stream_reader_seek(QFEAStreamReader *reader, int frame_index);

/**
 * Close stream reader.
 */
void QFEA_stream_reader_close(QFEAStreamReader *reader);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Partial I/O
 * \{ */

/**
 * Write tensor field slice to file.
 *
 * Writes only a spatial slice, reducing memory usage.
 *
 * \param file: File handle
 * \param tensor: Tensor field
 * \param channel_index: Channel to write
 * \param axis: Slice axis (0=X, 1=Y, 2=Z)
 * \param slice_index: Slice position along axis
 * \return true if successful
 */
bool QFEA_file_write_tensorfield_slice(QFEAFile *file,
                                         const QFEATensorField *tensor,
                                         int channel_index,
                                         int axis,
                                         int slice_index);

/**
 * Read tensor field slice from file.
 *
 * \param file: File handle
 * \param channel_index: Channel to read
 * \param axis: Slice axis
 * \param slice_index: Slice position
 * \param r_data: Output data buffer (allocated by caller)
 * \return true if successful
 */
bool QFEA_file_read_tensorfield_slice(QFEAFile *file,
                                        int channel_index,
                                        int axis,
                                        int slice_index,
                                        float *r_data);

/**
 * Write tensor field region to file.
 *
 * Writes only specified 3D region.
 *
 * \param file: File handle
 * \param tensor: Tensor field
 * \param channel_index: Channel to write
 * \param start: Region start indices [i, j, k]
 * \param count: Region dimensions [ni, nj, nk]
 * \return true if successful
 */
bool QFEA_file_write_tensorfield_region(QFEAFile *file,
                                          const QFEATensorField *tensor,
                                          int channel_index,
                                          const int start[3],
                                          const int count[3]);

/**
 * Read tensor field region from file.
 *
 * \param file: File handle
 * \param channel_index: Channel to read
 * \param start: Region start indices
 * \param count: Region dimensions
 * \param r_data: Output data buffer (allocated by caller)
 * \return true if successful
 */
bool QFEA_file_read_tensorfield_region(QFEAFile *file,
                                         int channel_index,
                                         const int start[3],
                                         const int count[3],
                                         float *r_data);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inspection
 * \{ */

/**
 * Get file information without loading data.
 *
 * Queries metadata and dataset dimensions.
 */
typedef struct QFEAFileInfo {
  char version[32];
  char blender_version[32];
  double creation_time;
  char author[128];
  char description[512];

  bool has_voxelgrid;
  bool has_tensorfield;
  bool has_physics;
  bool has_timeline;

  /* Voxel grid info */
  int voxel_dims[3];
  float voxel_resolution;

  /* Tensor field info */
  int num_channels;
  char **channel_names;

  /* Timeline info */
  int num_timeline_sources;
  int num_timeline_samples;

  /* File size */
  size_t file_size_bytes;
  size_t compressed_size_bytes;
} QFEAFileInfo;

/**
 * Get file information.
 *
 * \param filepath: File path
 * \param r_info: Output file info
 * \return true if successful
 */
bool QFEA_file_get_info(const char *filepath, QFEAFileInfo *r_info);

/**
 * Free file info structure.
 */
void QFEA_file_info_free(QFEAFileInfo *info);

/**
 * List all datasets in file.
 *
 * \param file: File handle
 * \param r_dataset_paths: Output array of dataset paths (allocated by function)
 * \param r_count: Number of datasets
 */
void QFEA_file_list_datasets(QFEAFile *file, char ***r_dataset_paths, int *r_count);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Legacy Format Support
 * \{ */

/**
 * Import from VTK format.
 *
 * \param filepath: VTK file path
 * \return Tensor field or NULL if failed
 */
QFEATensorField *QFEA_import_vtk(const char *filepath);

/**
 * Export to VTK format.
 *
 * \param filepath: Output VTK path
 * \param tensor: Tensor field
 * \param channel_index: Channel to export
 * \return true if successful
 */
bool QFEA_export_vtk(const char *filepath, const QFEATensorField *tensor, int channel_index);

/**
 * Import from raw binary format.
 *
 * \param filepath: Raw file path
 * \param dims: Grid dimensions
 * \param data_type: Data type (FLOAT32, FLOAT64)
 * \return Tensor field or NULL if failed
 */
QFEATensorField *QFEA_import_raw(const char *filepath,
                                   const int dims[3],
                                   eQFEATensorDataType data_type);

/**
 * Export to raw binary format.
 *
 * \param filepath: Output raw path
 * \param tensor: Tensor field
 * \param channel_index: Channel to export
 * \return true if successful
 */
bool QFEA_export_raw(const char *filepath, const QFEATensorField *tensor, int channel_index);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Check if file is valid .qfea format.
 *
 * \param filepath: File path
 * \return true if valid
 */
bool QFEA_file_is_valid(const char *filepath);

/**
 * Get file format version.
 *
 * \param filepath: File path
 * \return Version string or NULL if invalid
 */
const char *QFEA_file_get_version(const char *filepath);

/**
 * Compress existing .qfea file.
 *
 * Re-writes file with compression enabled.
 *
 * \param filepath: File to compress
 * \param compression_codec: Compression type
 * \param compression_level: Compression level
 * \return true if successful
 */
bool QFEA_file_compress(const char *filepath,
                         const char *compression_codec,
                         int compression_level);

/**
 * Repair corrupted .qfea file.
 *
 * Attempts to recover data from damaged file.
 *
 * \param filepath: File to repair
 * \param output_filepath: Repaired file path
 * \return true if successful
 */
bool QFEA_file_repair(const char *filepath, const char *output_filepath);

/** \} */

#ifdef __cplusplus
}
#endif
