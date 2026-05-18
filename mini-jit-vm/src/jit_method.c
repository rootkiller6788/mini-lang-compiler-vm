#include "jit_method.h"
#include <string.h>
#include <time.h>

void jit_compiler_init(JITCompiler* jc, int32_t threshold) {
    jc->compilation_threshold = threshold;
    jc->call_count            = 0;
    jc->is_compiled           = false;
    memset(&jc->code_block, 0, sizeof(jc->code_block));
    jc->code_block.compiled_fn_ptr = NULL;
    jc->code_block.size             = 0;
}

bool jit_should_compile(const JITCompiler* jc) {
    return !jc->is_compiled && jc->call_count >= jc->compilation_threshold;
}

void jit_record_call(JITCompiler* jc) {
    jc->call_count++;
}

static int64_t jit_native_add(int64_t a, int64_t b) { return a + b; }
static int64_t jit_native_sub(int64_t a, int64_t b) { return a - b; }
static int64_t jit_native_mul(int64_t a, int64_t b) { return a * b; }
static int64_t jit_native_div(int64_t a, int64_t b) { return b ? a / b : 0; }
static int64_t jit_native_neg(int64_t a, int64_t _unused) { (void)_unused; return -a; }
static int64_t jit_native_and(int64_t a, int64_t b) { return (a && b) ? 1 : 0; }
static int64_t jit_native_or(int64_t a, int64_t b)  { return (a || b) ? 1 : 0; }
static int64_t jit_native_not(int64_t a, int64_t _unused) { (void)_unused; return a ? 0 : 1; }

typedef int64_t (*BinOp)(int64_t, int64_t);
typedef int64_t (*UnaryOp)(int64_t, int64_t);

typedef struct {
    OpCode  op;
    int32_t arg;
} DecodedInstr;

static int32_t decode_bytecode(const ByteCode* bc, DecodedInstr* decoded, int32_t max) {
    int32_t count = 0;
    for (int32_t i = 0; i < bc->num_inst && count < max; i++) {
        int32_t instr = bc->instructions[i];
        decoded[count].op  = (OpCode)(instr & 0xFF);
        decoded[count].arg = (instr >> 8) & 0xFFFFFF;
        count++;
    }
    return count;
}

static int64_t run_interpreter(const ByteCode* bc) {
    int64_t stack[VM_STACK_SIZE];
    int32_t sp = 0;

    for (int32_t ip = 0; ip < bc->num_inst; ip++) {
        int32_t instr = bc->instructions[ip];
        OpCode op = (OpCode)(instr & 0xFF);
        int32_t arg = (instr >> 8) & 0xFFFFFF;

        switch (op) {
            case OP_PUSH:
                if (arg >= 0 && arg < bc->const_count) {
                    stack[sp++] = bc->const_pool[arg].data.int_val;
                }
                break;
            case OP_ADD: { int64_t b = stack[--sp]; int64_t a = stack[--sp]; stack[sp++] = a + b; break; }
            case OP_SUB: { int64_t b = stack[--sp]; int64_t a = stack[--sp]; stack[sp++] = a - b; break; }
            case OP_MUL: { int64_t b = stack[--sp]; int64_t a = stack[--sp]; stack[sp++] = a * b; break; }
            case OP_DIV: { int64_t b = stack[--sp]; stack[sp++] /= b; break; }
            case OP_NEG: { stack[sp - 1] = -stack[sp - 1]; break; }
            case OP_NOT: { stack[sp - 1] = stack[sp - 1] ? 0 : 1; break; }
            case OP_AND: { int64_t b = stack[--sp]; int64_t a = stack[--sp]; stack[sp++] = (a && b) ? 1 : 0; break; }
            case OP_OR:  { int64_t b = stack[--sp]; int64_t a = stack[--sp]; stack[sp++] = (a || b) ? 1 : 0; break; }
            case OP_JMP: ip = arg; break;
            case OP_JMP_IF_FALSE: if (!stack[--sp]) ip = arg; break;
            case OP_RET: break;
            case OP_HALT: return stack[sp > 0 ? sp - 1 : 0];
            default: break;
        }
    }
    return stack[sp > 0 ? sp - 1 : 0];
}

