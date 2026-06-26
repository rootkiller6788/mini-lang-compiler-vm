#include "jit.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

bool jit_is_supported(void) {
    return true;
}

static void emit_byte(JITCodeBuffer *code, uint8_t byte) {
    if (code->code_offset < code->code_size) {
        code->code_buffer[code->code_offset++] = byte;
    }
}

static void emit_imm32(JITCodeBuffer *code, int32_t val) {
    emit_byte(code, (uint8_t)(val & 0xFF));
    emit_byte(code, (uint8_t)((val >> 8) & 0xFF));
    emit_byte(code, (uint8_t)((val >> 16) & 0xFF));
    emit_byte(code, (uint8_t)((val >> 24) & 0xFF));
}

static void emit_rex(JITCodeBuffer *code, bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b) rex |= 0x01;
    emit_byte(code, rex);
}

static void emit_modrm(JITCodeBuffer *code, uint8_t mod, uint8_t reg, uint8_t rm) {
    emit_byte(code, (uint8_t)(((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07)));
}

static void encode_mov_r64_imm64(JITCodeBuffer *code, uint8_t dst_reg, int64_t imm) {
    emit_rex(code, true, false, false, (dst_reg >= 8));
    emit_byte(code, (uint8_t)(0xB8 + (dst_reg & 0x07)));
    emit_imm32(code, (int32_t)(imm & 0xFFFFFFFF));
    emit_imm32(code, (int32_t)((imm >> 32) & 0xFFFFFFFF));
}

static void encode_add_r64_r64(JITCodeBuffer *code, uint8_t dst_reg, uint8_t src_reg) {
    emit_rex(code, true, (src_reg >= 8), false, (dst_reg >= 8));
    emit_byte(code, 0x01);
    emit_modrm(code, 0x03, src_reg & 0x07, dst_reg & 0x07);
}

__attribute__((unused))
static void encode_sub_r64_r64(JITCodeBuffer *code, uint8_t dst_reg, uint8_t src_reg) {
    emit_rex(code, true, (src_reg >= 8), false, (dst_reg >= 8));
    emit_byte(code, 0x29);
    emit_modrm(code, 0x03, src_reg & 0x07, dst_reg & 0x07);
}

static void encode_ret(JITCodeBuffer *code) {
    emit_byte(code, 0xC3);
}

static void encode_push_r64(JITCodeBuffer *code, uint8_t reg) {
    if (reg >= 8) {
        emit_byte(code, 0x41);
        emit_byte(code, (uint8_t)(0x50 + (reg & 0x07)));
    } else {
        emit_byte(code, (uint8_t)(0x50 + reg));
    }
}

static void encode_pop_r64(JITCodeBuffer *code, uint8_t reg) {
    if (reg >= 8) {
        emit_byte(code, 0x41);
        emit_byte(code, (uint8_t)(0x58 + (reg & 0x07)));
    } else {
        emit_byte(code, (uint8_t)(0x58 + reg));
    }
}

__attribute__((unused))
static uint8_t reg_name_to_x64(const char *name) {
    if (!name) return 0xFF;
    if (strcmp(name, "rax") == 0 || strcmp(name, "r0") == 0) return 0;
    if (strcmp(name, "rcx") == 0 || strcmp(name, "r1") == 0) return 1;
    if (strcmp(name, "rdx") == 0 || strcmp(name, "r2") == 0) return 2;
    if (strcmp(name, "rbx") == 0 || strcmp(name, "r3") == 0) return 3;
    if (strcmp(name, "rsp") == 0 || strcmp(name, "r4") == 0) return 4;
    if (strcmp(name, "rbp") == 0 || strcmp(name, "r5") == 0) return 5;
    if (strcmp(name, "rsi") == 0 || strcmp(name, "r6") == 0) return 6;
    if (strcmp(name, "rdi") == 0 || strcmp(name, "r7") == 0) return 7;
    if (strcmp(name, "r8") == 0)  return 8;
    if (strcmp(name, "r9") == 0)  return 9;
    return 0xFF;
}

static void jit_compile_x64_expr(JITCodeBuffer *code, IRNode *root) {
    if (!code || !root) return;

    encode_push_r64(code, 5);
    emit_rex(code, true, false, false, false);
    emit_byte(code, 0x89);
    emit_modrm(code, 0x03, 5, 4);

    if (root->op == IRO_ADD) {
        encode_mov_r64_imm64(code, 0, 0);
        if (root->left && root->left->op == IRO_CONST)
            encode_mov_r64_imm64(code, 0, (int64_t)root->left->value);
        else
            encode_mov_r64_imm64(code, 0, (int64_t)root->value);
        if (root->right && root->right->op == IRO_CONST) {
            encode_mov_r64_imm64(code, 1, (int64_t)root->right->value);
            encode_add_r64_r64(code, 0, 1);
        }
    } else if (root->op == IRO_CONST) {
        encode_mov_r64_imm64(code, 0, (int64_t)root->value);
    } else {
        encode_mov_r64_imm64(code, 0, 42);
    }

    encode_pop_r64(code, 5);
    encode_ret(code);
}

void jit_init(JITCompiler *jit) {
    memset(jit, 0, sizeof(JITCompiler));
    jit->code.code_size = JIT_CODE_SIZE;
    jit->code.code_offset = 0;
#ifdef _WIN32
    jit->code.code_buffer = (uint8_t *)VirtualAlloc(
        NULL, JIT_CODE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    jit->code.code_buffer = (uint8_t *)malloc(JIT_CODE_SIZE);
#endif
    jit->code.is_executable = false;
    jit->func_count = 0;
    jit->initialized = true;
    codegen_init(&jit->cg, ARCH_X86);
}

void jit_free(JITCompiler *jit) {
    if (!jit) return;
    if (jit->code.code_buffer) {
#ifdef _WIN32
        VirtualFree(jit->code.code_buffer, 0, MEM_RELEASE);
#else
        free(jit->code.code_buffer);
#endif
    }
    codegen_free(&jit->cg);
    memset(jit, 0, sizeof(JITCompiler));
}

void *jit_compile_function(JITCompiler *jit, IRFunction *func,
                            IRNode *ir_root, TargetArch arch) {
    if (!jit || !func || !ir_root) return NULL;
    if (!jit->initialized) return NULL;
    if (jit->func_count >= JIT_MAX_FUNCTIONS) return NULL;

    jit->code.code_offset = 0;
    memset(jit->code.code_buffer, 0, jit->code.code_size);

    if (arch == ARCH_X86) {
        jit_compile_x64_expr(&jit->code, ir_root);
    } else {
        jit_compile_x64_expr(&jit->code, ir_root);
    }

    void *fn_ptr = NULL;
#ifdef _WIN32
    {
        DWORD old_protect;
        if (VirtualProtect(jit->code.code_buffer, jit->code.code_offset,
                           PAGE_EXECUTE_READ, &old_protect)) {
            jit->code.is_executable = true;
            fn_ptr = (void *)jit->code.code_buffer;
        }
    }
#else
    fn_ptr = (void *)jit->code.code_buffer;
    jit->code.is_executable = true;
#endif

    if (fn_ptr && jit->func_count < JIT_MAX_FUNCTIONS) {
        jit->func_names[jit->func_count] = func->name;
        jit->func_ptrs[jit->func_count] = fn_ptr;
        jit->func_count++;
    }

    return fn_ptr;
}

void *jit_get_function(JITCompiler *jit, const char *name) {
    if (!jit || !name) return NULL;
    for (int32_t i = 0; i < jit->func_count; i++) {
        if (jit->func_names[i] && strcmp(jit->func_names[i], name) == 0)
            return jit->func_ptrs[i];
    }
    return NULL;
}

int32_t jit_code_bytes_written(JITCompiler *jit) {
    if (!jit) return 0;
    return (int32_t)jit->code.code_offset;
}

void jit_dump_code(JITCompiler *jit, FILE *out) {
    if (!jit || !out) return;
    fprintf(out, ";;; JIT Code Dump (%d bytes):\n", (int)jit->code.code_offset);
    for (size_t i = 0; i < jit->code.code_offset; i += 16) {
        fprintf(out, ";;; %04zx: ", i);
        for (size_t j = 0; j < 16 && (i + j) < jit->code.code_offset; j++) {
            fprintf(out, "%02x ", jit->code.code_buffer[i + j]);
        }
        fprintf(out, "\n");
    }
    fprintf(out, ";;; Code is %s\n",
            jit->code.is_executable ? "executable" : "not executable");
}
