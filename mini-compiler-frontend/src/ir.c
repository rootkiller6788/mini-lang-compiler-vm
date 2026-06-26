#include "ir.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Static string table for opcode names ─────────────────────────── */

static const char *ir_opcode_names[IR_COUNT] = {
    "LABEL", "BINARY", "UNARY", "COPY", "LOAD_IMM",
    "BR", "BR_COND", "BR_NOT_COND", "CALL", "RET",
    "PARAM", "ALLOCA", "STORE", "LOAD", "PHI", "NOP"
};

const char *ir_opcode_name(IROpcode op) {
    if (op >= 0 && op < IR_COUNT) return ir_opcode_names[op];
    return "UNKNOWN";
}

/* ─── IR Module Lifecycle ───────────────────────────────────────────── */

IRModule *ir_create(void) {
    IRModule *ir = (IRModule *)calloc(1, sizeof(IRModule));
    if (!ir) { fprintf(stderr, "ir error: out of memory\n"); exit(1); }
    ir->next_id = 1;
    ir->temp_counter = 0;
    ir->label_counter = 0;
    return ir;
}

void ir_destroy(IRModule *ir) {
    if (!ir) return;
    IRInstr *instr = ir->head;
    while (instr) {
        IRInstr *next = instr->next;
        free(instr);
        instr = next;
    }
    if (ir->cfg) cfg_destroy(ir->cfg);
    free(ir);
}

/* ─── Internal: append instruction to list ──────────────────────────── */

static IRInstr *ir_append(IRModule *ir, IROpcode opcode) {
    IRInstr *instr = (IRInstr *)calloc(1, sizeof(IRInstr));
    if (!instr) { fprintf(stderr, "ir error: out of memory\n"); exit(1); }
    instr->opcode = opcode;
    instr->id = ir->next_id++;
    instr->dest[0] = '\0';
    instr->src1[0] = '\0';
    instr->src2[0] = '\0';
    instr->imm = 0;
    instr->op = '\0';
    instr->dead = false;

    if (ir->tail) {
        ir->tail->next = instr;
        instr->prev = ir->tail;
        ir->tail = instr;
    } else {
        ir->head = ir->tail = instr;
    }
    return instr;
}

/* ─── Temp and Label generation ─────────────────────────────────────── */

char *ir_new_temp(IRModule *ir, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%%t%d", ir->temp_counter++);
    return buf;
}

char *ir_new_label(IRModule *ir, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "L%d", ir->label_counter++);
    return buf;
}

/* ─── Instruction Emitters ──────────────────────────────────────────── */

IRInstr *ir_emit_label(IRModule *ir, const char *name) {
    IRInstr *instr = ir_append(ir, IR_LABEL);
    snprintf(instr->src1, sizeof(instr->src1), "%s", name);
    return instr;
}

IRInstr *ir_emit_binary(IRModule *ir, const char *dst, const char *src1,
                         char op, const char *src2) {
    IRInstr *instr = ir_append(ir, IR_BINARY);
    snprintf(instr->dest, sizeof(instr->dest), "%s", dst ? dst : "");
    snprintf(instr->src1, sizeof(instr->src1), "%s", src1 ? src1 : "");
    snprintf(instr->src2, sizeof(instr->src2), "%s", src2 ? src2 : "");
    instr->op = op;
    return instr;
}

IRInstr *ir_emit_unary(IRModule *ir, const char *dst, char op, const char *src) {
    IRInstr *instr = ir_append(ir, IR_UNARY);
    snprintf(instr->dest, sizeof(instr->dest), "%s", dst ? dst : "");
    snprintf(instr->src1, sizeof(instr->src1), "%s", src ? src : "");
    instr->op = op;
    return instr;
}

IRInstr *ir_emit_copy(IRModule *ir, const char *dst, const char *src) {
    IRInstr *instr = ir_append(ir, IR_COPY);
    snprintf(instr->dest, sizeof(instr->dest), "%s", dst ? dst : "");
    snprintf(instr->src1, sizeof(instr->src1), "%s", src ? src : "");
    return instr;
}

IRInstr *ir_emit_load_imm(IRModule *ir, const char *dst, int imm) {
    IRInstr *instr = ir_append(ir, IR_LOAD_IMM);
    snprintf(instr->dest, sizeof(instr->dest), "%s", dst ? dst : "");
    instr->imm = imm;
    return instr;
}

IRInstr *ir_emit_br(IRModule *ir, const char *label) {
    IRInstr *instr = ir_append(ir, IR_BR);
    snprintf(instr->src1, sizeof(instr->src1), "%s", label ? label : "");
    return instr;
}

IRInstr *ir_emit_br_cond(IRModule *ir, const char *cond, const char *label) {
    IRInstr *instr = ir_append(ir, IR_BR_COND);
    snprintf(instr->src1, sizeof(instr->src1), "%s", cond ? cond : "");
    snprintf(instr->src2, sizeof(instr->src2), "%s", label ? label : "");
    return instr;
}

IRInstr *ir_emit_br_not_cond(IRModule *ir, const char *cond, const char *label) {
    IRInstr *instr = ir_append(ir, IR_BR_NOT_COND);
    snprintf(instr->src1, sizeof(instr->src1), "%s", cond ? cond : "");
    snprintf(instr->src2, sizeof(instr->src2), "%s", label ? label : "");
    return instr;
}

IRInstr *ir_emit_call(IRModule *ir, const char *dst, const char *func, int nargs) {
    IRInstr *instr = ir_append(ir, IR_CALL);
    snprintf(instr->dest, sizeof(instr->dest), "%s", dst ? dst : "");
    snprintf(instr->src1, sizeof(instr->src1), "%s", func ? func : "");
    instr->imm = nargs;
    return instr;
}

IRInstr *ir_emit_ret(IRModule *ir, const char *val) {
    IRInstr *instr = ir_append(ir, IR_RET);
    if (val) snprintf(instr->src1, sizeof(instr->src1), "%s", val);
    return instr;
}

IRInstr *ir_emit_param(IRModule *ir, const char *val) {
    IRInstr *instr = ir_append(ir, IR_PARAM);
    snprintf(instr->src1, sizeof(instr->src1), "%s", val ? val : "");
    return instr;
}

IRInstr *ir_emit_alloca(IRModule *ir, const char *dst, int size) {
    IRInstr *instr = ir_append(ir, IR_ALLOCA);
    snprintf(instr->dest, sizeof(instr->dest), "%s", dst ? dst : "");
    instr->imm = size;
    return instr;
}

