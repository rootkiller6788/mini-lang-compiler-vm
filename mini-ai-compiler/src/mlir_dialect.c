#include "mlir_dialect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MLIROp mlir_create_op(const char *dialect, const char *name, const char *location)
{
    MLIROp op;
    memset(&op, 0, sizeof(op));
    strncpy(op.dialect, dialect, MLIR_MAX_STR_LEN - 1);
    strncpy(op.name, name, MLIR_MAX_STR_LEN - 1);
    strncpy(op.location, location, MLIR_LOC_LEN - 1);
    op.operand_count = 0;
    op.result_count = 0;
    op.attr_count = 0;
    return op;
}

void mlir_op_add_operand(MLIROp *op, const char *name, MLIRType type)
{
    if (op->operand_count >= MLIR_MAX_OPERANDS) return;
    strncpy(op->operands[op->operand_count].name, name, MLIR_MAX_STR_LEN - 1);
    op->operands[op->operand_count].type = type;
    op->operand_count++;
}

void mlir_op_add_result(MLIROp *op, const char *name, MLIRType type)
{
    if (op->result_count >= MLIR_MAX_RESULTS) return;
    strncpy(op->results[op->result_count].name, name, MLIR_MAX_STR_LEN - 1);
    op->results[op->result_count].type = type;
    op->result_count++;
}

void mlir_op_add_attr_int(MLIROp *op, const char *key, long long value)
{
    if (op->attr_count >= MLIR_MAX_ATTRIBUTES) return;
    strncpy(op->attributes[op->attr_count].key, key, MLIR_MAX_STR_LEN - 1);
    op->attributes[op->attr_count].value.i = value;
    op->attributes[op->attr_count].value_kind = 0;
    op->attr_count++;
}

void mlir_op_add_attr_float(MLIROp *op, const char *key, double value)
{
    if (op->attr_count >= MLIR_MAX_ATTRIBUTES) return;
    strncpy(op->attributes[op->attr_count].key, key, MLIR_MAX_STR_LEN - 1);
    op->attributes[op->attr_count].value.f = value;
    op->attributes[op->attr_count].value_kind = 1;
    op->attr_count++;
}

void mlir_op_add_attr_str(MLIROp *op, const char *key, const char *value)
{
    if (op->attr_count >= MLIR_MAX_ATTRIBUTES) return;
    strncpy(op->attributes[op->attr_count].key, key, MLIR_MAX_STR_LEN - 1);
    strncpy(op->attributes[op->attr_count].value.s, value, MLIR_MAX_STR_LEN - 1);
    op->attributes[op->attr_count].value_kind = 2;
    op->attr_count++;
}

MLIRBlock *mlir_create_block(void)
{
    MLIRBlock *block = (MLIRBlock *)malloc(sizeof(MLIRBlock));
    if (!block) return NULL;
    memset(block, 0, sizeof(MLIRBlock));
    block->op_count = 0;
    block->arg_count = 0;
    block->next = NULL;
    return block;
}

void mlir_block_add_arg(MLIRBlock *block, const char *name, MLIRType type)
{
    if (block->arg_count >= MLIR_MAX_OPERANDS) return;
    strncpy(block->arguments[block->arg_count].name, name, MLIR_MAX_STR_LEN - 1);
    block->arguments[block->arg_count].type = type;
    block->arg_count++;
}

void mlir_block_add_op(MLIRBlock *block, MLIROp op)
{
    if (block->op_count >= MLIR_MAX_OPS) return;
    block->operations[block->op_count] = op;
    block->op_count++;
}

MLIRRegion mlir_create_region(void)
{
    MLIRRegion region;
    memset(&region, 0, sizeof(region));
    region.blocks = NULL;
    region.block_count = 0;
    return region;
}

void mlir_region_add_block(MLIRRegion *region, MLIRBlock *block)
{
    if (!region->blocks) {
        region->blocks = block;
    } else {
        MLIRBlock *cur = region->blocks;
        while (cur->next) cur = cur->next;
        cur->next = block;
    }
    region->block_count++;
}

bool mlir_verify(MLIROp *op)
{
    int i;
    if (strlen(op->dialect) == 0 || strlen(op->name) == 0) return false;

    for (i = 0; i < op->operand_count; i++) {
        if (op->operands[i].type == MLIR_TYPE_NONE) return false;
    }
    for (i = 0; i < op->result_count; i++) {
        if (op->results[i].type == MLIR_TYPE_NONE) return false;
    }
    return true;
}

const char *mlir_type_name(MLIRType type)
{
    switch (type) {
    case MLIR_TYPE_I8:   return "i8";
    case MLIR_TYPE_I16:  return "i16";
    case MLIR_TYPE_I32:  return "i32";
    case MLIR_TYPE_I64:  return "i64";
    case MLIR_TYPE_F32:  return "f32";
    case MLIR_TYPE_F64:  return "f64";
    case MLIR_TYPE_NONE: return "none";
    case MLIR_TYPE_MEMREF_I32: return "memref<?xi32>";
    case MLIR_TYPE_MEMREF_F32: return "memref<?xf32>";
    default: return "unknown";
    }
}

