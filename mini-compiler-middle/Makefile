CC = gcc
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS = -lm
OUTDIR = bin
SRCDIR = src
EXAMPLEDIR = examples

_DEPS = ir.h ssa.h dataflow.h optimizer.h cfg.h
_OBJS = ir.o ssa.o dataflow.o optimizer.o cfg.o
_EXAMPLES = ir_demo ssa_demo opt_demo

DEPS = $(patsubst %,include/%,$(_DEPS))
OBJS = $(patsubst %,$(SRCDIR)/%,$(_OBJS))
TARGETS = $(patsubst %,$(OUTDIR)/%,$(_EXAMPLES))

.PHONY: all clean examples

all: $(OUTDIR) $(TARGETS)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/ir_demo: $(SRCDIR)/ir.o $(SRCDIR)/cfg.o $(EXAMPLEDIR)/ir_demo.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(EXAMPLEDIR)/ir_demo.c $(SRCDIR)/ir.o $(SRCDIR)/cfg.o $(LDFLAGS)

$(OUTDIR)/ssa_demo: $(SRCDIR)/ir.o $(SRCDIR)/ssa.o $(SRCDIR)/cfg.o $(EXAMPLEDIR)/ssa_demo.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(EXAMPLEDIR)/ssa_demo.c $(SRCDIR)/ir.o $(SRCDIR)/ssa.o $(SRCDIR)/cfg.o $(LDFLAGS)

$(OUTDIR)/opt_demo: $(SRCDIR)/ir.o $(SRCDIR)/optimizer.o $(SRCDIR)/cfg.o $(EXAMPLEDIR)/opt_demo.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(EXAMPLEDIR)/opt_demo.c $(SRCDIR)/ir.o $(SRCDIR)/optimizer.o $(SRCDIR)/cfg.o $(LDFLAGS)

$(SRCDIR)/ir.o: $(SRCDIR)/ir.c include/ir.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/ir.c

$(SRCDIR)/ssa.o: $(SRCDIR)/ssa.c include/ssa.h include/ir.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/ssa.c

$(SRCDIR)/dataflow.o: $(SRCDIR)/dataflow.c include/dataflow.h include/ir.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/dataflow.c

$(SRCDIR)/optimizer.o: $(SRCDIR)/optimizer.c include/optimizer.h include/ir.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/optimizer.c

$(SRCDIR)/cfg.o: $(SRCDIR)/cfg.c include/cfg.h include/ir.h
	$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/cfg.c

clean:
	rm -f $(OUTDIR)/*
	rm -f $(SRCDIR)/*.o