IRInstr *ir_emit_store(IRModule *ir, const char *addr, const char *val) {
    IRInstr *instr = ir_append(ir, IR_STORE);
    snprintf(instr->src1, sizeof(instr->src1), "%s", addr ? addr : "");
    snprintf(instr->src2, sizeof(instr->src2), "%s", val ? val : "");
    return instr;
}

IRInstr *ir_emit_load(IRModule *ir, const char *dst, const char *addr) {
    IRInstr *instr = ir_append(ir, IR_LOAD);
    snprintf(instr->dest, sizeof(instr->dest), "%s", dst ? dst : "");
    snprintf(instr->src1, sizeof(instr->src1), "%s", addr ? addr : "");
    return instr;
}

IRInstr *ir_emit_phi(IRModule *ir, const char *dst) {
    IRInstr *instr = ir_append(ir, IR_PHI);
    snprintf(instr->dest, sizeof(instr->dest), "%s", dst ? dst : "");
    return instr;
}

IRInstr *ir_emit_nop(IRModule *ir) {
    return ir_append(ir, IR_NOP);
}

/* ─── IR Printing ────────────────────────────────────────────────────── */

void ir_print_instr(const IRInstr *instr) {
    if (!instr) return;
    printf("  %4d: ", instr->id);
    switch (instr->opcode) {
    case IR_LABEL:
        printf("%s:", instr->src1);
        break;
    case IR_BINARY:
        printf("%-12s = %s %c %s", instr->dest, instr->src1, instr->op, instr->src2);
        break;
    case IR_UNARY:
        printf("%-12s = %c %s", instr->dest, instr->op, instr->src1);
        break;
    case IR_COPY:
        printf("%-12s = %s", instr->dest, instr->src1);
        break;
    case IR_LOAD_IMM:
        printf("%-12s = %d", instr->dest, instr->imm);
        break;
    case IR_BR:
        printf("goto %s", instr->src1);
        break;
    case IR_BR_COND:
        printf("if %s goto %s", instr->src1, instr->src2);
        break;
    case IR_BR_NOT_COND:
        printf("if !%s goto %s", instr->src1, instr->src2);
        break;
    case IR_CALL:
        printf("%-12s = call %s(%d args)", instr->dest, instr->src1, instr->imm);
        break;
    case IR_RET:
        printf("return %s", instr->src1[0] ? instr->src1 : "");
        break;
    case IR_PARAM:
        printf("param %s", instr->src1);
        break;
    case IR_ALLOCA:
        printf("%-12s = alloca %d", instr->dest, instr->imm);
        break;
    case IR_STORE:
        printf("*%s = %s", instr->src1, instr->src2);
        break;
    case IR_LOAD:
        printf("%-12s = *%s", instr->dest, instr->src1);
        break;
    case IR_PHI:
        printf("%-12s = phi(%s : %s)", instr->dest, instr->src1, instr->src2);
        break;
    case IR_NOP:
        printf("nop");
        break;
    default:
        printf("???");
        break;
    }
    if (instr->dead) printf("  [dead]");
    printf("\n");
}

void ir_print(const IRModule *ir) {
    if (!ir) return;
    printf("=== IR (Three-Address Code) ===\n");
    for (IRInstr *instr = ir->head; instr; instr = instr->next) {
        ir_print_instr(instr);
    }
}

/* ─── AST → IR Translation (L5: Algorithm) ─────────────────────────── */

/*
 * Translation strategy:
 * Each AST expression is compiled into a sequence of IR instructions that
 * compute the value into a new temporary. The temporary name is returned
 * via the `result` buffer.
 *
 * This is a classic syntax-directed translation: each AST node type has a
 * corresponding IR generation rule.
 *
 * Reference:
 *   - Aho et al. "Compilers" Ch.6 "Intermediate-Code Generation"
 *   - Appel "Modern Compiler Implementation in C" Ch.7
 */

static void gen_expr(IRModule *ir, const ASTNode *node, char *result, size_t rsz);
static void gen_stmt(IRModule *ir, const ASTNode *node);

/* Variable naming: user variables get prefix "#" to distinguish from IR temps */
static void var_name(const char *src, char *buf, size_t sz) {
    snprintf(buf, sz, "#%s", src);
}

static void gen_expr(IRModule *ir, const ASTNode *node, char *result, size_t rsz) {
    if (!node) { result[0] = '\0'; return; }

    char t1[64], t2[64];

    switch (node->type) {
    case AST_INT_LIT: {
        char *tmp = ir_new_temp(ir, t1, sizeof(t1));
        ir_emit_load_imm(ir, tmp, node->int_value);
        snprintf(result, rsz, "%s", tmp);
        break;
    }
    case AST_STRING_LIT: {
        /* String literals: treat as immediate with value 0 (address placeholder) */
        char *tmp = ir_new_temp(ir, t1, sizeof(t1));
        ir_emit_load_imm(ir, tmp, 0);
        snprintf(result, rsz, "%s", tmp);
        break;
    }
    case AST_IDENT: {
        snprintf(result, rsz, "#%s", node->name);
        break;
    }
    case AST_UNARY_OP: {
        gen_expr(ir, ast_get_child(node, 0), t1, sizeof(t1));
        char *tmp = ir_new_temp(ir, t2, sizeof(t2));
        ir_emit_unary(ir, tmp, node->op, t1);
        snprintf(result, rsz, "%s", tmp);
        break;
    }
    case AST_BINARY_OP: {
        gen_expr(ir, ast_get_child(node, 0), t1, sizeof(t1));
        gen_expr(ir, ast_get_child(node, 1), t2, sizeof(t2));
        char *tmp = ir_new_temp(ir, result, rsz);
        ir_emit_binary(ir, tmp, t1, node->op, t2);
        break;
    }
    case AST_ASSIGN: {
        /* Assignment: evaluate RHS, then store to LHS variable */
        ASTNode *lhs = ast_get_child(node, 0);
        ASTNode *rhs = ast_get_child(node, 1);
        gen_expr(ir, rhs, t1, sizeof(t1));
        if (lhs && lhs->type == AST_IDENT) {
            char vname[64];
            var_name(lhs->name, vname, sizeof(vname));
            ir_emit_copy(ir, vname, t1);
        }
        snprintf(result, rsz, "%s", t1);
        break;
    }
    case AST_CALL: {
        /* Generate PARAM instructions for each argument, then CALL */
        for (int i = 0; i < node->child_count; i++) {
            gen_expr(ir, node->children[i], t1, sizeof(t1));
            ir_emit_param(ir, t1);
        }
        char *tmp = ir_new_temp(ir, t1, sizeof(t1));
        ir_emit_call(ir, tmp, node->name, node->child_count);
        snprintf(result, rsz, "%s", tmp);
        break;
    }
    default:
        result[0] = '\0';
        break;
    }
}

