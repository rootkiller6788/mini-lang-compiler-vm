CC = clang
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS =
OUTDIR = bin

.PHONY: all clean demo1 demo2 demo3

all: $(OUTDIR)/make_parse_demo.exe $(OUTDIR)/ninja_demo.exe $(OUTDIR)/dep_solver_demo.exe

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/make_parse_demo.exe: src/make_engine.c examples/make_parse_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) src/make_engine.c examples/make_parse_demo.c -o $@ $(LDFLAGS)

$(OUTDIR)/ninja_demo.exe: src/ninja_graph.c examples/ninja_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) src/ninja_graph.c examples/ninja_demo.c -o $@ $(LDFLAGS)

$(OUTDIR)/dep_solver_demo.exe: src/dep_graph.c examples/dep_solver_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) src/dep_graph.c examples/dep_solver_demo.c -o $@ $(LDFLAGS)

demo1: $(OUTDIR)/make_parse_demo.exe
	$(OUTDIR)/make_parse_demo.exe

demo2: $(OUTDIR)/ninja_demo.exe
	$(OUTDIR)/ninja_demo.exe

demo3: $(OUTDIR)/dep_solver_demo.exe
	$(OUTDIR)/dep_solver_demo.exe

clean:
	rm -rf $(OUTDIR)
