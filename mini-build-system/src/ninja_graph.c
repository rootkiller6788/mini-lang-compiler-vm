#define _CRT_SECURE_NO_WARNINGS
#include "ninja_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *my_strtok_r(char *str, const char *delim, char **saveptr) {
    char *end;
    if (str == NULL) str = *saveptr;
    if (*str == '\0') { *saveptr = str; return NULL; }
    str += strspn(str, delim);
    if (*str == '\0') { *saveptr = str; return NULL; }
    end = str + strcspn(str, delim);
    if (*end == '\0') { *saveptr = end; return str; }
    *end = '\0';
    *saveptr = end + 1;
    return str;
}

static void trim_line(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

static char *trim_leading(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static char *trim_trailing(char *s) {
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t')) end--;
    *(end + 1) = '\0';
    return s;
}

int ninja_find_node(NinjaBuild *nb, const char *path) {
    for (int i = 0; i < nb->num_nodes; i++) {
        if (strcmp(nb->nodes[i].path, path) == 0) return i;
    }
    return -1;
}

int ninja_add_node(NinjaBuild *nb, const char *path, NinjaNodeType type) {
    int idx = ninja_find_node(nb, path);
    if (idx >= 0) return idx;
    if (nb->num_nodes >= NINJA_MAX_NODES) return -1;
    idx = nb->num_nodes++;
    strncpy(nb->nodes[idx].path, path, sizeof(nb->nodes[idx].path) - 1);
    nb->nodes[idx].type = type;
    nb->nodes[idx].mtime = 0;
    nb->nodes[idx].dirty = NINJA_NODE_CLEAN;
    nb->nodes[idx].exists = true;
    nb->nodes[idx].critical_path = 0;
    return idx;
}

NinjaEdge *ninja_find_edge(NinjaBuild *nb, const char *output) {
    for (int i = 0; i < nb->num_edges; i++) {
        if (strcmp(nb->edges[i].outputs[0], output) == 0)
            return &nb->edges[i];
    }
    return NULL;
}

static char build_vars[NINJA_MAX_VARS][2][256];
static int build_var_count = 0;

static const char *lookup_build_var(const char *name) {
    for (int i = 0; i < build_var_count; i++) {
        if (strcmp(build_vars[i][0], name) == 0)
            return build_vars[i][1];
    }
    return NULL;
}

static void set_build_var(const char *name, const char *value) {
    for (int i = 0; i < build_var_count; i++) {
        if (strcmp(build_vars[i][0], name) == 0) {
            strncpy(build_vars[i][1], value, 255);
            return;
        }
    }
    if (build_var_count < NINJA_MAX_VARS) {
        strncpy(build_vars[build_var_count][0], name, 255);
        strncpy(build_vars[build_var_count][1], value, 255);
        build_var_count++;
    }
}

static void expand_ninja_var(char *line, size_t max_len) {
    char result[NINJA_MAX_LINE] = {0};
    char *src = line;
    char *dst = result;
    while (*src && (size_t)(dst - result) < max_len - 1) {
        if (*src == '$') {
            src++;
            if (*src == '$') {
                *dst++ = '$';
                src++;
            } else if (*src == '{') {
                src++;
                char vname[128] = {0};
                int vi = 0;
                while (*src && *src != '}' && vi < 127)
                    vname[vi++] = *src++;
                if (*src == '}') src++;
                const char *val = lookup_build_var(vname);
                if (val) {
                    strcpy(dst, val);
                    dst += strlen(val);
                }
            } else {
                char vname[128] = {0};
                int vi = 0;
                while (*src && !isspace((unsigned char)*src) && *src != ':' &&
                       *src != '|' && vi < 127)
                    vname[vi++] = *src++;
                const char *val = lookup_build_var(vname);
                if (val && vi > 0) {
                    strcpy(dst, val);
                    dst += strlen(val);
                }
            }
        } else if (*src == '#') {
            break;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    strncpy(line, result, max_len - 1);
    line[max_len - 1] = '\0';
}

static void parse_build_line(NinjaBuild *nb, char *line) {
    if (nb->num_edges >= NINJA_MAX_EDGES) return;

    char *colon = strchr(line, ':');
    if (!colon) return;

    *colon = '\0';
    char *outputs_part = trim_leading(line);
    char *after_colon = colon + 1;

    char *pipe = strchr(after_colon, '|');
    char *pipe2 = pipe ? strchr(pipe + 1, '|') : NULL;
    char *explicit_part = after_colon;
    char *implicit_part = NULL;
    char *order_only_part = NULL;

    if (pipe && pipe2) {
        *pipe = '\0';
        *pipe2 = '\0';
        explicit_part = after_colon;
        implicit_part = pipe + 1;
        order_only_part = pipe2 + 1;
    } else if (pipe) {
        *pipe = '\0';
        char *colon2 = strchr(pipe + 1, ':');
        if (colon2) {
            implicit_part = pipe + 1;
            order_only_part = colon2 + 1;
        } else {
            order_only_part = pipe + 1;
        }
    }

    char *save;
    char *tok;

    char *rule_name = my_strtok_r(explicit_part, " \t", &save);
    if (!rule_name) return;

    NinjaEdge *edge = &nb->edges[nb->num_edges++];
    memset(edge, 0, sizeof(*edge));
    strncpy(edge->rule_name, rule_name, 127);

    tok = my_strtok_r(outputs_part, " \t", &save);
    if (tok) {
        strncpy(edge->outputs[0], tok, 255);
        ninja_add_node(nb, tok, NINJA_NODE_FILE);
    }

    edge->num_inputs = 0;
    while ((tok = my_strtok_r(NULL, " \t", &save)) != NULL) {
        if (edge->num_inputs < NINJA_MAX_INPUTS) {
            strncpy(edge->inputs[edge->num_inputs], tok, 255);
            ninja_add_node(nb, tok, NINJA_NODE_FILE);
            edge->num_inputs++;
        }
    }

    if (implicit_part) {
        char *isave;
        char *itok = my_strtok_r(implicit_part, " \t", &isave);
        edge->num_implicit = 0;
        while (itok && edge->num_implicit < NINJA_MAX_IMPLICIT) {
            strncpy(edge->implicit_inputs[edge->num_implicit], itok, 255);
            ninja_add_node(nb, itok, NINJA_NODE_FILE);
            edge->num_implicit++;
            itok = my_strtok_r(NULL, " \t", &isave);
        }
    }

    if (order_only_part) {
        char *osave;
        char *otok = my_strtok_r(order_only_part, " \t", &osave);
        edge->num_order_only = 0;
        while (otok && edge->num_order_only < NINJA_MAX_ORDER_ONLY) {
            strncpy(edge->order_only_deps[edge->num_order_only], otok, 255);
            ninja_add_node(nb, otok, NINJA_NODE_FILE);
            edge->num_order_only++;
            otok = my_strtok_r(NULL, " \t", &osave);
        }
    }
}

bool ninja_parse(NinjaBuild *nb, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return false;

    memset(nb, 0, sizeof(*nb));
    build_var_count = 0;
    char line[NINJA_MAX_LINE];
    char current_rule_name[128] = {0};

    while (fgets(line, sizeof(line), fp)) {
        trim_line(line);

        if (line[0] == '#' || line[0] == '\0') continue;

        if (strncmp(line, "rule ", 5) == 0) {
            strncpy(current_rule_name, line + 5, 127);
            current_rule_name[127] = '\0';
        } else if (strncmp(line, "build ", 6) == 0) {
            expand_ninja_var(line, sizeof(line));
            parse_build_line(nb, line);
        } else if (strncmp(line, "default ", 8) == 0) {
            char *dlist = line + 8;
            char *save;
            char *tok = my_strtok_r(dlist, " \t", &save);
            while (tok && nb->num_default_targets < 8) {
                strncpy(nb->default_targets[nb->num_default_targets++], tok, 255);
                tok = my_strtok_r(NULL, " \t", &save);
            }
        } else if (strncmp(line, "  ", 2) == 0 || line[0] == ' ') {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *name = trim_leading(line);
                trim_trailing(name);
                char *value = trim_leading(eq + 1);
                trim_trailing(value);
                set_build_var(name, value);
            } else if (current_rule_name[0]) {
                char *eq2 = strchr(line, '=');
                if (eq2) {
                    *eq2 = '\0';
                    char *name = trim_leading(line);
                    trim_trailing(name);
                    char *value = trim_leading(eq2 + 1);
                    trim_trailing(value);
                    char fullname[256];
                    snprintf(fullname, sizeof(fullname), "%s.%s", current_rule_name, name);
                    set_build_var(fullname, value);
                }
            }
        } else if (strchr(line, '=')) {
            char *eq = strchr(line, '=');
            *eq = '\0';
            char *name = trim_leading(line);
            trim_trailing(name);
            char *value = trim_leading(eq + 1);
            trim_trailing(value);
            set_build_var(name, value);
        }
    }
    fclose(fp);
    return true;
}

void ninja_compute_dirty(NinjaBuild *nb) {
    for (int i = 0; i < nb->num_nodes; i++) {
        nb->nodes[i].dirty = NINJA_NODE_CLEAN;
        FILE *fp = fopen(nb->nodes[i].path, "r");
        nb->nodes[i].exists = (fp != NULL);
        if (fp) fclose(fp);
    }

    for (int i = 0; i < nb->num_edges; i++) {
        NinjaEdge *edge = &nb->edges[i];
        int out_idx = ninja_find_node(nb, edge->outputs[0]);
        if (out_idx < 0) continue;

        bool inputs_dirty = false;
        for (int j = 0; j < edge->num_inputs; j++) {
            int in_idx = ninja_find_node(nb, edge->inputs[j]);
            if (in_idx >= 0 && nb->nodes[in_idx].dirty != NINJA_NODE_CLEAN) {
                inputs_dirty = true;
                break;
            }
        }
        if (!nb->nodes[out_idx].exists) inputs_dirty = true;

        if (inputs_dirty)
            nb->nodes[out_idx].dirty = NINJA_NODE_DIRTY;
    }
}

static int compute_cp(NinjaBuild *nb, int node_idx, bool *visiting, bool *visited) {
    if (visited[node_idx]) return nb->nodes[node_idx].critical_path;
    if (visiting[node_idx]) return 0; /* cycle */

    visiting[node_idx] = true;
    int max_cp = 1;

    for (int i = 0; i < nb->num_edges; i++) {
        NinjaEdge *edge = &nb->edges[i];
        if (strcmp(edge->outputs[0], nb->nodes[node_idx].path) == 0) continue;

        for (int j = 0; j < edge->num_inputs; j++) {
            if (strcmp(edge->inputs[j], nb->nodes[node_idx].path) == 0) {
                int out_idx = ninja_find_node(nb, edge->outputs[0]);
                if (out_idx >= 0 && out_idx != node_idx) {
                    int cp = compute_cp(nb, out_idx, visiting, visited) + 1;
                    if (cp > max_cp) max_cp = cp;
                }
            }
        }
    }

    visiting[node_idx] = false;
    visited[node_idx] = true;
    nb->nodes[node_idx].critical_path = max_cp;
    return max_cp;
}

void ninja_compute_critical_path(NinjaBuild *nb) {
    bool visiting[NINJA_MAX_NODES];
    bool visited[NINJA_MAX_NODES];
    memset(visiting, 0, sizeof(visiting));
    memset(visited, 0, sizeof(visited));
    for (int i = 0; i < nb->num_nodes; i++) {
        if (!visited[i])
            compute_cp(nb, i, visiting, visited);
    }
}

void ninja_schedule(NinjaBuild *nb) {
    ninja_compute_critical_path(nb);
    ninja_compute_dirty(nb);

    printf("\n=== Ninja Build Schedule (by critical path) ===\n");
    for (int i = 0; i < nb->num_nodes; i++) {
        NinjaNode *n = &nb->nodes[i];
        if (n->dirty != NINJA_NODE_CLEAN) {
            printf("  [D] %s (cp=%d)\n", n->path, n->critical_path);
        }
    }
    printf("================================================\n");
}

void ninja_execute(NinjaBuild *nb, const char *target) {
    int tgt_idx = ninja_find_node(nb, target);
    ninja_compute_dirty(nb);

    printf("[ninja] Building target: %s\n", target);

    for (int i = 0; i < nb->num_edges; i++) {
        NinjaEdge *edge = &nb->edges[i];
        bool should_build = false;

        int out_idx = ninja_find_node(nb, edge->outputs[0]);
        if (out_idx >= 0 && nb->nodes[out_idx].dirty != NINJA_NODE_CLEAN)
            should_build = true;

        if (tgt_idx >= 0) {
            if (strcmp(edge->outputs[0], target) == 0)
                should_build = true;
            for (int j = 0; j < edge->num_inputs; j++) {
                if (strcmp(edge->inputs[j], target) == 0) {
                    should_build = true;
                    break;
                }
            }
        }

        if (should_build) {
            printf("  [%s] %s", edge->rule_name, edge->outputs[0]);
            if (edge->num_inputs > 0) {
                printf(" : ");
                for (int j = 0; j < edge->num_inputs; j++)
                    printf("%s ", edge->inputs[j]);
            }
            printf("\n");

            if (out_idx >= 0) {
                nb->nodes[out_idx].dirty = NINJA_NODE_CLEAN;
                nb->nodes[out_idx].exists = true;
            }
        }
    }

    if (tgt_idx >= 0)
        nb->nodes[tgt_idx].dirty = NINJA_NODE_CLEAN;

    printf("[ninja] Build complete: %s\n", target);
}

void ninja_print_graph(const NinjaBuild *nb) {
    printf("\n=== Ninja Build Graph ===\n");
    printf("Nodes (%d):\n", nb->num_nodes);
    for (int i = 0; i < nb->num_nodes; i++) {
        const NinjaNode *n = &nb->nodes[i];
        const char *type_str = n->type == NINJA_NODE_FILE ? "FILE" :
                                n->type == NINJA_NODE_RULE ? "RULE" : "PHONY";
        const char *dirty_str = n->dirty == NINJA_NODE_CLEAN ? "clean" :
                                 n->dirty == NINJA_NODE_DIRTY ? "dirty" : "restat";
        printf("  [%d] %s (%s, %s, cp=%d)\n", i, n->path, type_str,
               dirty_str, n->critical_path);
    }
    printf("Edges (%d):\n", nb->num_edges);
    for (int i = 0; i < nb->num_edges; i++) {
        const NinjaEdge *e = &nb->edges[i];
        printf("  %s: %s <- [", e->outputs[0], e->rule_name);
        for (int j = 0; j < e->num_inputs; j++)
            printf("%s%s", e->inputs[j], j < e->num_inputs - 1 ? ", " : "");
        printf("]");
        if (e->num_order_only > 0) {
            printf(" order_only:[");
            for (int j = 0; j < e->num_order_only; j++)
                printf("%s%s", e->order_only_deps[j],
                       j < e->num_order_only - 1 ? ", " : "");
            printf("]");
        }
        printf("\n");
    }
    printf("==========================\n");
}

const char *ninja_lookup_var(const NinjaBuild *nb, const char *name) {
    (void)nb;
    return lookup_build_var(name);
}
