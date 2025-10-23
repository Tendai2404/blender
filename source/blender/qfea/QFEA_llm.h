/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup qfea
 *
 * QFEA LLM integration API for AI-assisted simulation workflows.
 */

#include "DNA_qfea_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name LLM Session Management
 * \{ */

/**
 * LLM provider types.
 */
typedef enum eQFEALLMProvider {
  QFEA_LLM_PROVIDER_OPENAI = 0,      /* OpenAI API (GPT-4, etc.) */
  QFEA_LLM_PROVIDER_ANTHROPIC = 1,   /* Anthropic Claude */
  QFEA_LLM_PROVIDER_LOCAL = 2,       /* Local model (llama.cpp, etc.) */
  QFEA_LLM_PROVIDER_CUSTOM = 3,      /* Custom API endpoint */
} eQFEALLMProvider;

/**
 * LLM configuration.
 */
typedef struct QFEALLMConfig {
  eQFEALLMProvider provider;
  char api_key[256];           /* API key (encrypted in preferences) */
  char api_endpoint[512];      /* Custom API endpoint URL */
  char model_name[128];        /* Model identifier */

  float temperature;           /* Sampling temperature (0-1) */
  int max_tokens;              /* Maximum response length */
  float top_p;                 /* Nucleus sampling parameter */

  /* Local model settings */
  char local_model_path[1024]; /* Path to .gguf file */
  int num_threads;             /* CPU threads for inference */
  int context_size;            /* Context window size */

  /* Privacy */
  char send_geometry;          /* Send mesh data to LLM */
  char send_materials;         /* Send material properties */
  char send_results;           /* Send simulation results */
} QFEALLMConfig;

/**
 * Create new LLM session.
 *
 * \param config: LLM configuration
 * \return Session handle
 */
struct QFEALLMSession *QFEA_llm_session_new(const QFEALLMConfig *config);

/**
 * Free LLM session.
 */
void QFEA_llm_session_free(struct QFEALLMSession *session);

/**
 * Test connection to LLM provider.
 *
 * \param config: Configuration to test
 * \param r_error: Error message if connection fails
 * \return true if successful
 */
bool QFEA_llm_test_connection(const QFEALLMConfig *config, char **r_error);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Prompt Engineering
 * \{ */

/**
 * Message role.
 */
typedef enum eQFEALLMRole {
  QFEA_LLM_ROLE_SYSTEM = 0,    /* System prompt */
  QFEA_LLM_ROLE_USER = 1,      /* User message */
  QFEA_LLM_ROLE_ASSISTANT = 2, /* Assistant response */
} eQFEALLMRole;

/**
 * Conversation message.
 */
typedef struct QFEALLMMessage {
  struct QFEALLMMessage *next, *prev;

  eQFEALLMRole role;
  char *content;              /* Message text (dynamically allocated) */
  double timestamp;           /* Unix timestamp */
} QFEALLMMessage;

/**
 * Add message to conversation.
 *
 * \param session: LLM session
 * \param role: Message role
 * \param content: Message text
 */
void QFEA_llm_add_message(struct QFEALLMSession *session,
                           eQFEALLMRole role,
                           const char *content);

/**
 * Clear conversation history.
 */
void QFEA_llm_clear_history(struct QFEALLMSession *session);

/**
 * Build system prompt from current scene context.
 *
 * Automatically describes:
 * - Available materials in database
 * - Active simulation physics modules
 * - Voxel grid configuration
 * - Available compute devices
 *
 * \param session: LLM session
 * \param voxel_grid: Current voxel grid (optional)
 * \param physics: Physics configuration (optional)
 * \return System prompt text (must be freed)
 */
char *QFEA_llm_build_system_prompt(struct QFEALLMSession *session,
                                     const QFEAVoxelGrid *voxel_grid,
                                     const QFEAPhysicsModule *physics);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inference
 * \{ */

/**
 * Send query to LLM and get response.
 *
 * Blocks until response received or timeout.
 *
 * \param session: LLM session
 * \param prompt: User query
 * \param r_response: Output response text (allocated by function)
 * \param timeout: Timeout in seconds (0 = no timeout)
 * \return true if successful
 */
bool QFEA_llm_query(struct QFEALLMSession *session,
                     const char *prompt,
                     char **r_response,
                     float timeout);

/**
 * Asynchronous query (non-blocking).
 *
 * Returns immediately, call QFEA_llm_request_poll() to check status.
 *
 * \param session: LLM session
 * \param prompt: User query
 * \return Request handle
 */
struct QFEALLMRequest *QFEA_llm_query_async(struct QFEALLMSession *session, const char *prompt);

/**
 * Poll asynchronous request status.
 *
 * \param request: Request handle
 * \param r_response: Output response if ready (allocated by function)
 * \return true if response ready
 */
bool QFEA_llm_request_poll(struct QFEALLMRequest *request, char **r_response);

/**
 * Cancel asynchronous request.
 */
void QFEA_llm_request_cancel(struct QFEALLMRequest *request);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Task-Specific Queries
 * \{ */

/**
 * Ask LLM to suggest simulation parameters.
 *
 * \param session: LLM session
 * \param description: Natural language description of desired simulation
 * \param r_suggestions: Output suggestions (JSON format)
 * \return true if successful
 */
bool QFEA_llm_suggest_parameters(struct QFEALLMSession *session,
                                   const char *description,
                                   char **r_suggestions);

/**
 * Ask LLM to explain simulation results.
 *
 * \param session: LLM session
 * \param timeline: Timeline with recorded data
 * \param source_name: Data source to analyze
 * \param r_explanation: Output explanation text
 * \return true if successful
 */
bool QFEA_llm_explain_results(struct QFEALLMSession *session,
                                const QFEATimeline *timeline,
                                const char *source_name,
                                char **r_explanation);

/**
 * Ask LLM to recommend material for application.
 *
 * \param session: LLM session
 * \param application: Application description (e.g., "high-frequency antenna")
 * \param constraints: Constraint string (e.g., "conductivity > 1e6, cost < $10/kg")
 * \param r_materials: Output material recommendations (JSON array)
 * \return true if successful
 */
bool QFEA_llm_recommend_material(struct QFEALLMSession *session,
                                   const char *application,
                                   const char *constraints,
                                   char **r_materials);

/**
 * Ask LLM to generate node tree from description.
 *
 * \param session: LLM session
 * \param description: Natural language workflow description
 * \param r_node_tree_json: Output node tree structure (JSON)
 * \return true if successful
 */
bool QFEA_llm_generate_node_tree(struct QFEALLMSession *session,
                                   const char *description,
                                   char **r_node_tree_json);

/**
 * Ask LLM to debug failed simulation.
 *
 * \param session: LLM session
 * \param error_log: Simulation error messages
 * \param physics: Physics configuration
 * \param r_diagnosis: Output diagnosis and fix suggestions
 * \return true if successful
 */
bool QFEA_llm_debug_simulation(struct QFEALLMSession *session,
                                 const char *error_log,
                                 const QFEAPhysicsModule *physics,
                                 char **r_diagnosis);

/**
 * Ask LLM to optimize simulation performance.
 *
 * \param session: LLM session
 * \param stats: Compute statistics
 * \param voxel_grid: Current voxel grid
 * \param r_optimizations: Output optimization suggestions
 * \return true if successful
 */
bool QFEA_llm_optimize_performance(struct QFEALLMSession *session,
                                     const struct QFEAComputeStats *stats,
                                     const QFEAVoxelGrid *voxel_grid,
                                     char **r_optimizations);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Code Generation
 * \{ */

/**
 * Generate Python script from natural language.
 *
 * \param session: LLM session
 * \param description: Task description
 * \param r_script: Output Python code
 * \return true if successful
 */
bool QFEA_llm_generate_python_script(struct QFEALLMSession *session,
                                       const char *description,
                                       char **r_script);

/**
 * Generate custom CUDA kernel from specification.
 *
 * \param session: LLM session
 * \param specification: Kernel behavior description
 * \param r_cuda_code: Output CUDA C++ code
 * \return true if successful
 */
bool QFEA_llm_generate_cuda_kernel(struct QFEALLMSession *session,
                                     const char *specification,
                                     char **r_cuda_code);

/**
 * Generate material definition from physical description.
 *
 * \param session: LLM session
 * \param description: Material description
 * \param r_material_json: Output material properties (JSON)
 * \return true if successful
 */
bool QFEA_llm_generate_material_definition(struct QFEALLMSession *session,
                                             const char *description,
                                             char **r_material_json);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Building
 * \{ */

/**
 * Serialize voxel grid to JSON for LLM context.
 *
 * \param voxel_grid: Voxel grid
 * \return JSON string (must be freed)
 */
char *QFEA_llm_serialize_voxelgrid(const QFEAVoxelGrid *voxel_grid);

/**
 * Serialize material to JSON.
 *
 * \param material: Material
 * \return JSON string (must be freed)
 */
char *QFEA_llm_serialize_material(const QFEAMaterial *material);

/**
 * Serialize physics configuration to JSON.
 *
 * \param physics: Physics module
 * \return JSON string (must be freed)
 */
char *QFEA_llm_serialize_physics(const QFEAPhysicsModule *physics);

/**
 * Serialize timeline source to JSON.
 *
 * \param source: Timeline source
 * \param include_data: Include actual data values (can be large)
 * \return JSON string (must be freed)
 */
char *QFEA_llm_serialize_timeline_source(const struct QFEATimelineSource *source,
                                           bool include_data);

/**
 * Serialize compute statistics to JSON.
 *
 * \param stats: Compute stats
 * \return JSON string (must be freed)
 */
char *QFEA_llm_serialize_compute_stats(const struct QFEAComputeStats *stats);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Response Parsing
 * \{ */

/**
 * Parse JSON response into material.
 *
 * \param json: JSON string from LLM
 * \param r_material: Output material (allocated by function)
 * \return true if parsing successful
 */
bool QFEA_llm_parse_material(const char *json, QFEAMaterial **r_material);

/**
 * Parse JSON response into physics configuration.
 *
 * \param json: JSON string
 * \param r_physics: Output physics module (allocated by function)
 * \return true if successful
 */
bool QFEA_llm_parse_physics(const char *json, QFEAPhysicsModule **r_physics);

/**
 * Parse JSON response into node tree.
 *
 * \param json: JSON string
 * \param r_node_tree: Output node tree (allocated by function)
 * \return true if successful
 */
bool QFEA_llm_parse_node_tree(const char *json, struct bNodeTree **r_node_tree);

/**
 * Extract code block from markdown response.
 *
 * Extracts first code fence (```language...```) from LLM response.
 *
 * \param markdown: LLM response text
 * \param language: Expected language (or NULL for any)
 * \param r_code: Output code (allocated by function)
 * \return true if code found
 */
bool QFEA_llm_extract_code_block(const char *markdown, const char *language, char **r_code);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Embeddings
 * \{ */

/**
 * Generate embedding vector for text.
 *
 * Used for semantic search in material database, documentation, etc.
 *
 * \param session: LLM session
 * \param text: Text to embed
 * \param r_embedding: Output embedding vector (allocated by function)
 * \param r_dimension: Embedding dimension
 * \return true if successful
 */
bool QFEA_llm_generate_embedding(struct QFEALLMSession *session,
                                   const char *text,
                                   float **r_embedding,
                                   int *r_dimension);

/**
 * Compute cosine similarity between embeddings.
 *
 * \param embedding1: First embedding
 * \param embedding2: Second embedding
 * \param dimension: Embedding dimension
 * \return Similarity score (0-1)
 */
float QFEA_llm_cosine_similarity(const float *embedding1,
                                   const float *embedding2,
                                   int dimension);

/**
 * Semantic search in material database.
 *
 * \param session: LLM session
 * \param query: Natural language query
 * \param num_results: Number of results to return
 * \param r_materials: Output material array (allocated by function)
 * \param r_scores: Similarity scores
 * \return Number of results found
 */
int QFEA_llm_search_materials_semantic(struct QFEALLMSession *session,
                                         const char *query,
                                         int num_results,
                                         QFEAMaterial ***r_materials,
                                         float **r_scores);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interactive Mode
 * \{ */

/**
 * Start interactive chat session.
 *
 * Opens modal dialog for real-time conversation with LLM.
 *
 * \param session: LLM session
 * \param context: Optional context (C for current selection)
 */
void QFEA_llm_start_interactive_chat(struct QFEALLMSession *session,
                                       struct bContext *context);

/**
 * Send streaming query (token-by-token response).
 *
 * \param session: LLM session
 * \param prompt: User query
 * \param callback: Called for each token received
 * \param userdata: User data for callback
 * \return Request handle
 */
typedef void (*QFEALLMStreamCallback)(const char *token, void *userdata);

struct QFEALLMRequest *QFEA_llm_query_streaming(struct QFEALLMSession *session,
                                                   const char *prompt,
                                                   QFEALLMStreamCallback callback,
                                                   void *userdata);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Caching
 * \{ */

/**
 * Cache LLM response for prompt.
 *
 * Reduces API costs by caching common queries locally.
 *
 * \param prompt: Query prompt
 * \param response: LLM response
 * \param ttl: Time to live (seconds, 0 = forever)
 */
void QFEA_llm_cache_set(const char *prompt, const char *response, float ttl);

/**
 * Get cached response for prompt.
 *
 * \param prompt: Query prompt
 * \return Cached response or NULL if not found/expired
 */
const char *QFEA_llm_cache_get(const char *prompt);

/**
 * Clear all cached responses.
 */
void QFEA_llm_cache_clear(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rate Limiting
 * \{ */

/**
 * Rate limiter state.
 */
typedef struct QFEALLMRateLimiter {
  int requests_per_minute;
  int tokens_per_minute;

  int current_requests;
  int current_tokens;

  double window_start_time;
} QFEALLMRateLimiter;

/**
 * Check if request would exceed rate limits.
 *
 * \param limiter: Rate limiter
 * \param estimated_tokens: Estimated token count for request
 * \return true if within limits
 */
bool QFEA_llm_rate_limit_check(QFEALLMRateLimiter *limiter, int estimated_tokens);

/**
 * Record request for rate limiting.
 *
 * \param limiter: Rate limiter
 * \param tokens_used: Actual tokens consumed
 */
void QFEA_llm_rate_limit_record(QFEALLMRateLimiter *limiter, int tokens_used);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Safety
 * \{ */

/**
 * Sanitize user input before sending to LLM.
 *
 * Removes sensitive data (API keys, file paths, etc.).
 *
 * \param input: User input
 * \return Sanitized text (must be freed)
 */
char *QFEA_llm_sanitize_input(const char *input);

/**
 * Validate LLM response for safety.
 *
 * Checks for potentially harmful code or instructions.
 *
 * \param response: LLM response
 * \param r_issues: Output safety issues if any
 * \return true if safe
 */
bool QFEA_llm_validate_response(const char *response, char **r_issues);

/** \} */

#ifdef __cplusplus
}
#endif
