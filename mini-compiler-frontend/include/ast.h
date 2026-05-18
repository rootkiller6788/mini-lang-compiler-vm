#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>

#define AST_MAX_CHILDREN 16

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DEF,
    AST_BLOCK,
    AST_IF_STMT,
    AST_WHILE_STMT,
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

#endif
