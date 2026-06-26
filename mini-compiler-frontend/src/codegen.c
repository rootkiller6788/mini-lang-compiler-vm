#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Helper: generate unique label for branches */
static int cg_label_id = 0;

/* ─── String Buffer ────────────────────────────────────────────────── */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} StringBuffer;

static void sb_init(StringBuffer *sb, size_t initial) {
    sb->buf = (char *)malloc(initial);
    sb->buf[0] = '\0';
    sb->len = 0;
    sb->cap = initial;
}

static void sb_append(StringBuffer *sb, const char *str) {
    size_t slen = strlen(str);
    if (sb->len + slen + 1 > sb->cap) {
        sb->cap = (sb->len + slen + 1) * 2;
        sb->buf = (char *)realloc(sb->buf, sb->cap);
    }
    memcpy(sb->buf + sb->len, str, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
}

static void sb_appendf(StringBuffer *sb, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    sb_append(sb, tmp);
}

static void sb_free(StringBuffer *sb) {
    free(sb->buf);
    sb->buf = NULL;
}


/* ═══════════════════════════════════════════════════════════════════════
 * C Code Emitter
 *
 * L5: Instruction Selection — translating IR ops to C constructs.
 * L6: Canonical Compiler Problem — full compilation pipeline.
 *
 * Each IR temp becomes a C local variable. Basic block labels become
 * C labels. Branches become C goto statements. This maps IR directly
 * to C, letting the C compiler handle final register allocation.
 * ═══════════════════════════════════════════════════════════════════════ */

char *codegen_emit_c(IRModule *ir, const char *func_name, int num_regs) {
    (void)num_regs;
    StringBuffer sb;
    sb_init(&sb, 4096);
    cg_label_id = 0;

    sb_appendf(&sb, "/* Generated C from IR — function %s */\n",
               func_name ? func_name : "unknown");
    sb_appendf(&sb, "int %s(void) {\n", func_name ? func_name : "unknown_func");

    /* First pass: declare all temp/user variables */
    sb_append(&sb, "    /* --- variables --- */\n");
    for (IRInstr *p = ir->head; p; p = p->next) {
        if (p->dest[0] && (p->dest[0] == '%' || p->dest[0] == '#')) {
            if (p->opcode != IR_LABEL && p->opcode != IR_NOP) {
                sb_appendf(&sb, "    int %s;\n", p->dest);
            }
        }
    }

    sb_append(&sb, "\n    /* --- code --- */\n");

    /* Second pass: emit instructions */
    for (IRInstr *p = ir->head; p; p = p->next) {
        if (p->dead) continue;

        switch (p->opcode) {
        case IR_LABEL:
            sb_appendf(&sb, "    %s: ;\n", p->src1);
            break;
        case IR_LOAD_IMM:
            sb_appendf(&sb, "    %s = %d;\n", p->dest, p->imm);
            break;
        case IR_COPY:
            sb_appendf(&sb, "    %s = %s;\n", p->dest, p->src1);
            break;
        case IR_BINARY:
            if (p->op == 'L')
                sb_appendf(&sb, "    %s = (%s <= %s);\n", p->dest, p->src1, p->src2);
            else if (p->op == 'G')
                sb_appendf(&sb, "    %s = (%s >= %s);\n", p->dest, p->src1, p->src2);
            else if (p->op == '=')
                sb_appendf(&sb, "    %s = (%s == %s);\n", p->dest, p->src1, p->src2);
            else if (p->op == '!')
                sb_appendf(&sb, "    %s = (%s != %s);\n", p->dest, p->src1, p->src2);
            else if (p->op == '&')
                sb_appendf(&sb, "    %s = (%s && %s);\n", p->dest, p->src1, p->src2);
            else if (p->op == '|')
                sb_appendf(&sb, "    %s = (%s || %s);\n", p->dest, p->src1, p->src2);
            else
                sb_appendf(&sb, "    %s = (%s %c %s);\n", p->dest, p->src1, p->op, p->src2);
            break;
        case IR_UNARY:
            sb_appendf(&sb, "    %s = %c%s;\n", p->dest, p->op, p->src1);
            break;
        case IR_BR:
            sb_appendf(&sb, "    goto %s;\n", p->src1);
            break;
        case IR_BR_COND:
            sb_appendf(&sb, "    if (%s) goto %s;\n", p->src1, p->src2);
            break;
        case IR_BR_NOT_COND:
            sb_appendf(&sb, "    if (!(%s)) goto %s;\n", p->src1, p->src2);
            break;
        case IR_RET:
            sb_appendf(&sb, "    return %s;\n", p->src1[0] ? p->src1 : "0");
            break;
        case IR_ALLOCA:
            break;
        case IR_CALL:
            sb_appendf(&sb, "    %s = %s(", p->dest, p->src1);
            {
                int nargs = p->imm;
                char *args[16];
                int arg_count = 0;
                for (IRInstr *q = p->prev; q && arg_count < nargs; q = q->prev) {
                    if (q->opcode == IR_PARAM && !q->dead) {
                        args[nargs - 1 - arg_count] = q->src1;
                        arg_count++;
                        q->dead = true;
                    }
                }
                for (int a = 0; a < arg_count; a++)
                    sb_appendf(&sb, "%s%s", a ? ", " : "", args[a]);
            }
            sb_append(&sb, ");\n");
            break;
        case IR_PARAM:
            break;
        case IR_STORE:
            sb_appendf(&sb, "    *(%s) = %s;\n", p->src1, p->src2);
            break;
        case IR_LOAD:
            sb_appendf(&sb, "    %s = *(%s);\n", p->dest, p->src1);
            break;
        case IR_PHI:
            sb_appendf(&sb, "    %s = %s;  /* phi */\n", p->dest, p->src1[0] ? p->src1 : "0");
            break;
        case IR_NOP:
            sb_append(&sb, "    ;\n");
            break;
        default:
            break;
        }
    }

    sb_append(&sb, "}\n");
    return sb.buf;
}

char *codegen_emit_c_program(IRProgram *prog, int num_regs) {
    StringBuffer sb;
    sb_init(&sb, 16384);
    sb_append(&sb, "/* Generated C program from IR */\n");
    sb_append(&sb, "#include <stdio.h>\n\n");
    for (int i = 0; i < prog->count; i++)
        sb_appendf(&sb, "int %s(void);\n", prog->func_names[i]);
    sb_append(&sb, "\n");
    for (int i = 0; i < prog->count; i++) {
        char *func_c = codegen_emit_c(prog->functions[i], prog->func_names[i], num_regs);
        sb_append(&sb, func_c);
        sb_append(&sb, "\n");
        free(func_c);
    }
    sb_append(&sb, "int main(void) {\n    return main();\n}\n");
    cg_label_id = 0;
    return sb.buf;
}


/* ═══════════════════════════════════════════════════════════════════════
 * Stack VM Bytecode Emitter & Interpreter
 *
 * L6: Canonical problem — stack-based VM is the traditional educational
 *     compiler target (JVM, CLR, WebAssembly all use stack machines).
 * L4: The stack machine model is Turing-complete (Church-Turing thesis).
 *
 * Instruction set modeled after a simplified JVM/WebAssembly subset.
 * ═══════════════════════════════════════════════════════════════════════ */

static const char *vm_opcode_names[VM_COUNT] = {
    "PUSH_IMM", "PUSH_VAR", "POP_VAR",
    "ADD", "SUB", "MUL", "DIV",
    "NEG", "NOT",
    "EQ", "NE", "LT", "LE", "GT", "GE",
    "AND", "OR",
    "JMP", "JMP_FALSE", "LABEL",
    "CALL", "RET", "HALT",
    "DUP", "SWAP", "PRINT"
};

const char *vm_opcode_name(VMOpcode op) {
    if (op >= 0 && op < VM_COUNT) return vm_opcode_names[op];
    return "UNKNOWN";
}

void vm_program_print(const VMProgram *prog) {
    if (!prog) return;
    printf("=== VM Bytecode (%d instructions) ===\n", prog->count);
    for (int i = 0; i < prog->count; i++) {
        printf("  %4d: %-12s", i, vm_opcode_name(prog->instructions[i].opcode));
        switch (prog->instructions[i].opcode) {
        case VM_PUSH_IMM: case VM_PUSH_VAR: case VM_POP_VAR:
        case VM_JMP: case VM_JMP_FALSE: case VM_CALL:
            printf(" %d", prog->instructions[i].operand);
            break;
        default: break;
        }
        printf("\n");
    }
}

void vm_program_destroy(VMProgram *prog) {
    if (!prog) return;
    free(prog->instructions);
    free(prog->labels);
    free(prog);
}

static void vm_emit(VMProgram *prog, VMOpcode op, int operand) {
    if (prog->count >= prog->capacity) {
        prog->capacity = prog->capacity ? prog->capacity * 2 : 128;
        prog->instructions = (VMInstr *)realloc(prog->instructions,
                                                 prog->capacity * sizeof(VMInstr));
    }
    prog->instructions[prog->count].opcode = op;
    prog->instructions[prog->count].operand = operand;
    prog->instructions[prog->count].line = 0;
    prog->count++;
}

/*
 * Translate IR to VM bytecode.
 * Variables are assigned indices 0..N for direct VM addressing.
 */
VMProgram *vm_program_from_ir(IRModule *ir) {
    if (!ir) return NULL;

    VMProgram *prog = (VMProgram *)calloc(1, sizeof(VMProgram));
    prog->capacity = 128;
    prog->instructions = (VMInstr *)malloc(prog->capacity * sizeof(VMInstr));
    prog->labels = (int *)malloc(64 * sizeof(int));
    for (int i = 0; i < 64; i++) prog->labels[i] = -1;
    prog->label_count = 0;

    /* Variable name → index */
    const char *varnames[256];
    int nvars = 0;

    /* Label → position */
    typedef struct { char name[64]; int pos; } LabelMap;
    LabelMap label_map[128];
    int nlabels = 0;

    /* First pass: record label positions (approximate) */
    {
        int pos = 0;
        for (IRInstr *p = ir->head; p; p = p->next) {
            if (p->dead) continue;
            if (p->opcode == IR_LABEL && nlabels < 128) {
                snprintf(label_map[nlabels].name, sizeof(label_map[nlabels].name),
                         "%s", p->src1);
                label_map[nlabels].pos = pos;
                nlabels++;
            }
            /* Approximate: each IR instr maps to 1-4 VM instrs */
            switch (p->opcode) {
            case IR_LABEL:   pos += 1; break;
            case IR_LOAD_IMM: pos += (p->dest[0] ? 2 : 1); break;
            case IR_BINARY:   pos += (p->dest[0] ? 4 : 3); break;
            case IR_UNARY:    pos += (p->dest[0] ? 3 : 2); break;
            case IR_COPY:     pos += 2; break;
            case IR_BR:       pos += 1; break;
            case IR_BR_COND: case IR_BR_NOT_COND: pos += 2; break;
            case IR_RET:      pos += 1; break;
            case IR_CALL:     pos += (1 + p->imm + (p->dest[0] ? 1 : 0)); break;
            case IR_PARAM:    pos += 1; break;
            default: pos += 1; break;
            }
        }
    }

    /* Helper: get or assign variable index */
    #define GETIDX(name_str, out_var) do { \
        out_var = -1; \
        for (int _v = 0; _v < nvars; _v++) { \
            if (strcmp(varnames[_v], name_str) == 0) { out_var = _v; break; } \
        } \
        if (out_var < 0 && nvars < 256) { \
            varnames[nvars] = name_str; \
            out_var = nvars++; \
        } \
    } while(0)

    /* Helper: find label position */
    #define FINDLABEL(lname) ({ \
        int _p = -1; \
        for (int _l = 0; _l < nlabels; _l++) \
            if (strcmp(label_map[_l].name, lname) == 0) { _p = label_map[_l].pos; break; } \
        _p; \
    })

    /* Second pass: emit */
    for (IRInstr *p = ir->head; p; p = p->next) {
        if (p->dead) continue;
        int vi, vi2;

        switch (p->opcode) {
        case IR_LABEL:
            vm_emit(prog, VM_LABEL, 0);
            break;
        case IR_LOAD_IMM:
            vm_emit(prog, VM_PUSH_IMM, p->imm);
            if (p->dest[0]) { GETIDX(p->dest, vi); vm_emit(prog, VM_POP_VAR, vi); }
            break;
        case IR_COPY:
            if (p->src1[0] && p->dest[0]) {
                GETIDX(p->src1, vi); vm_emit(prog, VM_PUSH_VAR, vi);
                GETIDX(p->dest, vi2); vm_emit(prog, VM_POP_VAR, vi2);
            }
            break;
        case IR_BINARY: {
            if (p->src1[0]) { GETIDX(p->src1, vi); vm_emit(prog, VM_PUSH_VAR, vi); }
            if (p->src2[0]) { GETIDX(p->src2, vi); vm_emit(prog, VM_PUSH_VAR, vi); }
            VMOpcode op = VM_ADD;
            switch (p->op) {
            case '+': op = VM_ADD; break; case '-': op = VM_SUB; break;
            case '*': op = VM_MUL; break; case '/': op = VM_DIV; break;
            case '=': op = VM_EQ;  break; case '!': op = VM_NE;  break;
            case '<': op = VM_LT;  break; case '>': op = VM_GT;  break;
            case 'L': op = VM_LE;  break; case 'G': op = VM_GE;  break;
            case '&': op = VM_AND; break; case '|': op = VM_OR;  break;
            default: break;
            }
            vm_emit(prog, op, 0);
            if (p->dest[0]) { GETIDX(p->dest, vi); vm_emit(prog, VM_POP_VAR, vi); }
            break;
        }
        case IR_UNARY:
            if (p->src1[0]) { GETIDX(p->src1, vi); vm_emit(prog, VM_PUSH_VAR, vi); }
            vm_emit(prog, (p->op == '-' || p->op == 'N') ? VM_NEG : VM_NOT, 0);
            if (p->dest[0]) { GETIDX(p->dest, vi); vm_emit(prog, VM_POP_VAR, vi); }
            break;
        case IR_BR:
            vm_emit(prog, VM_JMP, FINDLABEL(p->src1));
            break;
        case IR_BR_COND:
            if (p->src1[0]) { GETIDX(p->src1, vi); vm_emit(prog, VM_PUSH_VAR, vi); }
            vm_emit(prog, VM_JMP_FALSE, FINDLABEL(p->src2));
            break;
        case IR_BR_NOT_COND:
            if (p->src1[0]) { GETIDX(p->src1, vi); vm_emit(prog, VM_PUSH_VAR, vi); }
            vm_emit(prog, VM_NOT, 0);
            vm_emit(prog, VM_JMP_FALSE, FINDLABEL(p->src2));
            break;
        case IR_RET:
            vm_emit(prog, VM_RET, 0);
            break;
        case IR_CALL:
            for (int a = 0; a < p->imm; a++) {
                vm_emit(prog, VM_PUSH_IMM, 0);  /* placeholder args */
            }
            vm_emit(prog, VM_CALL, p->imm);
            if (p->dest[0]) { GETIDX(p->dest, vi); vm_emit(prog, VM_POP_VAR, vi); }
            break;
        case IR_PARAM: case IR_ALLOCA: case IR_STORE: case IR_LOAD:
        case IR_PHI: case IR_NOP:
            break;
        default: break;
        }
    }

    vm_emit(prog, VM_HALT, 0);
    #undef GETIDX
    #undef FINDLABEL
    return prog;
}

