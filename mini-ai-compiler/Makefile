CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -I include
LDFLAGS  = -lm
OUTDIR   = bin

SRC      = src/mlir_dialect.c src/graph_ir.c src/op_fusion.c \
           src/layout_opt.c src/auto_schedule.c \
           src/ir_passes.c src/codegen.c src/quantize.c \
           src/type_infer.c src/tensor_expr.c
OBJ      = $(SRC:.c=.o)

EXAMPLES = $(OUTDIR)/mlir_demo $(OUTDIR)/graph_fusion_demo $(OUTDIR)/autotune_demo
TESTS    = $(OUTDIR)/test_ir_passes $(OUTDIR)/test_quantize \
           $(OUTDIR)/test_type_infer $(OUTDIR)/test_tensor_expr \
           $(OUTDIR)/test_codegen

.PHONY: all clean test

all: $(OUTDIR) $(EXAMPLES)

test: $(OUTDIR) $(TESTS)
	@echo "Running tests..."
	@for t in $(TESTS); do echo "  $$t"; $$t || exit 1; done
	@echo "All tests passed!"

$(OUTDIR):
	mkdir -p $(OUTDIR)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/mlir_demo: examples/mlir_demo.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ examples/mlir_demo.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/graph_fusion_demo: examples/graph_fusion_demo.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ examples/graph_fusion_demo.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/autotune_demo: examples/autotune_demo.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ examples/autotune_demo.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/test_ir_passes: tests/test_ir_passes.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ tests/test_ir_passes.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/test_quantize: tests/test_quantize.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ tests/test_quantize.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/test_type_infer: tests/test_type_infer.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ tests/test_type_infer.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/test_tensor_expr: tests/test_tensor_expr.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ tests/test_tensor_expr.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/test_codegen: tests/test_codegen.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ tests/test_codegen.c $(OBJ) $(LDFLAGS)

clean:
	rm -rf $(OUTDIR) src/*.o
