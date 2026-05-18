#ifndef MLIR_DIALECT_H
#define MLIR_DIALECT_H

#include <stdbool.h>
#include <stddef.h>

#define MLIR_MAX_OPERANDS    8
#define MLIR_MAX_RESULTS     4
#define MLIR_MAX_ATTRIBUTES  8
#define MLIR_MAX_BLOCKS      16
#define MLIR_MAX_OPS         64
#define MLIR_MAX_STR_LEN     128
#define MLIR_LOC_LEN         64

typedef enum {
    MLIR_TYPE_I8,
    MLIR_TYPE_I16,
    MLIR_TYPE_I32,
    MLIR_TYPE_I64,
    MLIR_TYPE_F32,
    MLIR_TYPE_F64,
    MLIR_TYPE_NONE,
    MLIR_TYPE_MEMREF_I32,
    MLIR_TYPE_MEMREF_F32,
    MLIR_TYPE_COUNT
} MLIRType;

typedef struct {
    char name[MLIR_MAX_STR_LEN];
    MLIRType type;
} MLIRValue;

typedef struct {
    char key[MLIR_MAX_STR_LEN];
    union {
        long long i;
        double f;
        char s[MLIR_MAX_STR_LEN];
    } value;
    int value_kind; /* 0=int, 1=float, 2=string */
} MLIRAttribute;

typedef struct {
    char name[MLIR_MAX_STR_LEN];
    MLIRValue operands[MLIR_MAX_OPERANDS];
    int operand_count;
    MLIRValue results[MLIR_MAX_RESULTS];
    int result_count;
    MLIRAttribute attributes[MLIR_MAX_ATTRIBUTES];
    int attr_count;
    char location[MLIR_LOC_LEN];
    char dialect[MLIR_MAX_STR_LEN];
} MLIROp;

typedef struct MLIRBlock {
    MLIROp operations[MLIR_MAX_OPS];
    int op_count;
    MLIRValue arguments[MLIR_MAX_OPERANDS];
    int arg_count;
    struct MLIRBlock *next;
} MLIRBlock;

typedef struct {
    MLIRBlock *blocks;
    int block_count;
} MLIRRegion;

typedef struct {
    char name[MLIR_MAX_STR_LEN];
    MLIROp ops[MLIR_MAX_OPS];
    int op_count;
} MLIRDialect;

MLIROp mlir_create_op(const char *dialect, const char *name, const char *location);
void mlir_op_add_operand(MLIROp *op, const char *name, MLIRType type);
void mlir_op_add_result(MLIROp *op, const char *name, MLIRType type);
void mlir_op_add_attr_int(MLIROp *op, const char *key, long long value);
void mlir_op_add_attr_float(MLIROp *op, const char *key, double value);
void mlir_op_add_attr_str(MLIROp *op, const char *key, const char *value);

MLIRBlock *mlir_create_block(void);
void mlir_block_add_arg(MLIRBlock *block, const char *name, MLIRType type);
void mlir_block_add_op(MLIRBlock *block, MLIROp op);

MLIRRegion mlir_create_region(void);
void mlir_region_add_block(MLIRRegion *region, MLIRBlock *block);

bool mlir_verify(MLIROp *op);
const char *mlir_type_name(MLIRType type);
void mlir_print_ir(MLIRRegion *region);
void mlir_print_op(MLIROp *op);

typedef struct {
    MLIRDialect builtin;
    MLIRDialect arith;
    MLIRDialect memref;
    MLIRDialect func;
} MLIRContext;

MLIRContext mlir_context_create(void);
void mlir_context_register_arith_ops(MLIRContext *ctx);
MLIROp mlir_arith_addi(const char *location);
MLIROp mlir_arith_muli(const char *location);

MLIROp mlir_memref_alloc(const char *location);
MLIROp mlir_memref_load(const char *location);
MLIROp mlir_memref_store(const char *location);

MLIROp mlir_func_func(const char *name, const char *location);
MLIROp mlir_func_return(const char *location);
MLIROp mlir_func_call(const char *callee, const char *location);

#endif
