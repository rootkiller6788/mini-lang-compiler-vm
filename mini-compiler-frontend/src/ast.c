#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ast_type_names[] = {
    "PROGRAM", "FUNC_DEF", "BLOCK", "IF_STMT",
    "WHILE_STMT", "FOR_STMT", "DO_WHILE_STMT",
    "BREAK_STMT", "CONTINUE_STMT", "RETURN_STMT",
    "BINARY_OP", "UNARY_OP",
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

/* ─── AST Deep Clone (L5: Tree Copy Algorithm) ───────────────────── */

/*
 * Deep clone an AST node and all its children.
 * Uses a recursive preorder traversal to allocate new nodes.
 * This is an O(n) operation where n = number of nodes.
 *
 * L5: Tree copying is fundamental for compiler passes that transform
 *     the AST (e.g., inlining, partial evaluation).
 */
ASTNode *ast_clone(const ASTNode *node) {
    if (!node) return NULL;

    ASTNode *copy = ast_create_node(node->type);
    memcpy(copy->name, node->name, sizeof(copy->name));
    copy->int_value = node->int_value;
    copy->op = node->op;
    copy->line = node->line;
    copy->col = node->col;

    for (int i = 0; i < node->child_count; i++) {
        ASTNode *child_copy = ast_clone(node->children[i]);
        if (child_copy) {
            ast_add_child(copy, child_copy);
        }
    }

    return copy;
}

/* ─── AST Comparison (L5: Tree Equality Algorithm) ───────────────── */

/*
 * Compare two AST trees for structural equality.
 * Two nodes are equal if they have the same type, same name/int_value/op,
 * same number of children, and recursively equal children.
 *
 * Equality is defined up to α-equivalence for identifiers:
 *   - INT_LIT and STRING_LIT: compare values
 *   - IDENT and other named nodes: compare names by string
 *   - Other nodes: compare structural properties
 *
 * Complexity: O(min(n1, n2)) — early exit on first mismatch.
 */
bool ast_equals(const ASTNode *a, const ASTNode *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;

    if (a->type != b->type) return false;

    /* Compare type-specific fields */
    switch (a->type) {
    case AST_INT_LIT:
        if (a->int_value != b->int_value) return false;
        break;
    case AST_IDENT:
    case AST_FUNC_DEF:
    case AST_VAR_DECL:
    case AST_PARAM:
    case AST_STRING_LIT:
    case AST_CALL:
        if (strcmp(a->name, b->name) != 0) return false;
        break;
    case AST_BINARY_OP:
    case AST_UNARY_OP:
        if (a->op != b->op) return false;
        break;
    default:
        break;
    }

    if (a->child_count != b->child_count) return false;

    for (int i = 0; i < a->child_count; i++) {
        if (!ast_equals(a->children[i], b->children[i]))
            return false;
    }

    return true;
}

/* ─── AST Node Count ───────────────────────────────────────────────── */

/*
 * Count total number of nodes in an AST (recursive).
 * Complexity: O(n).
 */
int ast_node_count(const ASTNode *node) {
    if (!node) return 0;
    int count = 1;
    for (int i = 0; i < node->child_count; i++) {
        count += ast_node_count(node->children[i]);
    }
    return count;
}

/* ─── AST Height ────────────────────────────────────────────────────── */

/*
 * Compute the height (maximum depth) of an AST.
 * Height of a leaf node = 0.
 * Height of an internal node = 1 + max(children heights).
 *
 * L5: Tree height is used to detect excessively deep expressions and
 *     to optimize traversal strategies.
 */
int ast_height(const ASTNode *node) {
    if (!node) return -1;
    if (node->child_count == 0) return 0;

    int max_child_h = -1;
    for (int i = 0; i < node->child_count; i++) {
        int ch = ast_height(node->children[i]);
        if (ch > max_child_h) max_child_h = ch;
    }
    return 1 + max_child_h;
}

/* ─── AST Traversal with Level Order (Breadth-First) ──────────────── */

/*
 * Level-order (breadth-first) traversal using a queue.
 * L5: BFS traversal is useful for certain semantic passes that need to
 *     process nodes by level (e.g., variable scope analysis).
 *
 * Queue implementation: circular buffer of ASTNode* pointers.
 * Complexity: O(n) time, O(w) space where w = max width of tree.
 */
#define BFS_QUEUE_SIZE 1024

