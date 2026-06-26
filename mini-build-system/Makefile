CC = clang
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS =
OUTDIR = bin

.PHONY: all clean test demo1 demo2 demo3

# All examples + test
all: $(OUTDIR)/make_parse_demo.exe $(OUTDIR)/ninja_demo.exe $(OUTDIR)/dep_solver_demo.exe $(OUTDIR)/test_suite.exe

$(OUTDIR):
	mkdir -p $(OUTDIR)

# Shared source objects
SRC = src/dep_graph.c src/incremental.c src/make_engine.c \
      src/ninja_graph.c src/task_scheduler.c \
      src/build_toolchain.c src/build_manifest.c src/build_logger.c \
      src/hash_cache.c src/scheduler_advanced.c src/build_theorem.c

$(OUTDIR)/make_parse_demo.exe: src/make_engine.c examples/make_parse_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) src/make_engine.c examples/make_parse_demo.c -o $@ $(LDFLAGS)

$(OUTDIR)/ninja_demo.exe: src/ninja_graph.c examples/ninja_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) src/ninja_graph.c examples/ninja_demo.c -o $@ $(LDFLAGS)

$(OUTDIR)/dep_solver_demo.exe: src/dep_graph.c examples/dep_solver_demo.c | $(OUTDIR)
	$(CC) $(CFLAGS) src/dep_graph.c examples/dep_solver_demo.c -o $@ $(LDFLAGS)

# Test target - compiles and runs all tests
$(OUTDIR)/test_suite.exe: tests/test_suite.c $(SRC) | $(OUTDIR)
	$(CC) $(CFLAGS) tests/test_suite.c $(SRC) -o $@ $(LDFLAGS)

test: $(OUTDIR)/test_suite.exe
	@echo "Running test suite..."
	$(OUTDIR)/test_suite.exe

demo1: $(OUTDIR)/make_parse_demo.exe
	$(OUTDIR)/make_parse_demo.exe

demo2: $(OUTDIR)/ninja_demo.exe
	$(OUTDIR)/ninja_demo.exe

demo3: $(OUTDIR)/dep_solver_demo.exe
	$(OUTDIR)/dep_solver_demo.exe

clean:
	rm -rf $(OUTDIR)