static void gen_stmt(IRModule *ir, const ASTNode *node) {
    if (!node) return;

    char t1[64], t2[64], lbl[64], lbl_end[64], lbl_else[64];

    switch (node->type) {
    case AST_BLOCK:
        for (int i = 0; i < node->child_count; i++) {
            gen_stmt(ir, node->children[i]);
        }
        break;

    case AST_IF_STMT: {
        /* if (cond) then [else end]
         * IR:
         *   t = cond
         *   if !t goto L_else
         *   <then>
         *   goto L_end
         * L_else:
         *   <else>
         * L_end:
         */
        ASTNode *cond   = ast_get_child(node, 0);
        ASTNode *then_b = ast_get_child(node, 1);
        ASTNode *else_b = ast_get_child(node, 2);

        ir_new_label(ir, lbl_else, sizeof(lbl_else));
        ir_new_label(ir, lbl_end, sizeof(lbl_end));

        gen_expr(ir, cond, t1, sizeof(t1));
        ir_emit_br_not_cond(ir, t1, lbl_else);

        gen_stmt(ir, then_b);
        ir_emit_br(ir, lbl_end);

        ir_emit_label(ir, lbl_else);
        if (else_b) {
            gen_stmt(ir, ast_get_child(else_b, 0));
        }
        ir_emit_label(ir, lbl_end);
        break;
    }

    case AST_WHILE_STMT: {
        /* while (cond) body
         * IR:
         * L_loop:
         *   t = cond
         *   if !t goto L_end
         *   <body>
         *   goto L_loop
         * L_end:
         */
        ASTNode *cond = ast_get_child(node, 0);
        ASTNode *body = ast_get_child(node, 1);

        ir_new_label(ir, lbl, sizeof(lbl));
        ir_new_label(ir, lbl_end, sizeof(lbl_end));

        ir_emit_label(ir, lbl);
        gen_expr(ir, cond, t1, sizeof(t1));
        ir_emit_br_not_cond(ir, t1, lbl_end);
        gen_stmt(ir, body);
        ir_emit_br(ir, lbl);
        ir_emit_label(ir, lbl_end);
        break;
    }

    case AST_RETURN_STMT: {
        ASTNode *ret_expr = ast_get_child(node, 0);
        if (ret_expr) {
            gen_expr(ir, ret_expr, t1, sizeof(t1));
            ir_emit_ret(ir, t1);
        } else {
            ir_emit_ret(ir, "");
        }
        break;
    }

    case AST_VAR_DECL: {
        /* Allocate stack space for variable */
        char vname[64];
        var_name(node->name, vname, sizeof(vname));
        ir_emit_alloca(ir, vname, 4);  /* 4 bytes for int */
        break;
    }

    case AST_ASSIGN:
    case AST_CALL:
    case AST_BINARY_OP:
    case AST_UNARY_OP:
        /* Expression statement: compute and discard */
        gen_expr(ir, node, t1, sizeof(t1));
        break;

    default:
        break;
    }
}

/* Translate one function AST node into an IR module */
static IRModule *ir_from_function(ASTNode *func_def) {
    IRModule *ir = ir_create();

    /* Emit function label */
    ir_emit_label(ir, func_def->name);

    /* Params become ALLOCA'd variables */
    for (int i = 0; i < func_def->child_count; i++) {
        ASTNode *child = func_def->children[i];
        if (child->type == AST_PARAM) {
            char vname[64];
            var_name(child->name, vname, sizeof(vname));
            ir_emit_alloca(ir, vname, 4);
            /* Parameter value is stored into the alloca'd slot */
            ir_emit_copy(ir, vname, vname);  /* simplified: params already in slots */
        }
    }

    /* Generate body */
    for (int i = 0; i < func_def->child_count; i++) {
        if (func_def->children[i]->type == AST_BLOCK) {
            gen_stmt(ir, func_def->children[i]);
        }
    }

    /* Ensure terminator */
    if (!ir->tail ||
        (ir->tail->opcode != IR_RET && ir->tail->opcode != IR_BR)) {
        ir_emit_ret(ir, "0");
    }

    return ir;
}

IRProgram *ir_program_from_ast(ASTNode *program) {
    if (!program || program->type != AST_PROGRAM) return NULL;

    /* Count functions */
    int nfuncs = 0;
    for (int i = 0; i < program->child_count; i++) {
        if (program->children[i]->type == AST_FUNC_DEF) nfuncs++;
    }

    IRProgram *prog = (IRProgram *)calloc(1, sizeof(IRProgram));
    if (!prog) return NULL;

    prog->functions  = (IRModule **)calloc(nfuncs + 1, sizeof(IRModule *));
    prog->func_names = (char **)calloc(nfuncs + 1, sizeof(char *));
    if (!prog->functions || !prog->func_names) { free(prog); return NULL; }

    int idx = 0;
    for (int i = 0; i < program->child_count; i++) {
        if (program->children[i]->type == AST_FUNC_DEF) {
            prog->func_names[idx] = strdup(program->children[i]->name);
            prog->functions[idx]  = ir_from_function(program->children[i]);
            idx++;
        }
    }
    prog->count = idx;
    return prog;
}

void ir_program_destroy(IRProgram *prog) {
    if (!prog) return;
    for (int i = 0; i < prog->count; i++) {
        ir_destroy(prog->functions[i]);
        free(prog->func_names[i]);
    }
    free(prog->functions);
    free(prog->func_names);
    free(prog);
}

/* ─── Constant Folding (L5: Algorithm) ────────────────────────────── */

/*
 * Constant Folding: evaluate constant expressions at compile time.
 *
 * For each binary operation where both operands are LOAD_IMM constants,
 * compute the result and replace the operation with a LOAD_IMM.
 *
 * This is a simplified version of sparse conditional constant propagation
 * (SCCP, Wegman & Zadeck 1991).  Full SCCP would track three lattice
 * values (TOP, CONSTANT, BOTTOM) per variable; here we only fold when
 * both operands are immediate constants.
 *
 * Complexity: O(n) where n = number of instructions.
 */