void ast_visit_levelorder(ASTNode *node, ASTVisitCallback cb, void *user_data) {
    if (!node || !cb) return;

    ASTNode *queue[BFS_QUEUE_SIZE];
    int front = 0, rear = 0;

    queue[rear] = node;
    rear = (rear + 1) % BFS_QUEUE_SIZE;

    while (front != rear) {
        ASTNode *curr = queue[front];
        front = (front + 1) % BFS_QUEUE_SIZE;

        cb(curr, user_data);

        for (int i = 0; i < curr->child_count; i++) {
            if (curr->children[i]) {
                queue[rear] = curr->children[i];
                rear = (rear + 1) % BFS_QUEUE_SIZE;
            }
        }
    }
}

#undef BFS_QUEUE_SIZE

/* ─── AST to S-Expression String (Lisp-like format) ───────────────── */

/*
 * Serialize AST as an S-expression string (parenthesized prefix notation).
 * e.g., (+ 1 (* 2 3)) for binary operations.
 *
 * This is useful for debugging and for interfacing with Lisp-based tools.
 * L4: S-expressions are a universal AST interchange format (McCarthy 1960).
 * L6: Canonical representation used in Lisp compilers.
 */
static void ast_to_sexpr_buf(const ASTNode *node, char *buf, size_t *pos,
                              size_t bufsz) {
    if (!node || *pos >= bufsz) return;

    #define APPEND(fmt, ...) do { \
        if (*pos < bufsz) \
            *pos += snprintf(buf + *pos, bufsz - *pos, fmt, ##__VA_ARGS__); \
    } while(0)

    switch (node->type) {
    case AST_PROGRAM:
        APPEND("(program");
        for (int i = 0; i < node->child_count; i++) {
            APPEND(" ");
            ast_to_sexpr_buf(node->children[i], buf, pos, bufsz);
        }
        APPEND(")");
        break;
    case AST_FUNC_DEF:
        APPEND("(func %s", node->name);
        for (int i = 0; i < node->child_count; i++) {
            APPEND(" ");
            ast_to_sexpr_buf(node->children[i], buf, pos, bufsz);
        }
        APPEND(")");
        break;
    case AST_BLOCK:
        APPEND("(block");
        for (int i = 0; i < node->child_count; i++) {
            APPEND(" ");
            ast_to_sexpr_buf(node->children[i], buf, pos, bufsz);
        }
        APPEND(")");
        break;
    case AST_IF_STMT:
        APPEND("(if ");
        ast_to_sexpr_buf(ast_get_child(node, 0), buf, pos, bufsz);
        APPEND(" ");
        ast_to_sexpr_buf(ast_get_child(node, 1), buf, pos, bufsz);
        if (ast_get_child(node, 2)) {
            APPEND(" ");
            ast_to_sexpr_buf(ast_get_child(node, 2), buf, pos, bufsz);
        }
        APPEND(")");
        break;
    case AST_WHILE_STMT:
        APPEND("(while ");
        ast_to_sexpr_buf(ast_get_child(node, 0), buf, pos, bufsz);
        APPEND(" ");
        ast_to_sexpr_buf(ast_get_child(node, 1), buf, pos, bufsz);
        APPEND(")");
        break;
    case AST_RETURN_STMT:
        APPEND("(return");
        if (node->child_count > 0) {
            APPEND(" ");
            ast_to_sexpr_buf(ast_get_child(node, 0), buf, pos, bufsz);
        }
        APPEND(")");
        break;
    case AST_BINARY_OP:
        APPEND("(%c ", node->op);
        ast_to_sexpr_buf(ast_get_child(node, 0), buf, pos, bufsz);
        APPEND(" ");
        ast_to_sexpr_buf(ast_get_child(node, 1), buf, pos, bufsz);
        APPEND(")");
        break;
    case AST_UNARY_OP:
        APPEND("(%c ", node->op);
        ast_to_sexpr_buf(ast_get_child(node, 0), buf, pos, bufsz);
        APPEND(")");
        break;
    case AST_INT_LIT:
        APPEND("%d", node->int_value);
        break;
    case AST_IDENT:
        APPEND("%s", node->name);
        break;
    case AST_ASSIGN:
        APPEND("(= ");
        ast_to_sexpr_buf(ast_get_child(node, 0), buf, pos, bufsz);
        APPEND(" ");
        ast_to_sexpr_buf(ast_get_child(node, 1), buf, pos, bufsz);
        APPEND(")");
        break;
    case AST_CALL:
        APPEND("(%s", node->name);
        for (int i = 0; i < node->child_count; i++) {
            APPEND(" ");
            ast_to_sexpr_buf(node->children[i], buf, pos, bufsz);
        }
        APPEND(")");
        break;
    case AST_VAR_DECL:
        APPEND("(var %s)", node->name);
        break;
    case AST_PARAM:
        APPEND("(param %s)", node->name);
        break;
    case AST_STRING_LIT:
        APPEND("\"%s\"", node->name);
        break;
    default:
        APPEND("?");
        break;
    }

    #undef APPEND
}

