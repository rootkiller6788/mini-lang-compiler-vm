#define _CRT_SECURE_NO_WARNINGS
#include "build_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ============================================================================
 * L1: BuildWorkspace / ProjectSpec / TargetSpec Implementation
 * L2: Workspace Management Core Concept
 * L3: Multi-module dependency resolution & build ordering
 * L6: Canonical multi-project build orchestration
 *
 * Reference: Bazel BUILD files, CMake add_subdirectory(), Buck BUCK files
 * Course: CMU 15-410 (OS), Stanford CS 245 (Database System Principles)
 * ============================================================================
 */

void manifest_init(BuildWorkspace *ws, const char *name, const char *root_dir) {
    memset(ws, 0, sizeof(*ws));
    strncpy(ws->name, name, MANIFEST_NAME_LEN - 1);
    strncpy(ws->root_dir, root_dir, MANIFEST_PATH_LEN - 1);
    ws->num_projects = 0;
    ws->num_in_order = 0;
    ws->is_configured = false;
}

int manifest_add_project(BuildWorkspace *ws, const char *name,
                          const char *root_path, ProjectType type) {
    if (ws->num_projects >= MANIFEST_MAX_PROJECTS) return -1;
    /* Check duplicate */
    for (int i = 0; i < ws->num_projects; i++) {
        if (strcmp(ws->projects[i].name, name) == 0) return i;
    }
    int idx = ws->num_projects++;
    ProjectSpec *p = &ws->projects[idx];
    memset(p, 0, sizeof(*p));
    strncpy(p->name, name, MANIFEST_NAME_LEN - 1);
    strncpy(p->root_path, root_path, MANIFEST_PATH_LEN - 1);
    p->type = type;
    p->num_targets = 0;
    p->num_deps = 0;
    p->enabled = true;
    return idx;
}

int manifest_add_target(BuildWorkspace *ws, int proj_idx, const char *name,
                         const char *source_dir, ProjectType type) {
    if (proj_idx < 0 || proj_idx >= ws->num_projects) return -1;
    ProjectSpec *p = &ws->projects[proj_idx];
    if (p->num_targets >= MANIFEST_MAX_TARGETS) return -1;
    int idx = p->num_targets++;
    TargetSpec *t = &p->targets[idx];
    memset(t, 0, sizeof(*t));
    strncpy(t->name, name, MANIFEST_NAME_LEN - 1);
    strncpy(t->source_dir, source_dir, MANIFEST_PATH_LEN - 1);
    t->type = type;
    t->num_deps = 0;
    t->is_default = false;
    t->requires_toolchain = false;
    return idx;
}

bool manifest_add_target_dep(BuildWorkspace *ws, int proj_idx,
                              int tgt_idx, const char *dep_name) {
    if (proj_idx < 0 || proj_idx >= ws->num_projects) return false;
    ProjectSpec *p = &ws->projects[proj_idx];
    if (tgt_idx < 0 || tgt_idx >= p->num_targets) return false;
    TargetSpec *t = &p->targets[tgt_idx];
    if (t->num_deps >= MANIFEST_MAX_DEPS) return false;
    strncpy(t->deps[t->num_deps], dep_name, MANIFEST_NAME_LEN - 1);
    t->num_deps++;
    return true;
}

bool manifest_add_project_dep(BuildWorkspace *ws, int proj_idx,
                               const char *dep_name) {
    if (proj_idx < 0 || proj_idx >= ws->num_projects) return false;
    ProjectSpec *p = &ws->projects[proj_idx];
    if (p->num_deps >= MANIFEST_MAX_DEPS) return false;
    strncpy(p->deps[p->num_deps], dep_name, MANIFEST_NAME_LEN - 1);
    p->num_deps++;
    return true;
}

int manifest_find_project(const BuildWorkspace *ws, const char *name) {
    for (int i = 0; i < ws->num_projects; i++) {
        if (strcmp(ws->projects[i].name, name) == 0) return i;
    }
    return -1;
}

/* L3: Engineering Structure - Dependency resolution with cycle detection.
 *
 * Uses DFS with white-gray-black coloring (Tarjan's algorithm variant)
 * to detect circular dependencies between projects.
 *
 * Reference: Cormen, Leiserson, Rivest, Stein "Introduction to Algorithms"
 *            Section 22.3 - Depth-First Search
 *            Section 22.4 - Topological Sort
 */
typedef enum { WHITE, GRAY, BLACK } DfsColor;

static bool dfs_cycle_detect(const BuildWorkspace *ws, int proj_idx,
                              DfsColor *color) {
    color[proj_idx] = GRAY;
    const ProjectSpec *p = &ws->projects[proj_idx];
    for (int i = 0; i < p->num_deps; i++) {
        int dep_idx = manifest_find_project(ws, p->deps[i]);
        if (dep_idx < 0) continue;
        if (color[dep_idx] == GRAY) return true;  /* back edge -> cycle */
        if (color[dep_idx] == WHITE) {
            if (dfs_cycle_detect(ws, dep_idx, color)) return true;
        }
    }
    color[proj_idx] = BLACK;
    return false;
}

bool manifest_has_cycles(const BuildWorkspace *ws) {
    DfsColor color[MANIFEST_MAX_PROJECTS];
    for (int i = 0; i < ws->num_projects; i++) color[i] = WHITE;
    for (int i = 0; i < ws->num_projects; i++) {
        if (color[i] == WHITE) {
            if (dfs_cycle_detect(ws, i, color)) return true;
        }
    }
    return false;
}

