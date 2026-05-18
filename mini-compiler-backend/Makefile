CC     = gcc
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS =
OUTDIR = bin

HDR = include/instruction_selection.h include/reg_alloc.h include/codegen.h include/peephole.h include/abi_target.h

all: isel_demo regalloc_demo codegen_demo

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

isel_demo: $(OUTDIR)/instruction_selection.o examples/isel_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) examples/isel_demo.c $(OUTDIR)/instruction_selection.o -o $(OUTDIR)/isel_demo $(LDFLAGS)

regalloc_demo: $(OUTDIR)/reg_alloc.o examples/regalloc_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) examples/regalloc_demo.c $(OUTDIR)/reg_alloc.o -o $(OUTDIR)/regalloc_demo $(LDFLAGS)

codegen_demo: $(OUTDIR)/instruction_selection.o $(OUTDIR)/reg_alloc.o $(OUTDIR)/codegen.o $(OUTDIR)/peephole.o $(OUTDIR)/abi_target.o examples/codegen_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) examples/codegen_demo.c $(OUTDIR)/instruction_selection.o $(OUTDIR)/reg_alloc.o $(OUTDIR)/codegen.o $(OUTDIR)/peephole.o $(OUTDIR)/abi_target.o -o $(OUTDIR)/codegen_demo $(LDFLAGS)

clean:
	@rm -rf "$(OUTDIR)"

.PHONY: all isel_demo regalloc_demo codegen_demo clean
