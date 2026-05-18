#ifndef PATTERN_MATCH_H
#define PATTERN_MATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PM_MAX_CASES      64
#define PM_MAX_BINDINGS   32
#define PM_MAX_NODES      128
#define PM_MAX_NAME_LEN   32
#define PM_MAX_ARGS       16

typedef enum {
    P_WILD,
    P_VAR,
    P_INT,
    P_BOOL,
    P_STRING,
    P_CONS,
    P_TUPLE,
    P_OR,
    P_AS
} PatternTag;

typedef struct Pattern Pattern;
struct Pattern {
    PatternTag tag;
    union {
        int   int_val;
        bool  bool_val;
        char  string_val[PM_MAX_NAME_LEN];
        struct {
            char var_name[PM_MAX_NAME_LEN];
        } var;
        struct {
            char       cons_name[PM_MAX_NAME_LEN];
            Pattern*   sub_patterns[PM_MAX_ARGS];
            int        arity;
        } cons;
        struct {
            Pattern* elements[PM_MAX_ARGS];
            int      count;
        } tuple;
        struct {
            Pattern* left;
            Pattern* right;
        } or_pattern;
        struct {
            char     var_name[PM_MAX_NAME_LEN];
            Pattern* sub;
        } as_pattern;
    };
    Pattern* next;
};

typedef struct {
    char  name[PM_MAX_NAME_LEN];
    void* value;
} Binding;

typedef struct {
    Pattern* pattern;
    void*    expression;
} MatchCase;

typedef enum {
    DT_TEST_CONSTRUCTOR,
    DT_TEST_INT,
    DT_TEST_BOOL,
    DT_LEAF,
    DT_SWITCH
} DTNodeType;

typedef struct DTNode DTNode;
struct DTNode {
    DTNodeType type;
    union {
        struct {
            char    cons_name[PM_MAX_NAME_LEN];
            int     arity;
            DTNode* success;
            DTNode* failure;
        } test_cons;
        struct {
            int     value;
            DTNode* success;
            DTNode* failure;
        } test_int;
        struct {
            bool    value;
            DTNode* success;
            DTNode* failure;
        } test_bool;
        struct {
            Binding bindings[PM_MAX_BINDINGS];
            int     binding_count;
            void*   action;
        } leaf;
        struct {
            int     test_field;
            DTNode* branches[PM_MAX_CASES];
            int     branch_count;
            DTNode* default_branch;
        } sw;
    };
};

typedef struct {
    Pattern* pattern;
    int      value;
    bool     matched;
} MatchValue;

Pattern*  pattern_wild(void);
Pattern*  pattern_var(const char* name);
Pattern*  pattern_int(int value);
Pattern*  pattern_bool(bool value);
Pattern*  pattern_string(const char* s);
Pattern*  pattern_cons(const char* name, Pattern** subs, int arity);
Pattern*  pattern_tuple(Pattern** elems, int count);
Pattern*  pattern_or(Pattern* left, Pattern* right);
Pattern*  pattern_as(const char* name, Pattern* sub);
void      pattern_print(const Pattern* p);
void      pattern_destroy(Pattern* p);

MatchCase match_case_create(Pattern* pattern, void* expr);

DTNode*   match_compile(MatchCase* cases, int case_count);
void*     match_execute(DTNode* tree, MatchValue* value);
void      match_print_decision_tree(DTNode* node, int depth);
void      match_tree_destroy(DTNode* node);

bool      match_simple(Pattern* pattern, MatchValue* value, Binding* bindings, int* binding_count);
void      match_print_bindings(Binding* bindings, int count);

#endif
