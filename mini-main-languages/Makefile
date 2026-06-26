CC = gcc
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS = -lm
OUTDIR = bin
TESTDIR = $(OUTDIR)/tests

SRCS = src/c_subset.c src/ml_like.c src/script_lang.c src/protobuf_lang.c src/regex_lang.c
OBJS = $(patsubst src/%.c,$(OUTDIR)/%.o,$(SRCS))

DEMOS = $(OUTDIR)/c_interp_demo $(OUTDIR)/ml_repl_demo $(OUTDIR)/proto_demo

TESTS = $(OUTDIR)/test_c_subset.exe $(OUTDIR)/test_ml_like.exe \
        $(OUTDIR)/test_script_lang.exe $(OUTDIR)/test_protobuf.exe $(OUTDIR)/test_regex.exe

.PHONY: all clean test demos

all: $(OUTDIR) $(DEMOS)

test: $(OUTDIR) $(TESTS)
	@echo "=== Running Tests ==="
	@cmd.exe /c "$(subst /,\,$(OUTDIR))\test_c_subset.exe"
	@cmd.exe /c "$(subst /,\,$(OUTDIR))\test_ml_like.exe"
	@cmd.exe /c "$(subst /,\,$(OUTDIR))\test_script_lang.exe"
	@cmd.exe /c "$(subst /,\,$(OUTDIR))\test_protobuf.exe"
	@cmd.exe /c "$(subst /,\,$(OUTDIR))\test_regex.exe"
	@echo "=== All Tests Passed ==="

test_no_cmd: $(OUTDIR) $(TESTS)
	@echo "=== Running Tests ==="
	@"$(OUTDIR)/test_c_subset"
	@"$(OUTDIR)/test_ml_like"
	@"$(OUTDIR)/test_script_lang"
	@"$(OUTDIR)/test_protobuf"
	@"$(OUTDIR)/test_regex"
	@echo "=== All Tests Passed ==="

demos: $(OUTDIR) $(DEMOS)

$(OUTDIR):
	mkdir -p "$(OUTDIR)"

$(OUTDIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/c_interp_demo: examples/c_interp_demo.c $(OUTDIR)/c_subset.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(OUTDIR)/ml_repl_demo: examples/ml_repl_demo.c $(OUTDIR)/ml_like.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(OUTDIR)/proto_demo: examples/proto_demo.c $(OUTDIR)/protobuf_lang.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Tests
$(OUTDIR)/test_c_subset.exe: tests/test_c_subset.c $(OUTDIR)/c_subset.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(OUTDIR)/test_ml_like.exe: tests/test_ml_like.c $(OUTDIR)/ml_like.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(OUTDIR)/test_script_lang.exe: tests/test_script_lang.c $(OUTDIR)/script_lang.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(OUTDIR)/test_protobuf.exe: tests/test_protobuf.c $(OUTDIR)/protobuf_lang.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(OUTDIR)/test_regex.exe: tests/test_regex.c $(OUTDIR)/regex_lang.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -rf "$(OUTDIR)"