int ir_constant_folding(IRModule *ir) {
    int folded = 0;
    if (!ir) return 0;

    for (IRInstr *instr = ir->head; instr; instr = instr->next) {
        if (instr->opcode != IR_BINARY) continue;
        if (instr->dead) continue;

        /* Find the defining instructions for src1 and src2 */
        IRInstr *def1 = NULL, *def2 = NULL;
        for (IRInstr *d = ir->head; d && d != instr; d = d->next) {
            if (d->opcode == IR_LOAD_IMM && strcmp(d->dest, instr->src1) == 0
                && !d->dead) def1 = d;
            if (d->opcode == IR_LOAD_IMM && strcmp(d->dest, instr->src2) == 0
                && !d->dead) def2 = d;
        }

        if (!def1 || !def2) continue;

        int v1 = def1->imm;
        int v2 = def2->imm;
        int result;

        switch (instr->op) {
        case '+': result = v1 + v2; break;
        case '-': result = v1 - v2; break;
        case '*': result = v1 * v2; break;
        case '/':
            if (v2 == 0) continue;  /* division by zero: don't fold */
            result = v1 / v2;
            break;
        case '=': case '!':  /* equality comparison */
            result = (instr->op == '=') ? (v1 == v2) : (v1 != v2);
            break;
        case '<': result = (v1 < v2);  break;
        case '>': result = (v1 > v2);  break;
        case 'L': result = (v1 <= v2); break;  /* <= encoded as 'L' */
        case 'G': result = (v1 >= v2); break;  /* >= encoded as 'G' */
        case '&': result = (v1 && v2); break;
        case '|': result = (v1 || v2); break;
        default: continue;
        }

        /* Convert binary operation into LOAD_IMM */
        instr->opcode = IR_LOAD_IMM;
        instr->imm = result;
        instr->op = '\0';
        instr->src1[0] = '\0';
        instr->src2[0] = '\0';
        folded++;
    }

    return folded;
}

/* ─── Dead Code Elimination (L5: Algorithm) ────────────────────────── */

/*
 * Dead Code Elimination: mark-sweep algorithm.
 *
 * 1. MARK phase: start from "critical" instructions (those with side effects:
 *    STORE, CALL, RET, BR, BR_COND, BR_NOT_COND, PARAM) and recursively mark
 *    all instructions that define operands used by marked instructions.
 *
 * 2. SWEEP phase: remove (mark dead=true) all unmarked instructions.
 *
 * This is a conservative DCE: we keep all side-effecting instructions
 * and anything they depend on.
 *
 * Complexity: O(n^2) in current implementation (linear search for defs);
 * can be improved to O(n) with a use-def chain precomputation.
 */
int ir_dead_code_elimination(IRModule *ir) {
    if (!ir) return 0;

    /* First pass: mark all as dead, then revive critical and their deps */
    for (IRInstr *instr = ir->head; instr; instr = instr->next) {
        instr->dead = true;  /* start pessimistic */
    }

    /* Stack for worklist-based marking */
    IRInstr **worklist = NULL;
    int wl_size = 0, wl_cap = 0;

    /* Mark critical instructions and push to worklist */
    for (IRInstr *instr = ir->head; instr; instr = instr->next) {
        bool critical = false;
        switch (instr->opcode) {
        case IR_STORE: case IR_CALL: case IR_RET:
        case IR_BR: case IR_BR_COND: case IR_BR_NOT_COND:
        case IR_PARAM: case IR_LABEL:
            critical = true;
            break;
        default:
            break;
        }
        if (critical && instr->dead) {
            instr->dead = false;
            if (wl_size >= wl_cap) {
                wl_cap = wl_cap ? wl_cap * 2 : 64;
                worklist = (IRInstr **)realloc(worklist, wl_cap * sizeof(IRInstr *));
            }
            worklist[wl_size++] = instr;
        }
    }

    /* Process worklist: for each instruction, mark defs of its operands */
    while (wl_size > 0) {
        IRInstr *curr = worklist[--wl_size];

        /* For each source operand, find its defining instruction and mark it */
        const char *operands[2] = { curr->src1, curr->src2 };
        for (int oi = 0; oi < 2; oi++) {
            const char *opnd = operands[oi];
            if (!opnd || opnd[0] == '\0') continue;
            if (opnd[0] == '%' || opnd[0] == '#') {
                /* Search backward for the definition */
                for (IRInstr *def = ir->head; def && def != curr; def = def->next) {
                    if (def->dead && def->dest[0] && strcmp(def->dest, opnd) == 0) {
                        def->dead = false;
                        if (wl_size >= wl_cap) {
                            wl_cap *= 2;
                            worklist = (IRInstr **)realloc(worklist, wl_cap * sizeof(IRInstr *));
                        }
                        worklist[wl_size++] = def;
                        break;  /* only need the reaching definition */
                    }
                }
            }
        }
    }

    free(worklist);

    /* Count eliminated */
    int eliminated = 0;
    for (IRInstr *instr = ir->head; instr; instr = instr->next) {
        if (instr->dead) eliminated++;
    }

    return eliminated;
}

/* ─── Copy Propagation (L5: Algorithm) ──────────────────────────────── */

/*
 * Copy Propagation: for each COPY instruction  t = s,
 * replace subsequent uses of t with s (until t is redefined).
 *
 * Algorithm:
 *   For each instruction i:
 *     If i is  t = s (COPY):
 *       For each subsequent instruction j:
 *         Replace uses of t in j with s
 *         Stop if t is redefined
 *
 * Complexity: O(n^2) in this implementation.
 * Returns: number of uses replaced.
 */
int ir_copy_propagation(IRModule *ir) {
    if (!ir) return 0;
    int propagated = 0;

    for (IRInstr *instr = ir->head; instr; instr = instr->next) {
        if (instr->opcode != IR_COPY || instr->dead) continue;
        if (instr->dest[0] == '\0' || instr->src1[0] == '\0') continue;

        const char *copy_dst = instr->dest;
        const char *copy_src = instr->src1;

        /* Replace uses in subsequent instructions until redefinition */
        for (IRInstr *j = instr->next; j; j = j->next) {
            if (j->dead) continue;

            /* Stop if copy_dst is redefined */
            if (j->dest[0] && strcmp(j->dest, copy_dst) == 0
                && j->opcode != IR_COPY) {
                break;
            }

            /* Replace uses */
            if (j->src1[0] && strcmp(j->src1, copy_dst) == 0) {
                snprintf(j->src1, sizeof(j->src1), "%s", copy_src);
                propagated++;
            }
            if (j->src2[0] && strcmp(j->src2, copy_dst) == 0) {
                snprintf(j->src2, sizeof(j->src2), "%s", copy_src);
                propagated++;
            }
        }
    }

    return propagated;
}

/* ─── Optimization Driver ───────────────────────────────────────────── */

void ir_optimize(IRModule *ir) {
    if (!ir) return;
    /* Run passes iteratively until convergence (max 10 rounds) */
    for (int round = 0; round < 10; round++) {
        int changes = 0;
        changes += ir_constant_folding(ir);
        changes += ir_copy_propagation(ir);
        changes += ir_dead_code_elimination(ir);
        if (changes == 0) break;
    }
}

/* ─── CFG Construction (L5: Algorithm) ────────────────────────────── */

