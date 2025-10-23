/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA timeline system for multi-source data playback and analysis.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Timeline Management
 * \{ */

/**
 * Create new QFEA timeline.
 *
 * Timeline manages playback of simulation results, probes, and analysis data
 * with support for multiple data sources synchronized to simulation time.
 *
 * \return Timeline instance
 */
QFEATimeline *QFEA_timeline_new(void);

/**
 * Free timeline and all associated data sources.
 */
void QFEA_timeline_free(QFEATimeline *timeline);

/**
 * Set simulation time range.
 *
 * \param timeline: Timeline
 * \param start_time: Start time (seconds)
 * \param end_time: End time (seconds)
 */
void QFEA_timeline_set_range(QFEATimeline *timeline, float start_time, float end_time);

/**
 * Set current playback time.
 *
 * \param timeline: Timeline
 * \param time: Current time (seconds)
 */
void QFEA_timeline_set_current_time(QFEATimeline *timeline, float time);

/**
 * Get current playback time.
 */
float QFEA_timeline_get_current_time(const QFEATimeline *timeline);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Sources
 * \{ */

/**
 * Timeline data source types.
 */
typedef enum eQFEATimelineSourceType {
  QFEA_TIMELINE_SOURCE_TENSOR = 0,    /* Tensor field snapshots */
  QFEA_TIMELINE_SOURCE_PROBE = 1,     /* Field probe time series */
  QFEA_TIMELINE_SOURCE_ENERGY = 2,    /* Energy integral history */
  QFEA_TIMELINE_SOURCE_FLUX = 3,      /* Flux calculation history */
  QFEA_TIMELINE_SOURCE_PARTICLE = 4,  /* Particle trajectory */
  QFEA_TIMELINE_SOURCE_SPECTRUM = 5,  /* FFT spectrum evolution */
  QFEA_TIMELINE_SOURCE_STATS = 6,     /* Field statistics */
} eQFEATimelineSourceType;

/**
 * Timeline data source.
 */
typedef struct QFEATimelineSource {
  struct QFEATimelineSource *next, *prev;

  char name[64];                       /* Source name for UI */
  eQFEATimelineSourceType type;        /* Source type */

  int num_samples;                     /* Number of time samples */
  float *sample_times;                 /* Time points (seconds) */

  /* Type-specific data */
  void *data;                          /* Type-specific payload */

  /* Playback state */
  int current_sample_index;            /* Current sample for interpolation */
  char is_visible;                     /* Display in timeline */
  char is_muted;                       /* Skip during playback */

  /* Metadata */
  char unit[32];                       /* Physical unit */
  float min_value, max_value;          /* Value range for normalization */

  /* GPU cache */
  struct GPUVertBuf *gpu_cache;        /* Uploaded to GPU for realtime playback */
} QFEATimelineSource;

/**
 * Add tensor field snapshot source.
 *
 * Stores tensor field at discrete time points for playback.
 *
 * \param timeline: Timeline
 * \param name: Source name
 * \param tensor: Tensor field (will be copied at each snapshot)
 * \param channel_index: Channel to record (-1 for all channels)
 * \return Source pointer
 */
QFEATimelineSource *QFEA_timeline_add_tensor_source(QFEATimeline *timeline,
                                                      const char *name,
                                                      QFEATensorField *tensor,
                                                      int channel_index);

/**
 * Add field probe source.
 *
 * Records scalar/vector value at specific position over time.
 *
 * \param timeline: Timeline
 * \param name: Source name
 * \param position: Probe position (world coordinates)
 * \param tensor: Tensor field to probe
 * \param channel_index: Channel to sample
 * \return Source pointer
 */
QFEATimelineSource *QFEA_timeline_add_probe_source(QFEATimeline *timeline,
                                                     const char *name,
                                                     const float position[3],
                                                     QFEATensorField *tensor,
                                                     int channel_index);

/**
 * Add energy integral source.
 *
 * Records total field energy over time.
 *
 * \param timeline: Timeline
 * \param name: Source name
 * \param tensor: Tensor field
 * \param ex_channel, ey_channel, ez_channel: E-field components
 * \param epsilon: Permittivity array
 * \return Source pointer
 */
QFEATimelineSource *QFEA_timeline_add_energy_source(QFEATimeline *timeline,
                                                      const char *name,
                                                      QFEATensorField *tensor,
                                                      int ex_channel,
                                                      int ey_channel,
                                                      int ez_channel,
                                                      const float *epsilon);

/**
 * Add particle trajectory source.
 *
 * Records position and velocity of single particle over time.
 *
 * \param timeline: Timeline
 * \param name: Source name
 * \param particle_index: Index in particle system
 * \param particle_system: Particle system to track
 * \return Source pointer
 */
QFEATimelineSource *QFEA_timeline_add_particle_source(QFEATimeline *timeline,
                                                        const char *name,
                                                        int particle_index,
                                                        QFEAParticleSystem *particle_system);

/**
 * Add FFT spectrum source.
 *
 * Records frequency spectrum evolution over time.
 *
 * \param timeline: Timeline
 * \param name: Source name
 * \param tensor: Tensor field
 * \param channel_index: Channel to analyze
 * \param num_frequencies: FFT resolution
 * \return Source pointer
 */
QFEATimelineSource *QFEA_timeline_add_spectrum_source(QFEATimeline *timeline,
                                                        const char *name,
                                                        QFEATensorField *tensor,
                                                        int channel_index,
                                                        int num_frequencies);

/**
 * Remove data source from timeline.
 */
void QFEA_timeline_remove_source(QFEATimeline *timeline, QFEATimelineSource *source);

/**
 * Get source by name.
 *
 * \return Source pointer or NULL if not found
 */
QFEATimelineSource *QFEA_timeline_get_source(const QFEATimeline *timeline, const char *name);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recording
 * \{ */

/**
 * Start recording on all active sources.
 *
 * \param timeline: Timeline
 * \param timestep_interval: Record every N timesteps
 */
void QFEA_timeline_start_recording(QFEATimeline *timeline, int timestep_interval);

/**
 * Stop recording.
 */
void QFEA_timeline_stop_recording(QFEATimeline *timeline);

/**
 * Record current frame for all active sources.
 *
 * Samples all non-muted sources at current simulation time.
 *
 * \param timeline: Timeline
 * \param current_time: Simulation time (seconds)
 */
void QFEA_timeline_record_frame(QFEATimeline *timeline, float current_time);

/**
 * Clear all recorded data.
 */
void QFEA_timeline_clear_data(QFEATimeline *timeline);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Playback
 * \{ */

/**
 * Playback modes.
 */
typedef enum eQFEATimelinePlayMode {
  QFEA_TIMELINE_PLAY_ONCE = 0,       /* Play once and stop */
  QFEA_TIMELINE_PLAY_LOOP = 1,       /* Loop continuously */
  QFEA_TIMELINE_PLAY_PINGPONG = 2,   /* Ping-pong (forward then reverse) */
} eQFEATimelinePlayMode;

/**
 * Start playback.
 *
 * \param timeline: Timeline
 * \param mode: Playback mode
 * \param fps: Playback speed (frames per second)
 */
void QFEA_timeline_play(QFEATimeline *timeline, eQFEATimelinePlayMode mode, float fps);

/**
 * Pause playback.
 */
void QFEA_timeline_pause(QFEATimeline *timeline);

/**
 * Stop playback and reset to start.
 */
void QFEA_timeline_stop(QFEATimeline *timeline);

/**
 * Step forward one frame.
 */
void QFEA_timeline_step_forward(QFEATimeline *timeline);

/**
 * Step backward one frame.
 */
void QFEA_timeline_step_backward(QFEATimeline *timeline);

/**
 * Jump to specific frame.
 *
 * \param timeline: Timeline
 * \param frame_index: Frame number
 */
void QFEA_timeline_goto_frame(QFEATimeline *timeline, int frame_index);

/**
 * Check if timeline is currently playing.
 */
bool QFEA_timeline_is_playing(const QFEATimeline *timeline);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interpolation
 * \{ */

/**
 * Sample source at arbitrary time (interpolated).
 *
 * \param source: Timeline source
 * \param time: Time to sample (seconds)
 * \param r_value: Output value
 * \return true if successful
 */
bool QFEA_timeline_source_sample_scalar(const QFEATimelineSource *source,
                                          float time,
                                          float *r_value);

/**
 * Sample vector source at time.
 *
 * \param r_vector: Output vector [3]
 */
bool QFEA_timeline_source_sample_vector(const QFEATimelineSource *source,
                                          float time,
                                          float r_vector[3]);

/**
 * Sample tensor field source at time.
 *
 * Reconstructs tensor field state via temporal interpolation.
 *
 * \param source: Timeline source (must be tensor type)
 * \param time: Time to sample
 * \param r_tensor: Output tensor field (must be pre-allocated)
 */
bool QFEA_timeline_source_sample_tensor(const QFEATimelineSource *source,
                                          float time,
                                          QFEATensorField *r_tensor);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Analysis
 * \{ */

/**
 * Compute statistics over time range.
 */
typedef struct QFEATimelineStats {
  float min;
  float max;
  float mean;
  float std_dev;
  float rms;

  float time_at_min;  /* Time when minimum occurred */
  float time_at_max;  /* Time when maximum occurred */
} QFEATimelineStats;

void QFEA_timeline_source_compute_stats(const QFEATimelineSource *source,
                                          float start_time,
                                          float end_time,
                                          QFEATimelineStats *r_stats);

/**
 * Detect peaks in time series.
 *
 * \param source: Timeline source
 * \param threshold: Minimum peak height
 * \param r_peak_times: Output peak times (allocated by function)
 * \param r_peak_values: Output peak values
 * \return Number of peaks found
 */
int QFEA_timeline_source_find_peaks(const QFEATimelineSource *source,
                                      float threshold,
                                      float **r_peak_times,
                                      float **r_peak_values);

/**
 * Compute FFT of time series.
 *
 * \param source: Timeline source
 * \param r_frequencies: Output frequency array (Hz)
 * \param r_amplitudes: Output amplitude spectrum
 * \param r_num_frequencies: Number of frequency bins
 */
void QFEA_timeline_source_fft(const QFEATimelineSource *source,
                                float **r_frequencies,
                                float **r_amplitudes,
                                int *r_num_frequencies);

/**
 * Compute autocorrelation of time series.
 *
 * \param source: Timeline source
 * \param max_lag: Maximum lag time (seconds)
 * \param r_lags: Output lag times
 * \param r_correlation: Output correlation values
 * \param r_num_lags: Number of lag points
 */
void QFEA_timeline_source_autocorrelation(const QFEATimelineSource *source,
                                            float max_lag,
                                            float **r_lags,
                                            float **r_correlation,
                                            int *r_num_lags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Export
 * \{ */

/**
 * Export source to CSV file.
 *
 * \param source: Timeline source
 * \param filepath: Output CSV path
 * \return true if successful
 */
bool QFEA_timeline_source_export_csv(const QFEATimelineSource *source, const char *filepath);

/**
 * Export all sources to multi-column CSV.
 *
 * \param timeline: Timeline
 * \param filepath: Output CSV path
 * \return true if successful
 */
bool QFEA_timeline_export_csv(const QFEATimeline *timeline, const char *filepath);

/**
 * Export tensor field snapshots to image sequence.
 *
 * \param source: Tensor source
 * \param output_dir: Output directory
 * \param format: Image format ("PNG", "EXR", "TIFF")
 * \param channel_index: Channel to export
 * \param slice_axis: Axis to slice (0=X, 1=Y, 2=Z)
 * \param slice_index: Slice position
 * \return Number of images written
 */
int QFEA_timeline_export_image_sequence(const QFEATimelineSource *source,
                                          const char *output_dir,
                                          const char *format,
                                          int channel_index,
                                          int slice_axis,
                                          int slice_index);

/**
 * Export particle trajectories to VTK format.
 *
 * \param source: Particle source
 * \param filepath: Output VTK path
 * \return true if successful
 */
bool QFEA_timeline_export_particle_vtk(const QFEATimelineSource *source, const char *filepath);

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Synchronization
 * \{ */

/**
 * Upload source data to GPU for realtime playback.
 *
 * Creates GPU buffers for fast scrubbing and interpolation.
 *
 * \param source: Timeline source
 */
void QFEA_timeline_source_sync_gpu(QFEATimelineSource *source);

/**
 * Free GPU resources for source.
 */
void QFEA_timeline_source_gpu_free(QFEATimelineSource *source);

/**
 * Upload all sources to GPU.
 */
void QFEA_timeline_sync_gpu(QFEATimeline *timeline);

/**
 * Free all GPU resources.
 */
void QFEA_timeline_gpu_free(QFEATimeline *timeline);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Markers and Annotations
 * \{ */

/**
 * Timeline marker.
 */
typedef struct QFEATimelineMarker {
  struct QFEATimelineMarker *next, *prev;

  char name[64];
  float time;
  char color[3];  /* RGB color for UI */
  char note[256]; /* Optional annotation */
} QFEATimelineMarker;

/**
 * Add marker to timeline.
 *
 * \param timeline: Timeline
 * \param name: Marker name
 * \param time: Time position (seconds)
 * \return Marker pointer
 */
QFEATimelineMarker *QFEA_timeline_add_marker(QFEATimeline *timeline,
                                               const char *name,
                                               float time);

/**
 * Remove marker.
 */
void QFEA_timeline_remove_marker(QFEATimeline *timeline, QFEATimelineMarker *marker);

/**
 * Find nearest marker to time.
 *
 * \param timeline: Timeline
 * \param time: Time to search near
 * \param threshold: Maximum distance (seconds)
 * \return Marker pointer or NULL
 */
QFEATimelineMarker *QFEA_timeline_find_marker(const QFEATimeline *timeline,
                                                 float time,
                                                 float threshold);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Comparison
 * \{ */

/**
 * Compare two sources (e.g., experimental vs simulation).
 *
 * Computes RMSE, correlation coefficient, and other metrics.
 */
typedef struct QFEATimelineComparison {
  float rmse;                  /* Root mean square error */
  float mae;                   /* Mean absolute error */
  float correlation;           /* Pearson correlation coefficient */
  float r_squared;             /* R² coefficient of determination */

  float max_absolute_error;    /* Maximum error */
  float time_at_max_error;     /* When max error occurred */
} QFEATimelineComparison;

void QFEA_timeline_compare_sources(const QFEATimelineSource *source1,
                                     const QFEATimelineSource *source2,
                                     float start_time,
                                     float end_time,
                                     QFEATimelineComparison *r_comparison);

/** \} */

#ifdef __cplusplus
}
#endif
