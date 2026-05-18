#ifndef MAKE_ENGINE_H
#define MAKE_ENGINE_H

#include <stdbool.h>
#include <time.h>

#define MAX_PREREQS    16
#define MAX_COMMANDS    8
#define MAX_VARIABLES  64
#define MAX_RULES     256
#define MAX_TARGETS   256
#define MAX_LINE_LEN  512
#define MAX_VAR_NAME   64
#define MAX_VAR_VALUE 256

typedef enum {
    TARGET_NORMAL,
    TARGET_PHONY,
    TARGET_PATTERN
} TargetType;

typedef struct {
    char  target[128];
    char  prerequisites[MAX_PREREQS][128];
    int   num_prereqs;
    char  commands[MAX_COMMANDS][256];
    int   num_commands;
    bool  implicit;
    bool  is_pattern;
    char  pattern_target[128];
    char  pattern_prereq[128];
} MakeRule;

typedef struct {
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
} MakeVariable;

typedef struct {
    char      name[128];
    MakeRule *rule;
    bool      dirty;
    bool      built;
    time_t    timestamp;
    TargetType type;
} MakeTarget;

typedef struct {
    MakeRule     rules[MAX_RULES];
    int          num_rules;
    MakeVariable variables[MAX_VARIABLES];
    int          num_vars;
    MakeTarget   targets[MAX_TARGETS];
    int          num_targets;
    char         default_target[128];
} MakeFile;

bool make_parse(MakeFile *mf, const char *filepath);
bool make_build(MakeFile *mf, const char *target_name);
void make_resolve_vars(MakeFile *mf);
void make_print_graph(const MakeFile *mf);
void make_expand_auto_vars(MakeRule *rule, const char *target, const char *first_prereq);
bool make_match_pattern(const char *pattern, const char *target, char *stem, size_t stem_size);
MakeRule *make_find_rule_for_target(const MakeFile *mf, const char *target_name);
bool make_target_is_dirty(const MakeFile *mf, const MakeTarget *target);
void make_execute_recipe(const MakeRule *rule);

#endif
