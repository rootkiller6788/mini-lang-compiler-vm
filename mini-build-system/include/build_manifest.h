#ifndef BUILD_MANIFEST_H
#define BUILD_MANIFEST_H

#include <stdbool.h>

/* ============================================================================
 * L1: Core Definitions - Build Manifest / Workspace
 *
 * A build manifest describes a multi-module project structure.
 * Analogous to:
 *   Bazel: WORKSPACE + BUILD files
 *   CMake: CMakeLists.txt (top-level + subdirectories)
 *   Buck:  .buckconfig + BUCK files
 *
 * Course: CMU 15-410, Stanford CS 245
 * ========================================================================= */

#define MANIFEST_MAX_PROJECTS 32
#define MANIFEST_MAX_TARGETS  128
#define MANIFEST_MAX_DEPS     16
#define MANIFEST_NAME_LEN     128
#define MANIFEST_PATH_LEN     256
#define MANIFEST_MAX_BUILD_ORDER 256

/* L1: ProjectType - distinguishes project categories */
typedef enum {
    PROJECT_LIBRARY,       /* static/dynamic library */
    PROJECT_EXECUTABLE,    /* binary executable */
    PROJECT_TEST,          /* test project */
    PROJECT_BENCHMARK,     /* performance benchmark */
    PROJECT_HEADER_ONLY    /* header-only library */
} ProjectType;

/* L1: TargetSpec - a single buildable target within a project */
typedef struct {
    char        name[MANIFEST_NAME_LEN];
    char        source_dir[MANIFEST_PATH_LEN];
    char        output_name[MANIFEST_NAME_LEN];
    ProjectType type;
    char        deps[MANIFEST_MAX_DEPS][MANIFEST_NAME_LEN];
    int         num_deps;
    bool        is_default;
    bool        requires_toolchain;
    char        toolchain[64];
} TargetSpec;

/* L1: ProjectSpec - a project within a workspace */
typedef struct {
    char        name[MANIFEST_NAME_LEN];
    char        root_path[MANIFEST_PATH_LEN];
    ProjectType type;
    TargetSpec  targets[MANIFEST_MAX_TARGETS];
    int         num_targets;
    char        deps[MANIFEST_MAX_DEPS][MANIFEST_NAME_LEN];
    int         num_deps;
    bool        enabled;
} ProjectSpec;

/* L1: BuildWorkspace - the top-level workspace container */
typedef struct {
    char        name[MANIFEST_NAME_LEN];
    char        root_dir[MANIFEST_PATH_LEN];
    ProjectSpec projects[MANIFEST_MAX_PROJECTS];
    int         num_projects;
    char        build_order[MANIFEST_MAX_BUILD_ORDER][MANIFEST_NAME_LEN];
    int         num_in_order;
    bool        is_configured;
} BuildWorkspace;

/* Workspace lifecycle (L2: Core Concept - workspace management) */
void manifest_init(BuildWorkspace *ws, const char *name, const char *root_dir);
int  manifest_add_project(BuildWorkspace *ws, const char *name,
                           const char *root_path, ProjectType type);
int  manifest_add_target(BuildWorkspace *ws, int proj_idx, const char *name,
                          const char *source_dir, ProjectType type);
bool manifest_add_target_dep(BuildWorkspace *ws, int proj_idx,
                              int tgt_idx, const char *dep_name);
bool manifest_add_project_dep(BuildWorkspace *ws, int proj_idx,
                               const char *dep_name);

/* L3: Engineering Structure - dependency resolution & build ordering */
bool manifest_resolve_deps(BuildWorkspace *ws);
bool manifest_compute_build_order(BuildWorkspace *ws);
int  manifest_find_project(const BuildWorkspace *ws, const char *name);
bool manifest_validate(const BuildWorkspace *ws);

/* L6: Canonical Problem - multi-project build orchestration */
void manifest_print(const BuildWorkspace *ws);
void manifest_print_build_order(const BuildWorkspace *ws);
bool manifest_has_cycles(const BuildWorkspace *ws);

#endif