static void mlir_print_value(MLIRValue *v)
{
    printf("%%s : %s", v->name, mlir_type_name(v->type));
}

static void mlir_print_attr(MLIRAttribute *a)
{
    printf("{%s=", a->key);
    if (a->value_kind == 0) {
        printf("%lld", a->value.i);
    } else if (a->value_kind == 1) {
        printf("%g", a->value.f);
    } else {
        printf("\"%s\"", a->value.s);
    }
    printf("}");
}

void mlir_print_op(MLIROp *op)
{
    int i;
    if (op->result_count > 0) {
        printf("  %s = ", op->results[0].name);
    }
    printf("%s.%s", op->dialect, op->name);
    for (i = 0; i < op->operand_count; i++) {
        printf(" %s", op->operands[i].name);
    }
    if (op->attr_count > 0) {
        printf(" {");
        for (i = 0; i < op->attr_count; i++) {
            if (i > 0) printf(", ");
            mlir_print_attr(&op->attributes[i]);
        }
        printf("}");
    }
    printf(" : ");
    if (op->operand_count > 0) {
        for (i = 0; i < op->operand_count; i++) {
            if (i > 0) printf(", ");
            printf("%s", mlir_type_name(op->operands[i].type));
        }
    }
    printf(" -> ");
    for (i = 0; i < op->result_count; i++) {
        if (i > 0) printf(", ");
        printf("%s", mlir_type_name(op->results[i].type));
    }
    printf(" loc(\"%s\")\n", op->location);
}

void mlir_print_ir(MLIRRegion *region)
{
    MLIRBlock *block = region->blocks;
    printf("module {\n");
    while (block) {
        printf("  func.func @main(");
        int i;
        for (i = 0; i < block->arg_count; i++) {
            if (i > 0) printf(", ");
            printf("%%%s: %s", block->arguments[i].name,
                   mlir_type_name(block->arguments[i].type));
        }
        printf(") {\n");
        for (i = 0; i < block->op_count; i++) {
            mlir_print_op(&block->operations[i]);
        }
        printf("    func.return\n");
        printf("  }\n");
        block = block->next;
    }
    printf("}\n");
}

MLIRContext mlir_context_create(void)
{
    MLIRContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.builtin.name, "builtin", MLIR_MAX_STR_LEN - 1);
    strncpy(ctx.arith.name, "arith", MLIR_MAX_STR_LEN - 1);
    strncpy(ctx.memref.name, "memref", MLIR_MAX_STR_LEN - 1);
    strncpy(ctx.func.name, "func", MLIR_MAX_STR_LEN - 1);
    return ctx;
}

void mlir_context_register_arith_ops(MLIRContext *ctx)
{
    ctx->arith.op_count = 2;
    ctx->arith.ops[0] = mlir_create_op("arith", "addi", "arith");
    ctx->arith.ops[1] = mlir_create_op("arith", "muli", "arith");
    mlir_op_add_result(&ctx->arith.ops[0], "result", MLIR_TYPE_I32);
    mlir_op_add_result(&ctx->arith.ops[1], "result", MLIR_TYPE_I32);
}

MLIROp mlir_arith_addi(const char *location)
{
    MLIROp op = mlir_create_op("arith", "addi", location);
    mlir_op_add_result(&op, "sum", MLIR_TYPE_I32);
    return op;
}

MLIROp mlir_arith_muli(const char *location)
{
    MLIROp op = mlir_create_op("arith", "muli", location);
    mlir_op_add_result(&op, "prod", MLIR_TYPE_I32);
    return op;
}

MLIROp mlir_memref_alloc(const char *location)
{
    MLIROp op = mlir_create_op("memref", "alloc", location);
    mlir_op_add_result(&op, "mem", MLIR_TYPE_MEMREF_F32);
    return op;
}

MLIROp mlir_memref_load(const char *location)
{
    MLIROp op = mlir_create_op("memref", "load", location);
    mlir_op_add_result(&op, "val", MLIR_TYPE_F32);
    return op;
}

MLIROp mlir_memref_store(const char *location)
{
    MLIROp op = mlir_create_op("memref", "store", location);
    return op;
}

MLIROp mlir_func_func(const char *name, const char *location)
{
    MLIROp op = mlir_create_op("func", "func", location);
    mlir_op_add_attr_str(&op, "sym_name", name);
    return op;
}

MLIROp mlir_func_return(const char *location)
{
    MLIROp op = mlir_create_op("func", "return", location);
    return op;
}

MLIROp mlir_func_call(const char *callee, const char *location)
{
    MLIROp op = mlir_create_op("func", "call", location);
    mlir_op_add_attr_str(&op, "callee", callee);
    return op;
}
