#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define AST_MAX_CHILDREN 16

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DEF,
    AST_BLOCK,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_DO_WHILE_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_RETURN_STMT,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_INT_LIT,
    AST_IDENT,
    AST_ASSIGN,
    AST_CALL,
    AST_PARAM,
    AST_VAR_DECL,
    AST_STRING_LIT
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char name[256];
    int int_value;
    char op;
    struct ASTNode *children[AST_MAX_CHILDREN];
    int child_count;
    int line;
    int col;
} ASTNode;

typedef void (*ASTVisitCallback)(ASTNode *node, void *user_data);

ASTNode *ast_create_node(ASTNodeType type);
void ast_set_name(ASTNode *node, const char *name);
void ast_add_child(ASTNode *parent, ASTNode *child);
ASTNode *ast_get_child(const ASTNode *node, int index);
void ast_print_tree(const ASTNode *node, int indent);
void ast_free(ASTNode *node);
void ast_visit_preorder(ASTNode *node, ASTVisitCallback cb, void *user_data);
void ast_visit_postorder(ASTNode *node, ASTVisitCallback cb, void *user_data);
const char *ast_node_type_name(ASTNodeType type);

/* ─── Advanced AST Operations (L5: Algorithms) ────────────────────── */

/* Deep clone an AST subtree. O(n). */
ASTNode *ast_clone(const ASTNode *node);

/* Structural equality comparison. O(min(n1, n2)). */
bool ast_equals(const ASTNode *a, const ASTNode *b);

/* Count total nodes in subtree. O(n). */
int ast_node_count(const ASTNode *node);

/* Compute maximum tree depth. O(n). */
int ast_height(const ASTNode *node);

/* Breadth-first (level-order) traversal. O(n). */
void ast_visit_levelorder(ASTNode *node, ASTVisitCallback cb, void *user_data);

/* Serialize to S-expression string (Lisp-like). Caller must free(). */
char *ast_to_sexpr(const ASTNode *node);

/* Export to Graphviz DOT format (for visualization). */
void ast_export_dot(const ASTNode *node, FILE *fp);

/* Constant expression evaluation (partial evaluation).
 * Returns true if the expression is a compile-time constant. */
bool ast_eval_const(const ASTNode *node, int *result);

#endif
