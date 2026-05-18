#define _CRT_SECURE_NO_WARNINGS
#include "make_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *my_strtok_r(char *str, const char *delim, char **saveptr) {
    char *end;
    if (str == NULL) str = *saveptr;
    if (*str == '\0') {
        *saveptr = str;
        return NULL;
    }
    str += strspn(str, delim);
    if (*str == '\0') {
        *saveptr = str;
        return NULL;
    }
    end = str + strcspn(str, delim);
    if (*end == '\0') {
        *saveptr = end;
        return str;
    }
    *end = '\0';
    *saveptr = end + 1;
    return str;
}

static void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

static void strip_leading_tab(char *s) {
    char *p = s;
    while (*p == '\t' || *p == ' ')
        p++;
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
}

static void expand_variable(MakeFile *mf, char *line, size_t max_len) {
    char result[MAX_LINE_LEN] = {0};
    char *src = line;
    char *dst = result;

    while (*src && (size_t)(dst - result) < max_len - 1) {
        if (src[0] == '$' && src[1] == '(') {
            char var_name[MAX_VAR_NAME] = {0};
            char *close = strchr(src + 2, ')');
            if (close) {
                size_t vlen = close - (src + 2);
                if (vlen >= MAX_VAR_NAME) vlen = MAX_VAR_NAME - 1;
                memcpy(var_name, src + 2, vlen);
                for (int i = 0; i < mf->num_vars; i++) {
                    if (strcmp(mf->variables[i].name, var_name) == 0) {
                        strcpy(dst, mf->variables[i].value);
                        dst += strlen(mf->variables[i].value);
                        break;
                    }
                }
                src = close + 1;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    strcpy(line, result);
}

static bool is_rule_line(const char *line) {
    const char *p = line;
    while (*p && *p != ':' && *p != '=' && *p != '\t' && *p != ' ')
        p++;
    return *p == ':';
}

static bool is_var_line(const char *line) {
    const char *eq = strchr(line, '=');
    if (!eq) return false;
    const char *colon = strchr(line, ':');
    return !colon || eq < colon;
}

static void parse_rule_line(MakeFile *mf, char *line) {
    if (mf->num_rules >= MAX_RULES) return;
    MakeRule *rule = &mf->rules[mf->num_rules];

    char *colon = strchr(line, ':');
    if (!colon) return;
    *colon = '\0';
    char *target = line;
    while (*target == ' ' || *target == '\t') target++;
    char *tend = target + strlen(target) - 1;
    while (tend > target && (*tend == ' ' || *tend == '\t')) *tend-- = '\0';

    strncpy(rule->target, target, sizeof(rule->target) - 1);

    char *prereqs = colon + 1;
    while (*prereqs == ' ' || *prereqs == '\t') prereqs++;
    trim_newline(prereqs);

    char *save;
    char *token = my_strtok_r(prereqs, " \t", &save);
    rule->num_prereqs = 0;
    while (token && rule->num_prereqs < MAX_PREREQS) {
        strncpy(rule->prerequisites[rule->num_prereqs], token,
                sizeof(rule->prerequisites[0]) - 1);
        rule->num_prereqs++;
        token = my_strtok_r(NULL, " \t", &save);
    }

    rule->implicit = (strcmp(rule->target, ".PHONY") == 0);
    rule->num_commands = 0;
    rule->is_pattern = (strchr(rule->target, '%') != NULL);

    mf->num_rules++;
}

static void parse_command_line(MakeFile *mf, char *line) {
    if (mf->num_rules == 0) return;
    MakeRule *rule = &mf->rules[mf->num_rules - 1];
    if (rule->num_commands >= MAX_COMMANDS) return;
    strip_leading_tab(line);
    trim_newline(line);
    strncpy(rule->commands[rule->num_commands], line,
            sizeof(rule->commands[0]) - 1);
    rule->num_commands++;
}

static void build_target_list(MakeFile *mf) {
    mf->num_targets = 0;
    for (int i = 0; i < mf->num_rules; i++) {
        MakeRule *rule = &mf->rules[i];
        if (mf->num_targets >= MAX_TARGETS) break;

        if (strcmp(rule->target, ".PHONY") == 0) {
            for (int j = 0; j < rule->num_prereqs; j++) {
                MakeTarget *t = &mf->targets[mf->num_targets++];
                strncpy(t->name, rule->prerequisites[j], sizeof(t->name) - 1);
                t->rule = NULL;
                t->dirty = true;
                t->built = false;
                t->timestamp = 0;
                t->type = TARGET_PHONY;
            }
            continue;
        }

        MakeTarget *t = &mf->targets[mf->num_targets++];
        strncpy(t->name, rule->target, sizeof(t->name) - 1);
        t->rule = rule;
        t->dirty = false;
        t->built = false;
        t->timestamp = 0;
        t->type = rule->is_pattern ? TARGET_PATTERN : TARGET_NORMAL;
    }
}

static void parse_variable_line(MakeFile *mf, char *line) {
    char *eq = strchr(line, '=');
    if (!eq) return;
    *eq = '\0';
    char *name = line;
    char *value = eq + 1;

    while (*name == ' ' || *name == '\t') name++;
    char *nend = name + strlen(name) - 1;
    while (nend > name && (*nend == ' ' || *nend == '\t')) *nend-- = '\0';

    while (*value == ' ' || *value == '\t') value++;
    trim_newline(value);

    if (mf->num_vars < MAX_VARIABLES) {
        strncpy(mf->variables[mf->num_vars].name, name, MAX_VAR_NAME - 1);
        strncpy(mf->variables[mf->num_vars].value, value, MAX_VAR_VALUE - 1);
        mf->num_vars++;
    }
}

bool make_parse(MakeFile *mf, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return false;

    memset(mf, 0, sizeof(*mf));

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        if (line[0] == '\t' || line[0] == ' ') {
            if (line[0] == '\t') {
                parse_command_line(mf, line);
            }
        } else if (is_var_line(line)) {
            parse_variable_line(mf, line);
        } else if (is_rule_line(line)) {
            parse_rule_line(mf, line);
        }
    }
    fclose(fp);

    build_target_list(mf);
    if (mf->num_rules > 0 && strlen(mf->default_target) == 0) {
        strncpy(mf->default_target, mf->rules[0].target, 127);
    }
    return true;
}

MakeTarget *make_find_target(MakeFile *mf, const char *target_name) {
    for (int i = 0; i < mf->num_targets; i++) {
        if (strcmp(mf->targets[i].name, target_name) == 0)
            return &mf->targets[i];
    }
    return NULL;
}

bool make_target_is_dirty(const MakeFile *mf, const MakeTarget *target) {
    if (target->type == TARGET_PHONY) return true;
    if (!target->rule) return false;

    time_t tgt_time = time(NULL);
    FILE *fp = fopen(target->name, "r");
    if (fp) {
        fclose(fp);
    }

    for (int i = 0; i < target->rule->num_prereqs; i++) {
        const char *prereq = target->rule->prerequisites[i];
        MakeTarget *pt = make_find_target((MakeFile *)mf, prereq);
        if (pt && pt->dirty) return true;

        FILE *pf = fopen(prereq, "r");
        if (pf) {
            fclose(pf);
            if (tgt_time == 0) return true;
        }
    }
    return target->dirty;
}

static bool build_recursive(MakeFile *mf, const char *target_name, int depth) {
    if (depth > 64) return false;

    MakeTarget *target = make_find_target(mf, target_name);
    if (!target) {
        printf("[make] Target '%s' not found (maybe a file)\n", target_name);
        return true;
    }
    if (target->built) return true;
    if (target->type == TARGET_PHONY) {
        target->built = true;
        return true;
    }
    if (!target->rule) {
        target->built = true;
        return true;
    }

    for (int i = 0; i < target->rule->num_prereqs; i++) {
        const char *prereq = target->rule->prerequisites[i];
        if (!build_recursive(mf, prereq, depth + 1))
            return false;
    }

    if (make_target_is_dirty(mf, target)) {
        printf("[make] Building target: %s\n", target->name);
        make_execute_recipe(target->rule);
        target->timestamp = time(NULL);
    } else {
        printf("[make] Target '%s' is up to date\n", target->name);
    }

    target->built = true;
    target->dirty = false;
    return true;
}

bool make_build(MakeFile *mf, const char *target_name) {
    const char *tgt = target_name;
    if (!tgt || strlen(tgt) == 0)
        tgt = mf->default_target;

    for (int i = 0; i < mf->num_targets; i++) {
        mf->targets[i].built = false;
        mf->targets[i].dirty = false;
    }
    return build_recursive(mf, tgt, 0);
}

void make_resolve_vars(MakeFile *mf) {
    for (int i = 0; i < mf->num_vars; i++) {
        char expanded[MAX_VAR_VALUE];
        strncpy(expanded, mf->variables[i].value, sizeof(expanded) - 1);
        expand_variable(mf, expanded, sizeof(expanded));
        strncpy(mf->variables[i].value, expanded, sizeof(mf->variables[i].value) - 1);
    }
}

void make_print_graph(const MakeFile *mf) {
    printf("\n=== Make Dependency Graph ===\n");
    for (int i = 0; i < mf->num_rules; i++) {
        const MakeRule *rule = &mf->rules[i];
        if (strcmp(rule->target, ".PHONY") == 0) continue;
        printf("  [%s]", rule->target);
        if (rule->num_prereqs > 0) {
            printf(" -> ");
            for (int j = 0; j < rule->num_prereqs; j++) {
                printf("%s%s", rule->prerequisites[j],
                       j < rule->num_prereqs - 1 ? ", " : "");
            }
        }
        if (rule->implicit) printf(" (implicit)");
        if (rule->is_pattern) printf(" (pattern)");
        printf("\n");
    }
    printf("Variables:\n");
    for (int i = 0; i < mf->num_vars; i++) {
        printf("  %s = %s\n", mf->variables[i].name, mf->variables[i].value);
    }
    printf("=============================\n");
}

void make_expand_auto_vars(MakeRule *rule, const char *target,
                           const char *first_prereq) {
    for (int c = 0; c < rule->num_commands; c++) {
        char expanded[MAX_LINE_LEN];
        char *src = rule->commands[c];
        char *dst = expanded;

        while (*src) {
            if (src[0] == '$' && src[1] == '@' &&
                (src[2] == ' ' || src[2] == '\0' || src[2] == '\n')) {
                strcpy(dst, target);
                dst += strlen(target);
                src += 2;
            } else if (src[0] == '$' && src[1] == '<' &&
                       (src[2] == ' ' || src[2] == '\0' || src[2] == '\n')) {
                if (first_prereq) {
                    strcpy(dst, first_prereq);
                    dst += strlen(first_prereq);
                }
                src += 2;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
        strncpy(rule->commands[c], expanded, sizeof(rule->commands[0]) - 1);
    }
}

bool make_match_pattern(const char *pattern, const char *target,
                        char *stem, size_t stem_size) {
    const char *pp = strchr(pattern, '%');
    if (!pp) return strcmp(pattern, target) == 0;

    size_t prefix_len = pp - pattern;
    if (strncmp(target, pattern, prefix_len) != 0) return false;

    const char *suffix = pp + 1;
    size_t suffix_len = strlen(suffix);
    size_t tgt_len = strlen(target);

    if (tgt_len < prefix_len + suffix_len) return false;
    if (strcmp(target + tgt_len - suffix_len, suffix) != 0) return false;

    size_t stem_len = tgt_len - prefix_len - suffix_len;
    if (stem_len >= stem_size) return false;
    memcpy(stem, target + prefix_len, stem_len);
    stem[stem_len] = '\0';
    return true;
}

MakeRule *make_find_rule_for_target(const MakeFile *mf, const char *target_name) {
    for (int i = 0; i < mf->num_rules; i++) {
        if (strcmp(mf->rules[i].target, target_name) == 0)
            return (MakeRule *)&mf->rules[i];
    }
    for (int i = 0; i < mf->num_rules; i++) {
        if (mf->rules[i].is_pattern) {
            char stem[128];
            if (make_match_pattern(mf->rules[i].target, target_name, stem, sizeof(stem)))
                return (MakeRule *)&mf->rules[i];
        }
    }
    return NULL;
}

void make_execute_recipe(const MakeRule *rule) {
    for (int i = 0; i < rule->num_commands; i++) {
        printf("    $ %s\n", rule->commands[i]);
    }
}