/*
 * Build Control Flow Graph from flat IR:
 *
 * 1. Identify leaders:
 *    a. First instruction
 *    b. Any instruction that is the target of a branch (label)
 *    c. Any instruction immediately following a branch/return
 *
 * 2. Create basic blocks: each leader starts a new block extending to
 *    the next leader.
 *
 * 3. Connect edges: for each block's terminator, add successor edges.
 *
 * Reference:
 *   - Aho et al. "Compilers" Algorithm 9.5 (p.531)
 */

static BasicBlock *bb_create(int id, const char *label) {
    BasicBlock *bb = (BasicBlock *)calloc(1, sizeof(BasicBlock));
    bb->id = id;
    snprintf(bb->label, sizeof(bb->label), "%s", label ? label : "");
    bb->pred_cap = 4;
    bb->preds = (BasicBlock **)calloc(bb->pred_cap, sizeof(BasicBlock *));
    bb->succ_cap = 4;
    bb->succs = (BasicBlock **)calloc(bb->succ_cap, sizeof(BasicBlock *));
    return bb;
}

static void bb_add_pred(BasicBlock *bb, BasicBlock *pred) {
    for (int i = 0; i < bb->pred_count; i++) {
        if (bb->preds[i] == pred) return;
    }
    if (bb->pred_count >= bb->pred_cap) {
        bb->pred_cap *= 2;
        bb->preds = (BasicBlock **)realloc(bb->preds, bb->pred_cap * sizeof(BasicBlock *));
    }
    bb->preds[bb->pred_count++] = pred;
}

static void bb_add_succ(BasicBlock *bb, BasicBlock *succ) {
    for (int i = 0; i < bb->succ_count; i++) {
        if (bb->succs[i] == succ) return;
    }
    if (bb->succ_count >= bb->succ_cap) {
        bb->succ_cap *= 2;
        bb->succs = (BasicBlock **)realloc(bb->succs, bb->succ_cap * sizeof(BasicBlock *));
    }
    bb->succs[bb->succ_count++] = succ;
}

CFG *cfg_build(IRModule *ir) {
    if (!ir || !ir->head) return NULL;

    /* Step 1: mark leaders */
    int ninstrs = 0;
    for (IRInstr *p = ir->head; p; p = p->next) ninstrs++;
    if (ninstrs == 0) return NULL;

    bool *is_leader = (bool *)calloc(ninstrs, sizeof(bool));

    /* Map instruction to its index */
    IRInstr **idx_to_instr = (IRInstr **)calloc(ninstrs, sizeof(IRInstr *));
    {
        int i = 0;
        for (IRInstr *p = ir->head; p; p = p->next) idx_to_instr[i++] = p;
    }

    /* First instruction is a leader */
    is_leader[0] = true;

    /* Label targets and instructions after branches are leaders */
    for (int i = 0; i < ninstrs; i++) {
        IRInstr *p = idx_to_instr[i];
        if (p->opcode == IR_LABEL) is_leader[i] = true;
        if (p->opcode == IR_BR || p->opcode == IR_BR_COND ||
            p->opcode == IR_BR_NOT_COND || p->opcode == IR_RET) {
            if (i + 1 < ninstrs) is_leader[i + 1] = true;

            /* Branch target is a leader */
            const char *target = (p->opcode == IR_BR) ? p->src1 : p->src2;
            if (target[0]) {
                for (int j = 0; j < ninstrs; j++) {
                    if (idx_to_instr[j]->opcode == IR_LABEL &&
                        strcmp(idx_to_instr[j]->src1, target) == 0) {
                        is_leader[j] = true;
                    }
                }
            }
        }
    }

    /* Step 2: partition into blocks */
    int nblocks = 0;
    for (int i = 0; i < ninstrs; i++) if (is_leader[i]) nblocks++;

    BasicBlock **blocks = (BasicBlock **)calloc(nblocks, sizeof(BasicBlock *));
    int *leader_indices = (int *)calloc(nblocks, sizeof(int));

    {
        int bi = 0;
        for (int i = 0; i < ninstrs; i++) {
            if (is_leader[i]) leader_indices[bi++] = i;
        }
    }

    for (int bi = 0; bi < nblocks; bi++) {
        int start_idx = leader_indices[bi];
        int end_idx = (bi + 1 < nblocks) ? leader_indices[bi + 1] - 1 : ninstrs - 1;

        const char *lbl = "";
        if (idx_to_instr[start_idx]->opcode == IR_LABEL)
            lbl = idx_to_instr[start_idx]->src1;

        blocks[bi] = bb_create(bi, lbl);
        blocks[bi]->first = idx_to_instr[start_idx];
        blocks[bi]->last  = idx_to_instr[end_idx];
    }

    /* Step 3: connect edges */
    /* Build label → block map */
    for (int bi = 0; bi < nblocks; bi++) {
        IRInstr *last = blocks[bi]->last;

        /* Fall-through to next block (unless unconditional branch or return) */
        bool is_terminator = (last->opcode == IR_BR || last->opcode == IR_RET);

        if (!is_terminator && bi + 1 < nblocks) {
            bb_add_succ(blocks[bi], blocks[bi + 1]);
            bb_add_pred(blocks[bi + 1], blocks[bi]);
        }

        /* Branch successors */
        if (last->opcode == IR_BR) {
            /* Unconditional branch */
            for (int bj = 0; bj < nblocks; bj++) {
                if (blocks[bj]->label[0] &&
                    strcmp(blocks[bj]->label, last->src1) == 0) {
                    bb_add_succ(blocks[bi], blocks[bj]);
                    bb_add_pred(blocks[bj], blocks[bi]);
                    break;
                }
            }
        } else if (last->opcode == IR_BR_COND || last->opcode == IR_BR_NOT_COND) {
            /* Conditional branch: two successors (fall-through and target) */
            /* Fall-through already added above */
            for (int bj = 0; bj < nblocks; bj++) {
                if (blocks[bj]->label[0] &&
                    strcmp(blocks[bj]->label, last->src2) == 0) {
                    bb_add_succ(blocks[bi], blocks[bj]);
                    bb_add_pred(blocks[bj], blocks[bi]);
                    break;
                }
            }
        }
    }

    CFG *cfg = (CFG *)calloc(1, sizeof(CFG));
    cfg->blocks = blocks;
    cfg->block_count = nblocks;
    cfg->block_cap = nblocks;
    cfg->entry = (nblocks > 0) ? blocks[0] : NULL;

    free(is_leader);
    free(idx_to_instr);
    free(leader_indices);

    ir->cfg = cfg;
    return cfg;
}

void cfg_destroy(CFG *cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->block_count; i++) {
        free(cfg->blocks[i]->preds);
        free(cfg->blocks[i]->succs);
        free(cfg->blocks[i]);
    }
    free(cfg->blocks);
    free(cfg);
}