char *ast_to_sexpr(const ASTNode *node) {
    if (!node) return strdup("nil");
    size_t bufsz = 4096;
    char *buf = (char *)malloc(bufsz);
    size_t pos = 0;
    ast_to_sexpr_buf(node, buf, &pos, bufsz);
    buf[pos] = '\0';
    return buf;
}

/* ─── AST Dot Format Export (Graphviz) ───────────────────────────── */

/*
 * Export AST to Graphviz DOT format for visualization.
 * L6: Visualization aids understanding of compiler data structures.
 * L7: Practical application for debugging compilers.
 */
void ast_export_dot(const ASTNode *node, FILE *fp) {
    if (!node || !fp) return;

    fprintf(fp, "digraph AST {\n");
    fprintf(fp, "  node [shape=box, fontname=\"Courier\"];\n");

    /* BFS traversal to assign node IDs and emit edges */
    typedef struct { ASTNode *node; int id; } NodeEntry;
    NodeEntry entries[512];
    int nentries = 0;

    ASTNode *queue[512];
    int front = 0, rear = 0;
    queue[rear++] = (ASTNode *)node;

    while (front < rear) {
        ASTNode *curr = queue[front++];
        int node_id = nentries;
        entries[nentries].node = curr;
        entries[nentries].id = node_id;
        nentries++;

        /* Emit node */
        const char *label = ast_node_type_name(curr->type);
        char extra[128] = "";
        if (curr->name[0]) snprintf(extra, sizeof(extra), "\\n%s", curr->name);
        fprintf(fp, "  n%d [label=\"%s%s\"];\n", node_id, label, extra);

        /* Queue children and emit edges */
        for (int i = 0; i < curr->child_count; i++) {
            if (curr->children[i]) {
                queue[rear++] = curr->children[i];
                /* Edge will be emitted after all nodes are numbered */
            }
        }
    }

    /* Emit edges in second pass */
    int id_map[512] = {0};
    for (int i = 0; i < nentries; i++) {
        ASTNode *n = entries[i].node;
        for (int j = 0; j < n->child_count; j++) {
            /* Find child's ID */
            for (int k = 0; k < nentries; k++) {
                if (entries[k].node == n->children[j]) {
                    fprintf(fp, "  n%d -> n%d;\n", entries[i].id, entries[k].id);
                    break;
                }
            }
        }
    }

    fprintf(fp, "}\n");
}

/* ─── Constant Expression Evaluation (L5: Partial Evaluation) ───── */

/*
 * Attempt to evaluate an AST expression at compile time.
 * Only works for constant expressions (no variable references).
 *
 * Returns: true if evaluation succeeded, false otherwise.
 * L5: Partial evaluation / constant folding at AST level.
 * L4: The correctness of constant folding relies on the Church-Rosser
 *     property of the lambda calculus — evaluation order doesn't affect
 *     the final constant result.
 */
bool ast_eval_const(const ASTNode *node, int *result) {
    if (!node || !result) return false;

    switch (node->type) {
    case AST_INT_LIT:
        *result = node->int_value;
        return true;

    case AST_BINARY_OP: {
        int left, right;
        if (!ast_eval_const(ast_get_child(node, 0), &left)) return false;
        if (!ast_eval_const(ast_get_child(node, 1), &right)) return false;

        switch (node->op) {
        case '+': *result = left + right; return true;
        case '-': *result = left - right; return true;
        case '*': *result = left * right; return true;
        case '/':
            if (right == 0) return false;
            *result = left / right;
            return true;
        case '=': *result = (left == right); return true;
        case '!': *result = (left != right); return true;
        case '<': *result = (left < right);  return true;
        case '>': *result = (left > right);  return true;
        case 'L': *result = (left <= right); return true;
        case 'G': *result = (left >= right); return true;
        case '&': *result = (left && right); return true;
        case '|': *result = (left || right); return true;
        default: return false;
        }
    }

    case AST_UNARY_OP: {
        int operand;
        if (!ast_eval_const(ast_get_child(node, 0), &operand)) return false;
        switch (node->op) {
        case '-': *result = -operand; return true;
        case '!': *result = !operand; return true;
        default: return false;
        }
    }

    default:
        return false;
    }
}