/* L3: Topological sort of projects using Kahn's algorithm.
 *
 * Kahn, A.B. "Topological Sorting of Large Networks"
 * Communications of the ACM, Vol.5 No.11, 1962.
 * O(V + E) time complexity. */
bool manifest_compute_build_order(BuildWorkspace *ws) {
    ws->num_in_order = 0;

    int indegree[MANIFEST_MAX_PROJECTS];
    int queue[MANIFEST_MAX_PROJECTS];
    int qhead = 0, qtail = 0;

    /* Initialize indegree */
    for (int i = 0; i < ws->num_projects; i++) {
        indegree[i] = 0;
        queue[i] = -1;
    }

    /* Compute indegree */
    for (int i = 0; i < ws->num_projects; i++) {
        if (!ws->projects[i].enabled) continue;
        for (int j = 0; j < ws->projects[i].num_deps; j++) {
            int dep_idx = manifest_find_project(ws, ws->projects[i].deps[j]);
            if (dep_idx >= 0) indegree[i]++;
        }
    }

    /* Enqueue nodes with indegree 0 */
    for (int i = 0; i < ws->num_projects; i++) {
        if (ws->projects[i].enabled && indegree[i] == 0) {
            queue[qtail++] = i;
        }
    }

    /* Process queue */
    while (qhead < qtail) {
        int cur = queue[qhead++];
        strncpy(ws->build_order[ws->num_in_order], ws->projects[cur].name,
                MANIFEST_NAME_LEN - 1);
        ws->num_in_order++;

        /* Decrease indegree of dependents */
        for (int i = 0; i < ws->num_projects; i++) {
            if (!ws->projects[i].enabled) continue;
            for (int j = 0; j < ws->projects[i].num_deps; j++) {
                if (strcmp(ws->projects[i].deps[j], ws->projects[cur].name) == 0) {
                    indegree[i]--;
                    if (indegree[i] == 0) {
                        queue[qtail++] = i;
                    }
                }
            }
        }
    }

    /* Count enabled projects */
    int enabled_count = 0;
    for (int i = 0; i < ws->num_projects; i++) {
        if (ws->projects[i].enabled) enabled_count++;
    }

    return ws->num_in_order == enabled_count;
}

bool manifest_resolve_deps(BuildWorkspace *ws) {
    if (manifest_has_cycles(ws)) return false;
    ws->is_configured = manifest_compute_build_order(ws);
    return ws->is_configured;
}

bool manifest_validate(const BuildWorkspace *ws) {
    if (ws->num_projects == 0) return false;
    if (!ws->is_configured) return false;
    for (int i = 0; i < ws->num_projects; i++) {
        if (ws->projects[i].enabled && ws->projects[i].num_targets == 0) {
            /* Projects can be aggregator-only (no targets) in some conventions */
        }
    }
    return true;
}

/* L6: Canonical Problem - Pretty-printing multi-project build manifests.
 *
 * In production build systems, this function outputs data consumed by
 * IDEs (compile_commands.json, project generators) and CI pipelines. */
void manifest_print(const BuildWorkspace *ws) {
    printf("\n=== Build Workspace: %s ===\n", ws->name);
    printf("  Root: %s\n", ws->root_dir);
    printf("  Projects: %d | Configured: %s\n",
           ws->num_projects, ws->is_configured ? "yes" : "no");

    for (int i = 0; i < ws->num_projects; i++) {
        const ProjectSpec *p = &ws->projects[i];
        const char *type_str = "library";
        switch (p->type) {
            case PROJECT_EXECUTABLE:  type_str = "executable"; break;
            case PROJECT_TEST:        type_str = "test"; break;
            case PROJECT_BENCHMARK:   type_str = "benchmark"; break;
            case PROJECT_HEADER_ONLY: type_str = "header-only"; break;
            default: break;
        }
        printf("  [%d] %s (%s) [%s]\n", i, p->name, type_str,
               p->enabled ? "enabled" : "disabled");
        printf("      Path: %s\n", p->root_path);
        if (p->num_deps > 0) {
            printf("      Project deps: ");
            for (int j = 0; j < p->num_deps; j++)
                printf("%s%s", p->deps[j], j < p->num_deps - 1 ? ", " : "");
            printf("\n");
        }
        printf("      Targets: %d\n", p->num_targets);
        for (int t = 0; t < p->num_targets; t++) {
            const TargetSpec *ts = &p->targets[t];
            printf("        [%d] %s (%s)\n", t, ts->name, ts->source_dir);
            if (ts->num_deps > 0) {
                printf("            deps: ");
                for (int d = 0; d < ts->num_deps; d++)
                    printf("%s%s", ts->deps[d], d < ts->num_deps - 1 ? ", " : "");
                printf("\n");
            }
        }
    }
    printf("===================================\n");
}

void manifest_print_build_order(const BuildWorkspace *ws) {
    printf("\n=== Build Order (%d projects) ===\n", ws->num_in_order);
    for (int i = 0; i < ws->num_in_order; i++) {
        printf("  %2d. %s\n", i + 1, ws->build_order[i]);
    }
    printf("=================================\n");
}