void cfg_print(const CFG *cfg) {
    if (!cfg) return;
    printf("=== Control Flow Graph (%d blocks) ===\n", cfg->block_count);
    for (int i = 0; i < cfg->block_count; i++) {
        BasicBlock *bb = cfg->blocks[i];
        printf("Block %d", bb->id);
        if (bb->label[0]) printf(" [%s]", bb->label);
        printf("  preds: [");
        for (int j = 0; j < bb->pred_count; j++)
            printf("%s%d", j ? ", " : "", bb->preds[j]->id);
        printf("]  succs: [");
        for (int j = 0; j < bb->succ_count; j++)
            printf("%s%d", j ? ", " : "", bb->succs[j]->id);
        printf("]\n");
    }
}

/* ─── Dominator Computation (L5: Algorithm) ────────────────────────── */

/*
 * Immediate dominators via Cooper-Harvey-Kennedy iterative algorithm.
 *
 * A node d dominates node n if every path from the entry to n passes through d.
 * The immediate dominator idom(n) is the unique node that strictly dominates n
 * but does not strictly dominate any other strict dominator of n.
 *
 * Algorithm:
 *   dom[entry] = entry
 *   for all other n: dom[n] = undefined
 *   repeat until no change:
 *     for each n != entry:
 *       new_idom = first predecessor of n with defined dom
 *       for each other predecessor p of n:
 *         if dom[p] is defined:
 *           new_idom = intersect(p, new_idom)
 *       dom[n] = new_idom
 *
 * intersect(b1, b2): walk up dominator tree from b1 and b2 until they meet.
 *
 * Complexity: O(N * D) typical, where D is the loop-connectedness.
 *
 * Reference:
 *   - Cooper, Harvey, Kennedy "A Simple, Fast Dominance Algorithm" (2001)
 */

/* Helper: find intersection of two nodes in the dominator tree */
static int intersect_dom(const int *doms, int b1, int b2, int nblocks) {
    /* fingerprint array: track visited nodes */
    bool *visited = (bool *)calloc(nblocks, sizeof(bool));
    while (b1 >= 0 && b1 < nblocks) {
        visited[b1] = true;
        b1 = doms[b1];
        if (b1 == b2) { free(visited); return b2; }
    }
    while (b2 >= 0 && b2 < nblocks) {
        if (visited[b2]) { free(visited); return b2; }
        b2 = doms[b2];
    }
    free(visited);
    return -1;
}

int *cfg_compute_dominators(const CFG *cfg) {
    if (!cfg || cfg->block_count == 0) return NULL;

    int n = cfg->block_count;
    int *doms = (int *)malloc(n * sizeof(int));

    /* Initialize: entry dominates itself, others undefined (-2) */
    for (int i = 0; i < n; i++) doms[i] = -2;
    doms[0] = 0;  /* entry block idom = itself */

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 1; i < n; i++) {  /* skip entry */
            BasicBlock *bb = cfg->blocks[i];
            if (bb->pred_count == 0) continue;

            /* Find first defined predecessor as starting point */
            int new_idom = -1;
            for (int j = 0; j < bb->pred_count; j++) {
                int p = bb->preds[j]->id;
                if (doms[p] >= 0) { new_idom = p; break; }
            }
            if (new_idom < 0) continue;

            /* Intersect with remaining predecessors */
            for (int j = 0; j < bb->pred_count; j++) {
                int p = bb->preds[j]->id;
                if (p != new_idom && doms[p] >= 0) {
                    new_idom = intersect_dom(doms, new_idom, p, n);
                    if (new_idom < 0) break;
                }
            }

            if (new_idom >= 0 && doms[i] != new_idom) {
                doms[i] = new_idom;
                changed = true;
            }
        }
    }

    /* Set idom for each block */
    for (int i = 0; i < n; i++) {
        if (doms[i] >= 0 && doms[i] < n)
            cfg->blocks[i]->idom = cfg->blocks[doms[i]];
        else
            cfg->blocks[i]->idom = NULL;
    }

    return doms;
}

/* ─── Dominance Frontier (L8: Advanced) ────────────────────────────── */

/*
 * Dominance Frontier DF(b): set of nodes n such that b dominates a
 * predecessor of n but does not strictly dominate n.
 *
 * DF is fundamental to SSA construction: φ-nodes must be placed at the
 * dominance frontier of definitions.
 *
 * Algorithm (Cytron et al. 1991):
 *   For each node b:
 *     If b has multiple predecessors:
 *       For each predecessor p of b:
 *         runner = p
 *         while runner != idom(b):
 *           add b to DF(runner)
 *           runner = idom(runner)
 */

void dom_frontier_init(DomFrontier *df, int nblocks) {
    df->block_count = nblocks;
    df->frontier_starts = (int *)calloc(nblocks, sizeof(int));
    df->frontier_sizes  = (int *)calloc(nblocks, sizeof(int));
    /* Pre-allocate worst-case frontier storage (nblocks * nblocks) */
    df->frontier_sets = (int *)malloc(nblocks * nblocks * sizeof(int));
    for (int i = 0; i < nblocks * nblocks; i++) df->frontier_sets[i] = -1;
}

void dom_frontier_destroy(DomFrontier *df) {
    free(df->frontier_sets);
    free(df->frontier_starts);
    free(df->frontier_sizes);
}

static void df_add(DomFrontier *df, int block, int frontier_block) {
    int start = df->frontier_starts[block];
    int size  = df->frontier_sizes[block];
    /* Check for duplicate */
    for (int i = 0; i < size; i++) {
        if (df->frontier_sets[start + i] == frontier_block) return;
    }
    if (start + size < df->block_count * df->block_count) {
        df->frontier_sets[start + size] = frontier_block;
        df->frontier_sizes[block]++;
    }
}

void dom_frontier_compute(DomFrontier *df, const CFG *cfg, const int *idom) {
    int n = cfg->block_count;

    /* Compute offsets */
    int offset = 0;
    for (int i = 0; i < n; i++) {
        df->frontier_starts[i] = offset;
        offset += n;  /* worst-case: n frontier entries per block */
    }

    for (int b = 0; b < n; b++) {
        BasicBlock *bb = cfg->blocks[b];
        if (bb->pred_count < 2) continue;

        for (int j = 0; j < bb->pred_count; j++) {
            int runner = bb->preds[j]->id;
            while (runner >= 0 && runner != idom[b] && runner != b) {
                df_add(df, runner, b);
                runner = idom[runner];
            }
        }
    }
}

