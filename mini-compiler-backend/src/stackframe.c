#include "stackframe.h"

void stackframe_init(StackFrame *sf, int32_t alignment) {
    memset(sf, 0, sizeof(StackFrame));
    sf->stack_alignment = (alignment >= 4) ? alignment : 16;
    sf->total_size = 0;
    sf->frame_pointer_offset = 0;
    sf->setup_complete = false;
}

int32_t stackframe_alloc_local(StackFrame *sf, const char *name,
                                int32_t size, int32_t alignment) {
    if (sf->slot_count >= MAX_LOCALS) return -1;
    StackSlot *s = &sf->slots[sf->slot_count++];
    memset(s, 0, sizeof(StackSlot));
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->size = size;
    s->alignment = (alignment >= 1) ? alignment : 8;
    s->offset = 0;
    s->is_spill = false;
    s->is_arg = false;
    return sf->slot_count - 1;
}

int32_t stackframe_alloc_spill(StackFrame *sf, int32_t slot_id) {
    if (sf->slot_count >= MAX_LOCALS) return -1;
    StackSlot *s = &sf->slots[sf->slot_count++];
    memset(s, 0, sizeof(StackSlot));
    snprintf(s->name, sizeof(s->name), "spill_%d", slot_id);
    s->size = 8;
    s->alignment = 8;
    s->offset = 0;
    s->is_spill = true;
    return sf->slot_count - 1;
}

int32_t stackframe_alloc_arg(StackFrame *sf, const char *name,
                              int32_t size, int32_t arg_index) {
    if (sf->slot_count >= MAX_LOCALS) return -1;
    StackSlot *s = &sf->slots[sf->slot_count++];
    memset(s, 0, sizeof(StackSlot));
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->size = size;
    s->alignment = 8;
    s->offset = 0;
    s->is_spill = false;
    s->is_arg = true;
    s->arg_index = arg_index;
    return sf->slot_count - 1;
}

/*
 * Stack frame layout computation.
 *
 * The stack frame layout follows the target ABI conventions:
 *
 *   High addresses (toward caller's frame)
 *   +--------------------------+
 *   | Caller's arguments (>6)  |  args_area
 *   +--------------------------+
 *   | Return address           |  (pushed by CALL)
 *   +--------------------------+
 *   | Old frame pointer (rbp)  |  (pushed in prologue)
 *   +--------------------------+ <-- rbp (frame pointer)
 *   | Saved callee registers   |  saved_regs_area
 *   +--------------------------+
 *   | Local variables          |  locals_area
 *   +--------------------------+
 *   | Spill slots              |  spill_area
 *   +--------------------------+
 *   Low addresses (toward stack limit)
 *
 * Alignment: x86-64 SysV requires 16-byte stack alignment at CALL site.
 * ARM64 AAPCS also requires 16-byte alignment.
 */
void stackframe_layout(StackFrame *sf, FrameLayoutType layout) {
    int32_t offset = 0;
    int32_t align = sf->stack_alignment;

    sf->saved_regs_area_size = 0;
    sf->locals_area_size = 0;
    sf->spill_area_size = 0;
    sf->args_area_size = 0;

    for (int32_t i = 0; i < sf->slot_count; i++) {
        StackSlot *s = &sf->slots[i];
        if (s->is_spill) {
            sf->spill_area_size += s->size;
            if (sf->spill_area_size % s->alignment != 0)
                sf->spill_area_size += s->alignment -
                    (sf->spill_area_size % s->alignment);
        } else if (s->is_arg) {
            sf->args_area_size += s->size;
        } else {
            sf->locals_area_size += s->size;
            if (sf->locals_area_size % s->alignment != 0)
                sf->locals_area_size += s->alignment -
                    (sf->locals_area_size % s->alignment);
        }
    }

    sf->total_size = sf->saved_regs_area_size + sf->locals_area_size +
                     sf->spill_area_size + sf->args_area_size;

    if (layout == FRAME_LAYOUT_X86) {
        sf->total_size += 16;
        sf->frame_pointer_offset = 8;
    } else if (layout == FRAME_LAYOUT_ARM) {
        sf->total_size += 16;
        sf->frame_pointer_offset = 0;
    } else {
        sf->total_size += 32;
        sf->frame_pointer_offset = 0;
    }

    if (sf->total_size % align != 0)
        sf->total_size += align - (sf->total_size % align);

    offset = -sf->locals_area_size;
    for (int32_t i = 0; i < sf->slot_count; i++) {
        StackSlot *s = &sf->slots[i];
        if (!s->is_spill && !s->is_arg) {
            s->offset = offset;
            offset += s->size;
            if (offset % s->alignment != 0)
                offset += s->alignment - (offset % s->alignment);
        }
    }

    int32_t spill_off = offset - sf->spill_area_size;
    for (int32_t i = 0; i < sf->slot_count; i++) {
        StackSlot *s = &sf->slots[i];
        if (s->is_spill) {
            s->offset = spill_off;
            spill_off += s->size;
        }
    }

    int32_t arg_off = sf->frame_pointer_offset + 16;
    for (int32_t i = 0; i < sf->slot_count; i++) {
        StackSlot *s = &sf->slots[i];
        if (s->is_arg) {
            s->offset = arg_off;
            arg_off += s->size;
        }
    }

    sf->setup_complete = true;
}