bool jit_compile_function(JITCompiler* jc, const ByteCode* bc) {
    DecodedInstr decoded[JIT_CODE_BUFFER_SIZE / 4];
    int32_t instr_count = decode_bytecode(bc, decoded, JIT_CODE_BUFFER_SIZE / 4);

    jc->code_block.size = 0;

    for (int32_t i = 0; i < instr_count; i++) {
        OpCode op = decoded[i].op;
        int32_t arg = decoded[i].arg;

        switch (op) {
            case OP_PUSH: {
                int64_t val = (arg >= 0 && arg < bc->const_count)
                                  ? bc->const_pool[arg].data.int_val
                                  : 0;
                jc->code_block.native_code[jc->code_block.size++] = (uint8_t)((val) & 0xFF);
                jc->code_block.native_code[jc->code_block.size++] = (uint8_t)((val >> 8) & 0xFF);
                jc->code_block.native_code[jc->code_block.size++] = (uint8_t)((val >> 16) & 0xFF);
                jc->code_block.native_code[jc->code_block.size++] = (uint8_t)((val >> 24) & 0xFF);
                jc->code_block.native_code[jc->code_block.size++] = 0x00;
                break;
            }
            case OP_ADD:
                jc->code_block.native_code[jc->code_block.size++] = 0x01;
                break;
            case OP_SUB:
                jc->code_block.native_code[jc->code_block.size++] = 0x02;
                break;
            case OP_MUL:
                jc->code_block.native_code[jc->code_block.size++] = 0x03;
                break;
            case OP_DIV:
                jc->code_block.native_code[jc->code_block.size++] = 0x04;
                break;
            case OP_NEG:
                jc->code_block.native_code[jc->code_block.size++] = 0x05;
                break;
            case OP_AND:
                jc->code_block.native_code[jc->code_block.size++] = 0x06;
                break;
            case OP_OR:
                jc->code_block.native_code[jc->code_block.size++] = 0x07;
                break;
            case OP_NOT:
                jc->code_block.native_code[jc->code_block.size++] = 0x08;
                break;
            case OP_JMP:
                jc->code_block.native_code[jc->code_block.size++] = 0x09;
                jc->code_block.native_code[jc->code_block.size++] = (uint8_t)(arg & 0xFF);
                jc->code_block.native_code[jc->code_block.size++] = (uint8_t)((arg >> 8) & 0xFF);
                break;
            case OP_JMP_IF_FALSE:
                jc->code_block.native_code[jc->code_block.size++] = 0x0A;
                jc->code_block.native_code[jc->code_block.size++] = (uint8_t)(arg & 0xFF);
                jc->code_block.native_code[jc->code_block.size++] = (uint8_t)((arg >> 8) & 0xFF);
                break;
            case OP_RET:
                jc->code_block.native_code[jc->code_block.size++] = 0x0B;
                break;
            case OP_HALT:
                jc->code_block.native_code[jc->code_block.size++] = 0x0C;
                break;
            default:
                jc->code_block.native_code[jc->code_block.size++] = 0xFF;
                break;
        }
    }
    jc->is_compiled = true;
    return true;
}

bool jit_install_code(JITCompiler* jc) {
    (void)jc;
    return true;
}

int64_t jit_execute_native(const JITCompiler* jc) {
    if (!jc || !jc->is_compiled || jc->code_block.size == 0) return 0;

    int64_t stack[VM_STACK_SIZE];
    int32_t sp      = 0;
    int32_t ip      = 0;
    int32_t size    = jc->code_block.size;
    const uint8_t* code = jc->code_block.native_code;

    while (ip < size) {
        uint8_t op = code[ip++];

        switch (op) {
            case 0x00: {
                int64_t val = (int64_t)(uint32_t)(
                    code[ip] | (code[ip + 1] << 8) |
                    (code[ip + 2] << 16) | (code[ip + 3] << 24));
                ip += 4;
                stack[sp++] = val;
                break;
            }
            case 0x01: { int64_t b = stack[--sp]; stack[sp - 1] = stack[sp - 1] + b; break; }
            case 0x02: { int64_t b = stack[--sp]; stack[sp - 1] = stack[sp - 1] - b; break; }
            case 0x03: { int64_t b = stack[--sp]; stack[sp - 1] = stack[sp - 1] * b; break; }
            case 0x04: { int64_t b = stack[--sp]; stack[sp - 1] = b ? stack[sp - 1] / b : 0; break; }
            case 0x05: { stack[sp - 1] = -stack[sp - 1]; break; }
            case 0x06: { int64_t b = stack[--sp]; stack[sp - 1] = (stack[sp - 1] && b) ? 1 : 0; break; }
            case 0x07: { int64_t b = stack[--sp]; stack[sp - 1] = (stack[sp - 1] || b) ? 1 : 0; break; }
            case 0x08: { stack[sp - 1] = stack[sp - 1] ? 0 : 1; break; }
            case 0x09: {
                int32_t arg = code[ip] | (code[ip + 1] << 8);
                ip += 2;
                ip = arg;
                break;
            }
            case 0x0A: {
                int32_t arg = code[ip] | (code[ip + 1] << 8);
                ip += 2;
                if (!stack[--sp]) ip = arg;
                break;
            }
            case 0x0B:
                return stack[sp > 0 ? sp - 1 : 0];
            case 0x0C:
                return stack[sp > 0 ? sp - 1 : 0];
            default:
                fprintf(stderr, "jit_execute_native: unknown op 0x%02X at ip=%d\n", op, ip - 1);
                return 0;
        }
    }
    return stack[sp > 0 ? sp - 1 : 0];
}

int64_t jit_interp_fallback(const ByteCode* bc) {
    return run_interpreter(bc);
}