bool dom_frontier_contains(const DomFrontier *df, int block, int test_block) {
    if (block < 0 || block >= df->block_count) return false;
    int start = df->frontier_starts[block];
    int size  = df->frontier_sizes[block];
    for (int i = 0; i < size; i++) {
        if (df->frontier_sets[start + i] == test_block) return true;
    }
    return false;
}

void dom_frontier_print(const DomFrontier *df, const CFG *cfg) {
    printf("=== Dominance Frontiers ===\n");
    for (int i = 0; i < cfg->block_count; i++) {
        printf("  DF(%d) = {", i);
        int start = df->frontier_starts[i];
        int size  = df->frontier_sizes[i];
        for (int j = 0; j < size; j++) {
            printf("%s%d", j ? ", " : "", df->frontier_sets[start + j]);
        }
        printf("}\n");
    }
}

/* ─── Liveness Analysis (L8: Advanced, Backward Dataflow) ──────────── */

/*
 * Iterative backward dataflow analysis for liveness.
 *
 * Algorithm:
 *   for each block B: IN[B] = USE[B]; OUT[B] = {}
 *   repeat until no change:
 *     for each block B in reverse postorder:
 *       OUT[B] = ∪_{S∈succ[B]} IN[S]
 *       IN[B]  = USE[B] ∪ (OUT[B] - DEF[B])
 *
 * For simplicity, we track variables by string name and map them to indices.
 */

/* Collect all variable names (temps and user vars) from IR */
static int collect_vars(IRModule *ir, char ***varnames_out) {
    int nvars = 0;
    int cap = 64;
    char **varnames = (char **)malloc(cap * sizeof(char *));

    for (IRInstr *instr = ir->head; instr; instr = instr->next) {
        const char *candidates[3] = { instr->dest, instr->src1, instr->src2 };
        for (int ci = 0; ci < 3; ci++) {
            const char *name = candidates[ci];
            if (!name || name[0] == '\0') continue;
            /* Only track temps and user vars */
            if (name[0] != '%' && name[0] != '#') continue;

            /* Check if already in list */
            bool found = false;
            for (int vi = 0; vi < nvars; vi++) {
                if (strcmp(varnames[vi], name) == 0) { found = true; break; }
            }
            if (!found) {
                if (nvars >= cap) {
                    cap *= 2;
                    varnames = (char **)realloc(varnames, cap * sizeof(char *));
                }
                varnames[nvars] = strdup(name);
                nvars++;
            }
        }
    }

    *varnames_out = varnames;
    return nvars;
}

/* Map variable name to index */
static int var_index(char * const *varnames, int nvars, const char *name) {
    for (int i = 0; i < nvars; i++) {
        if (strcmp(varnames[i], name) == 0) return i;
    }
    return -1;
}

void lv_compute(CFG *cfg, BlockLiveness *block_lv,
                int nblocks, char * const *varnames, int varcount) {
    if (!cfg || !block_lv || nblocks == 0) return;

    /* Use the provided varnames (or compute our own) */
    char **names = varnames;
    int nv  = varcount;
    bool own_names = false;
    if (!names || nv == 0) {
        /* We need IR to collect vars, but lv_compute doesn't take IRModule.
         * For now, use up to LV_MAX_VARS from dest/src fields of block instructions. */
        nv = 0;
        names = (char **)calloc(LV_MAX_VARS, sizeof(char *));
        own_names = true;
        for (int bi = 0; bi < nblocks; bi++) {
            for (IRInstr *p = cfg->blocks[bi]->first; p && p != cfg->blocks[bi]->last->next; p = p->next) {
                const char *cand[3] = { p->dest, p->src1, p->src2 };
                for (int ci = 0; ci < 3; ci++) {
                    if (!cand[ci] || cand[ci][0] == '\0') continue;
                    if (cand[ci][0] != '%' && cand[ci][0] != '#') continue;
                    bool found = false;
                    for (int vi = 0; vi < nv; vi++)
                        if (strcmp(names[vi], cand[ci]) == 0) { found = true; break; }
                    if (!found && nv < LV_MAX_VARS) {
                        names[nv] = strdup(cand[ci]);
                        nv++;
                    }
                }
            }
        }
    }

    if (nv == 0) { if (own_names) free(names); return; }

    /* Initialize: compute USE/DEF per block */
    for (int bi = 0; bi < nblocks; bi++) {
        BlockLiveness *lv = &block_lv[bi];
        memset(lv->def, 0, sizeof(lv->def));
        memset(lv->use, 0, sizeof(lv->use));
        memset(lv->live_in, 0, sizeof(lv->live_in));
        memset(lv->live_out, 0, sizeof(lv->live_out));

        IRInstr *last = cfg->blocks[bi]->last;
        for (IRInstr *p = cfg->blocks[bi]->first;
             p && (bi == nblocks - 1 || p != last->next); p = p->next) {
            if (!p || p->dead) continue;

            /* USEs: operands read before defined in this block */
            const char *uses[2] = { p->src1, p->src2 };
            for (int ui = 0; ui < 2; ui++) {
                if (!uses[ui] || uses[ui][0] == '\0') continue;
                int vi = var_index(names, nv, uses[ui]);
                if (vi >= 0 && !lv->def[vi]) {
                    lv->use[vi] = true;
                }
            }

            /* DEF: destination defined */
            if (p->dest[0]) {
                int vi = var_index(names, nv, p->dest);
                if (vi >= 0) {
                    lv->def[vi] = true;
                }
            }
        }
    }

    /* Iterative fixed-point: backward dataflow */
    bool changed = true;
    int max_iter = nblocks * 10;  /* safety limit */
    int iter = 0;
    while (changed && iter < max_iter) {
        changed = false;
        iter++;

        /* Process in reverse order (approximate reverse postorder) */
        for (int bi = nblocks - 1; bi >= 0; bi--) {
            BlockLiveness *lv = &block_lv[bi];
            BasicBlock *bb = cfg->blocks[bi];

            /* OUT[B] = ∪ IN[succ] */
            bool new_out[LV_MAX_VARS] = {0};
            for (int si = 0; si < bb->succ_count; si++) {
                int sbi = bb->succs[si]->id;
                for (int vi = 0; vi < nv; vi++) {
                    if (block_lv[sbi].live_in[vi]) new_out[vi] = true;
                }
            }

            /* Check if OUT changed */
            for (int vi = 0; vi < nv; vi++) {
                if (new_out[vi] != lv->live_out[vi]) changed = true;
                lv->live_out[vi] = new_out[vi];
            }

            /* IN[B] = USE[B] ∪ (OUT[B] - DEF[B]) */
            bool new_in[LV_MAX_VARS] = {0};
            for (int vi = 0; vi < nv; vi++) {
                new_in[vi] = lv->use[vi] || (lv->live_out[vi] && !lv->def[vi]);
            }
            for (int vi = 0; vi < nv; vi++) {
                if (new_in[vi] != lv->live_in[vi]) changed = true;
                lv->live_in[vi] = new_in[vi];
            }
        }
    }

    if (own_names) {
        for (int i = 0; i < nv; i++) free(names[i]);
        free(names);
    }
}