int32_t stackframe_get_offset(StackFrame *sf, const char *name) {
    if (!sf || !name) return 0;
    for (int32_t i = 0; i < sf->slot_count; i++) {
        if (strcmp(sf->slots[i].name, name) == 0)
            return sf->slots[i].offset;
    }
    return 0;
}

void stackframe_emit_prologue(StackFrame *sf, FrameLayoutType layout, FILE *out) {
    if (!sf || !out) return;
    if (!sf->setup_complete) stackframe_layout(sf, layout);

    switch (layout) {
        case FRAME_LAYOUT_X86:
            fprintf(out, "  push rbp\n");
            fprintf(out, "  mov  rbp, rsp\n");
            if (sf->total_size > 0)
                fprintf(out, "  sub  rsp, %d\n", sf->total_size);
            break;
        case FRAME_LAYOUT_ARM:
            fprintf(out, "  stp  fp, lr, [sp, #-16]!\n");
            fprintf(out, "  mov  fp, sp\n");
            if (sf->total_size > 0)
                fprintf(out, "  sub  sp, sp, #%d\n", sf->total_size);
            break;
        case FRAME_LAYOUT_RISCV:
            fprintf(out, "  addi sp, sp, -%d\n", sf->total_size);
            fprintf(out, "  sd   ra, %d(sp)\n", sf->total_size - 8);
            fprintf(out, "  sd   fp, %d(sp)\n", sf->total_size - 16);
            fprintf(out, "  addi fp, sp, %d\n", sf->total_size);
            break;
    }
}

void stackframe_emit_epilogue(StackFrame *sf, FrameLayoutType layout, FILE *out) {
    if (!sf || !out) return;

    switch (layout) {
        case FRAME_LAYOUT_X86:
            fprintf(out, "  leave\n");
            fprintf(out, "  ret\n");
            break;
        case FRAME_LAYOUT_ARM:
            if (sf->total_size > 0)
                fprintf(out, "  add  sp, sp, #%d\n", sf->total_size);
            fprintf(out, "  ldp  fp, lr, [sp], #16\n");
            fprintf(out, "  ret\n");
            break;
        case FRAME_LAYOUT_RISCV:
            fprintf(out, "  ld   ra, %d(sp)\n", sf->total_size - 8);
            fprintf(out, "  ld   fp, %d(sp)\n", sf->total_size - 16);
            fprintf(out, "  addi sp, sp, %d\n", sf->total_size);
            fprintf(out, "  ret\n");
            break;
    }
}

void stackframe_dump(StackFrame *sf, FILE *out) {
    if (!sf || !out) return;
    fprintf(out, ";;; Stack Frame Layout:\n");
    fprintf(out, ";;;   Total size:       %d bytes\n", sf->total_size);
    fprintf(out, ";;;   Alignment:        %d bytes\n", sf->stack_alignment);
    fprintf(out, ";;;   Frame ptr offset: %d\n", sf->frame_pointer_offset);
    fprintf(out, ";;;   Locals area:      %d bytes\n", sf->locals_area_size);
    fprintf(out, ";;;   Spill area:       %d bytes\n", sf->spill_area_size);
    fprintf(out, ";;;   Args area:        %d bytes\n", sf->args_area_size);
    fprintf(out, ";;;   Saved regs area:  %d bytes\n", sf->saved_regs_area_size);
    fprintf(out, ";;;   Slots (%d):\n", sf->slot_count);
    for (int32_t i = 0; i < sf->slot_count; i++) {
        StackSlot *s = &sf->slots[i];
        fprintf(out, ";;;     [%2d] %-16s size=%-4d align=%-2d offset=%d",
                i, s->name, s->size, s->alignment, s->offset);
        if (s->is_spill) fprintf(out, " [SPILL]");
        if (s->is_arg) fprintf(out, " [ARG #%d]", s->arg_index);
        fprintf(out, "\n");
    }
}