/* ─── VM Interpreter ────────────────────────────────────────────────── */

#define VM_STACK_SIZE 1024
#define VM_VARS_MAX   256
#define VM_CALL_STACK 64

int vm_execute(const VMProgram *prog, bool trace) {
    if (!prog || prog->count == 0) return 0;

    int stack[VM_STACK_SIZE];
    int sp = 0;
    int vars[VM_VARS_MAX] = {0};
    int ip = 0;
    int call_stack[VM_CALL_STACK];
    int call_sp = 0;

    #define PUSH(v) do { if (sp < VM_STACK_SIZE) stack[sp++] = (v); } while(0)
    #define POP()   ((sp > 0) ? stack[--sp] : 0)

    while (ip >= 0 && ip < prog->count) {
        VMInstr instr = prog->instructions[ip];
        if (trace)
            printf("  [%4d] %-12s %d  (sp=%d)\n",
                   ip, vm_opcode_name(instr.opcode), instr.operand, sp);

        switch (instr.opcode) {
        case VM_PUSH_IMM: PUSH(instr.operand); ip++; break;
        case VM_PUSH_VAR:
            PUSH((instr.operand >= 0 && instr.operand < VM_VARS_MAX)
                 ? vars[instr.operand] : 0);
            ip++; break;
        case VM_POP_VAR:
            if (instr.operand >= 0 && instr.operand < VM_VARS_MAX)
                vars[instr.operand] = POP();
            ip++; break;
        case VM_ADD: { int b=POP(); int a=POP(); PUSH(a+b); ip++; break; }
        case VM_SUB: { int b=POP(); int a=POP(); PUSH(a-b); ip++; break; }
        case VM_MUL: { int b=POP(); int a=POP(); PUSH(a*b); ip++; break; }
        case VM_DIV: { int b=POP(); int a=POP(); PUSH(b?a/b:0); ip++; break; }
        case VM_NEG: { PUSH(-POP()); ip++; break; }
        case VM_NOT: { PUSH(!POP()); ip++; break; }
        case VM_EQ:  { int b=POP(); int a=POP(); PUSH(a==b); ip++; break; }
        case VM_NE:  { int b=POP(); int a=POP(); PUSH(a!=b); ip++; break; }
        case VM_LT:  { int b=POP(); int a=POP(); PUSH(a<b);  ip++; break; }
        case VM_LE:  { int b=POP(); int a=POP(); PUSH(a<=b); ip++; break; }
        case VM_GT:  { int b=POP(); int a=POP(); PUSH(a>b);  ip++; break; }
        case VM_GE:  { int b=POP(); int a=POP(); PUSH(a>=b); ip++; break; }
        case VM_AND: { int b=POP(); int a=POP(); PUSH(a&&b); ip++; break; }
        case VM_OR:  { int b=POP(); int a=POP(); PUSH(a||b); ip++; break; }
        case VM_JMP:
            ip = (instr.operand >= 0 && instr.operand < prog->count) ? instr.operand : ip + 1;
            break;
        case VM_JMP_FALSE: {
            int cond = POP();
            ip = (!cond && instr.operand >= 0 && instr.operand < prog->count)
                 ? instr.operand : ip + 1;
            break;
        }
        case VM_LABEL: ip++; break;
        case VM_CALL:
            if (call_sp < VM_CALL_STACK) call_stack[call_sp++] = ip + 1;
            ip = 0;  /* simplified: jump to function entry */
            break;
        case VM_RET:
            ip = (call_sp > 0) ? call_stack[--call_sp] : prog->count;
            break;
        case VM_HALT: ip = prog->count; break;
        case VM_DUP:  if (sp > 0) PUSH(stack[sp-1]); ip++; break;
        case VM_SWAP:
            if (sp >= 2) { int t=stack[sp-1]; stack[sp-1]=stack[sp-2]; stack[sp-2]=t; }
            ip++; break;
        case VM_PRINT: { int v=POP(); printf("%d\n", v); ip++; break; }
        default: ip++; break;
        }
    }
    #undef PUSH
    #undef POP
    return (sp > 0) ? stack[sp-1] : 0;
}
