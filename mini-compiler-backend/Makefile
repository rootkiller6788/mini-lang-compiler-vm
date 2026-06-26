CC     = gcc
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS =
OUTDIR = bin

HDR = include/instruction_selection.h include/reg_alloc.h include/codegen.h       include/peephole.h include/abi_target.h include/ssa.h include/cfg.h       include/dataflow.h include/stackframe.h include/jit.h include/inst_sched.h

all: isel_demo regalloc_demo codegen_demo test

$(OUTDIR):
	@mkdir -p "$(OUTDIR)"

$(OUTDIR)/instruction_selection.o: src/instruction_selection.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/instruction_selection.c -o $(OUTDIR)/instruction_selection.o

$(OUTDIR)/reg_alloc.o: src/reg_alloc.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/reg_alloc.c -o $(OUTDIR)/reg_alloc.o

$(OUTDIR)/codegen.o: src/codegen.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/codegen.c -o $(OUTDIR)/codegen.o

$(OUTDIR)/peephole.o: src/peephole.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/peephole.c -o $(OUTDIR)/peephole.o

$(OUTDIR)/abi_target.o: src/abi_target.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/abi_target.c -o $(OUTDIR)/abi_target.o

$(OUTDIR)/ssa.o: src/ssa.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/ssa.c -o $(OUTDIR)/ssa.o

$(OUTDIR)/cfg.o: src/cfg.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/cfg.c -o $(OUTDIR)/cfg.o

$(OUTDIR)/dataflow.o: src/dataflow.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/dataflow.c -o $(OUTDIR)/dataflow.o

$(OUTDIR)/stackframe.o: src/stackframe.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/stackframe.c -o $(OUTDIR)/stackframe.o

$(OUTDIR)/jit.o: src/jit.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/jit.c -o $(OUTDIR)/jit.o

$(OUTDIR)/inst_sched.o: src/inst_sched.c $(HDR) | $(OUTDIR)
	$(CC) $(CFLAGS) -c src/inst_sched.c -o $(OUTDIR)/inst_sched.o

ALL_OBJ = $(OUTDIR)/instruction_selection.o $(OUTDIR)/reg_alloc.o           $(OUTDIR)/codegen.o $(OUTDIR)/peephole.o $(OUTDIR)/abi_target.o           $(OUTDIR)/ssa.o $(OUTDIR)/cfg.o $(OUTDIR)/dataflow.o           $(OUTDIR)/stackframe.o $(OUTDIR)/jit.o $(OUTDIR)/inst_sched.o

isel_demo: $(OUTDIR)/instruction_selection.o examples/isel_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) examples/isel_demo.c $(OUTDIR)/instruction_selection.o -o $(OUTDIR)/isel_demo $(LDFLAGS)

regalloc_demo: $(OUTDIR)/reg_alloc.o examples/regalloc_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) examples/regalloc_demo.c $(OUTDIR)/reg_alloc.o -o $(OUTDIR)/regalloc_demo $(LDFLAGS)

codegen_demo: $(OUTDIR)/instruction_selection.o $(OUTDIR)/reg_alloc.o $(OUTDIR)/codegen.o $(OUTDIR)/peephole.o $(OUTDIR)/abi_target.o examples/codegen_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) examples/codegen_demo.c $(OUTDIR)/instruction_selection.o $(OUTDIR)/reg_alloc.o $(OUTDIR)/codegen.o $(OUTDIR)/peephole.o $(OUTDIR)/abi_target.o -o $(OUTDIR)/codegen_demo $(LDFLAGS)

test: $(ALL_OBJ) tests/test_all_inline.c | $(OUTDIR)
	$(CC) $(CFLAGS) tests/test_all_inline.c $(ALL_OBJ) -o $(OUTDIR)/test_suite $(LDFLAGS)
	@echo "Running test suite..."
	$(OUTDIR)/test_suite

clean:
	@rm -rf "$(OUTDIR)"

.PHONY: all isel_demo regalloc_demo codegen_demo test clean
