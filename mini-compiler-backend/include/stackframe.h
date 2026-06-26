#ifndef STACKFRAME_H
#define STACKFRAME_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOCALS        128
#define MAX_SPILL_SLOTS   64
#define MAX_CALLEE_REGS   16

typedef struct {
    char name[32];
    int32_t size;
    int32_t alignment;
    int32_t offset;
    bool is_spill;
    bool is_arg;
    int32_t arg_index;
} StackSlot;

typedef struct {
    int32_t total_size;
    int32_t stack_alignment;
    int32_t frame_pointer_offset;
    int32_t args_area_size;
    int32_t locals_area_size;
    int32_t spill_area_size;
    int32_t saved_regs_area_size;
    StackSlot slots[MAX_LOCALS];
    int32_t slot_count;
    bool setup_complete;
} StackFrame;

typedef enum {
    FRAME_LAYOUT_X86,
    FRAME_LAYOUT_ARM,
    FRAME_LAYOUT_RISCV
} FrameLayoutType;

void stackframe_init(StackFrame *sf, int32_t alignment);
int32_t stackframe_alloc_local(StackFrame *sf, const char *name,
                                int32_t size, int32_t alignment);
int32_t stackframe_alloc_spill(StackFrame *sf, int32_t slot_id);
int32_t stackframe_alloc_arg(StackFrame *sf, const char *name,
                              int32_t size, int32_t arg_index);
void stackframe_layout(StackFrame *sf, FrameLayoutType layout);
int32_t stackframe_get_offset(StackFrame *sf, const char *name);
void stackframe_emit_prologue(StackFrame *sf, FrameLayoutType layout, FILE *out);
void stackframe_emit_epilogue(StackFrame *sf, FrameLayoutType layout, FILE *out);
void stackframe_dump(StackFrame *sf, FILE *out);

#endif
