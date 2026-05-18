#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ast_type_names[] = {
    "PROGRAM", "FUNC_DEF", "BLOCK", "IF_STMT",
    "WHILE_STMT", "RETURN_STMT", "BINARY_OP", "UNARY_OP",
    "INT_LIT", "IDENT", "ASSIGN", "CALL",
    "PARAM", "VAR_DECL", "STRING_LIT"
};

const char *ast_node_type_name(ASTNodeType type) {
    if (type >= 0 && type <= AST_STRING_LIT) return ast_type_names[type];
    return "UNKNOWN";
}

ASTNode *ast_create_node(ASTNodeType type) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "ast error: failed to allocate node\n");
        exit(1);
    }
    node->type = type;
    node->name[0] = '\0';
    node->int_value = 0;
    node->op = '\0';
    node->child_count = 0;
    node->line = 0;
    node->col = 0;
    return node;
}

void ast_set_name(ASTNode *node, const char *name) {
    if (!node || !name) return;
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';
}

void ast_add_child(ASTNode *parent, ASTNode *child) {
    if (!parent || !child) return;
    if (parent->child_count >= AST_MAX_CHILDREN) {
        fprintf(stderr, "ast error: max children (%d) exceeded for node type %s\n",
                AST_MAX_CHILDREN, ast_node_type_name(parent->type));
        return;
    }
    parent->children[parent->child_count++] = child;
}

ASTNode *ast_get_child(const ASTNode *node, int index) {
    if (!node || index < 0 || index >= node->child_count) return NULL;
    return node->children[index];
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print_tree(const ASTNode *node, int indent) {
    if (!node) return;

    print_indent(indent);

    switch (node->type) {
    case AST_PROGRAM:
    case AST_BLOCK:
        printf("%s\n", ast_node_type_name(node->type));
        break;
    case AST_FUNC_DEF:
        printf("FUNC_DEF: %s\n", node->name[0] ? node->name : "<unnamed>");
        break;
    case AST_VAR_DECL:
        printf("VAR_DECL: %s\n", node->name);
        break;
    case AST_PARAM:
        printf("PARAM: %s\n", node->name);
        break;
    case AST_INT_LIT:
        printf("INT_LIT: %d\n", node->int_value);
        break;
    case AST_IDENT:
        printf("IDENT: %s\n", node->name);
        break;
    case AST_STRING_LIT:
        printf("STRING_LIT: \"%s\"\n", node->name);
        break;
    case AST_BINARY_OP:
        printf("BINARY_OP: '%c'\n", node->op);
        break;
    case AST_UNARY_OP:
        printf("UNARY_OP: '%c'\n", node->op);
        break;
    case AST_ASSIGN:
        printf("ASSIGN\n");
        break;
    case AST_CALL:
        printf("CALL: %s\n", node->name);
        break;
    case AST_IF_STMT:
        printf("IF_STMT\n");
        break;
    case AST_WHILE_STMT:
        printf("WHILE_STMT\n");
        break;
    case AST_RETURN_STMT:
        printf("RETURN_STMT\n");
        break;
    default:
        printf("%s\n", ast_node_type_name(node->type));
        break;
    }

    for (int i = 0; i < node->child_count; i++) {
        ast_print_tree(node->children[i], indent + 1);
    }
}

void ast_free(ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        ast_free(node->children[i]);
    }
    free(node);
}

void ast_visit_preorder(ASTNode *node, ASTVisitCallback cb, void *user_data) {
    if (!node || !cb) return;
    cb(node, user_data);
    for (int i = 0; i < node->child_count; i++) {
        ast_visit_preorder(node->children[i], cb, user_data);
    }
}

void ast_visit_postorder(ASTNode *node, ASTVisitCallback cb, void *user_data) {
    if (!node || !cb) return;
    for (int i = 0; i < node->child_count; i++) {
        ast_visit_postorder(node->children[i], cb, user_data);
    }
    cb(node, user_data);
}