void lv_print(const CFG *cfg, const BlockLiveness *block_lv, int nblocks,
              char * const *varnames, int nvars) {
    printf("=== Liveness Analysis ===\n");
    for (int bi = 0; bi < nblocks; bi++) {
        printf("  Block %d:\n", bi);
        printf("    IN:  {");
        bool first = true;
        for (int vi = 0; vi < nvars; vi++) {
            if (block_lv[bi].live_in[vi]) {
                printf("%s%s", first ? "" : ", ", varnames[vi]);
                first = false;
            }
        }
        printf("}\n");
        printf("    OUT: {");
        first = true;
        for (int vi = 0; vi < nvars; vi++) {
            if (block_lv[bi].live_out[vi]) {
                printf("%s%s", first ? "" : ", ", varnames[vi]);
                first = false;
            }
        }
        printf("}\n");
    }
}

/* ─── Interference Graph ────────────────────────────────────────────── */

/*
 * Two variables interfere if there exists a program point where both are live.
 * The interference graph is used for register allocation via graph coloring.
 *
 * L8: Chaitin's register allocation via graph coloring (1981).
 * L4: Graph coloring is NP-complete (Karp 1972); Chaitin-Briggs heuristic
 *     yields good results in polynomial time.
 */
InterferenceGraph *ig_build(CFG *cfg, const BlockLiveness *block_lv,
                             int nblocks, int nvars) {
    InterferenceGraph *ig = (InterferenceGraph *)calloc(1, sizeof(InterferenceGraph));
    ig->nvars = nvars;

    ig->matrix = (bool **)malloc(nvars * sizeof(bool *));
    for (int i = 0; i < nvars; i++) {
        ig->matrix[i] = (bool *)calloc(nvars, sizeof(bool));
    }

    ig->degree = (int *)calloc(nvars, sizeof(int));

    /* For each block, variables live-out interfere with each other
     * and with variables defined in the block (except at exit) */
    for (int bi = 0; bi < nblocks; bi++) {
        /* Variables live at any point inside the block interfere */
        for (int vi = 0; vi < nvars; vi++) {
            for (int vj = vi + 1; vj < nvars; vj++) {
                if ((block_lv[bi].live_in[vi] || block_lv[bi].live_out[vi]) &&
                    (block_lv[bi].live_in[vj] || block_lv[bi].live_out[vj])) {
                    if (!ig->matrix[vi][vj]) {
                        ig->matrix[vi][vj] = ig->matrix[vj][vi] = true;
                        ig->degree[vi]++;
                        ig->degree[vj]++;
                    }
                }
            }
        }
    }

    return ig;
}

void ig_destroy(InterferenceGraph *ig) {
    if (!ig) return;
    for (int i = 0; i < ig->nvars; i++) free(ig->matrix[i]);
    free(ig->matrix);
    free(ig->degree);
    free(ig);
}

/* ─── Chaitin-Briggs Register Allocation (L8: Advanced) ───────────── */

/*
 * Graph coloring register allocation:
 *
 * 1. While there exists a node with degree < k:
 *    - Push it onto a stack and remove it from the graph
 * 2. If all remaining nodes have degree >= k:
 *    - Choose a spill candidate (heuristic: highest degree)
 *    - Push and remove it (will be spilled)
 * 3. Pop nodes from stack, assign colors not used by neighbors
 * 4. If a node cannot be colored, mark it for spilling (-1)
 *
 * Reference:
 *   - Chaitin "Register Allocation and Spilling via Graph Coloring" (1982)
 *   - Briggs, Cooper, Torczon "Improvements to Graph Coloring Register
 *     Allocation" (1994) — optimistic coloring
 */
int *ig_alloc_registers(const InterferenceGraph *ig, int k) {
    int n = ig->nvars;
    if (n == 0) return NULL;

    int *result = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) result[i] = -1;

    /* Working copy of degrees and adjacency */
    int *deg = (int *)malloc(n * sizeof(int));
    bool **adj = (bool **)malloc(n * sizeof(bool *));
    for (int i = 0; i < n; i++) {
        deg[i] = ig->degree[i];
        adj[i] = (bool *)malloc(n * sizeof(bool));
        memcpy(adj[i], ig->matrix[i], n * sizeof(bool));
    }

    /* Stack for Chaitin-Briggs */
    int *stack = (int *)malloc(n * sizeof(int));
    int stack_top = 0;
    bool *removed = (bool *)calloc(n, sizeof(bool));

    /* Phase 1: Simplify — repeatedly remove low-degree nodes */
    bool progress = true;
    while (progress) {
        progress = false;
        for (int v = 0; v < n; v++) {
            if (removed[v]) continue;
            if (deg[v] < k) {
                removed[v] = true;
                stack[stack_top++] = v;
                /* Decrement degree of neighbors */
                for (int u = 0; u < n; u++) {
                    if (adj[v][u]) deg[u]--;
                }
                progress = true;
            }
        }

        /* If stuck, spill one node (heuristic: highest degree) */
        if (!progress) {
            int spill_cand = -1, max_deg = -1;
            for (int v = 0; v < n; v++) {
                if (!removed[v] && deg[v] > max_deg) {
                    max_deg = deg[v];
                    spill_cand = v;
                }
            }
            if (spill_cand >= 0) {
                removed[spill_cand] = true;
                stack[stack_top++] = spill_cand;
                for (int u = 0; u < n; u++) {
                    if (adj[spill_cand][u]) deg[u]--;
                }
                progress = true;
            }
        }
    }

    /* Phase 2: Select — pop nodes, assign colors */
    bool *used_colors = (bool *)malloc(k * sizeof(bool));

    while (stack_top > 0) {
        int v = stack[--stack_top];

        /* Colors used by neighbors already assigned */
        memset(used_colors, 0, k * sizeof(bool));
        for (int u = 0; u < n; u++) {
            if (ig->matrix[v][u] && result[u] >= 0 && result[u] < k) {
                used_colors[result[u]] = true;
            }
        }

        /* Find first available color */
        int color = -1;
        for (int c = 0; c < k; c++) {
            if (!used_colors[c]) { color = c; break; }
        }
        result[v] = color;  /* -1 means spill */
    }

    free(used_colors);
    free(removed);
    free(stack);
    for (int i = 0; i < n; i++) free(adj[i]);
    free(adj);
    free(deg);

    return result;
}
