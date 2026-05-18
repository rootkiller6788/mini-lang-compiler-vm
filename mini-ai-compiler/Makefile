CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -I include
LDFLAGS  = -lm
OUTDIR   = bin

SRC      = src/mlir_dialect.c src/graph_ir.c src/op_fusion.c src/layout_opt.c src/auto_schedule.c
OBJ      = src/mlir_dialect.o src/graph_ir.o src/op_fusion.o src/layout_opt.o src/auto_schedule.o

EXAMPLES = $(OUTDIR)/mlir_demo $(OUTDIR)/graph_fusion_demo $(OUTDIR)/autotune_demo

.PHONY: all clean

all: $(OUTDIR) $(EXAMPLES)

$(OUTDIR):
	mkdir -p $(OUTDIR)

src/mlir_dialect.o: src/mlir_dialect.c
	$(CC) $(CFLAGS) -c src/mlir_dialect.c -o src/mlir_dialect.o

src/graph_ir.o: src/graph_ir.c
	$(CC) $(CFLAGS) -c src/graph_ir.c -o src/graph_ir.o

src/op_fusion.o: src/op_fusion.c
	$(CC) $(CFLAGS) -c src/op_fusion.c -o src/op_fusion.o

src/layout_opt.o: src/layout_opt.c
	$(CC) $(CFLAGS) -c src/layout_opt.c -o src/layout_opt.o

src/auto_schedule.o: src/auto_schedule.c
	$(CC) $(CFLAGS) -c src/auto_schedule.c -o src/auto_schedule.o

$(OUTDIR)/mlir_demo: examples/mlir_demo.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ examples/mlir_demo.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/graph_fusion_demo: examples/graph_fusion_demo.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ examples/graph_fusion_demo.c $(OBJ) $(LDFLAGS)

$(OUTDIR)/autotune_demo: examples/autotune_demo.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ examples/autotune_demo.c $(OBJ) $(LDFLAGS)

clean:
	rm -rf $(OUTDIR) src/*.o
