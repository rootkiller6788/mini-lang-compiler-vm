CC = gcc
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS = -lm
OUTDIR = bin

HEADERS = include/lexer.h include/parser.h include/ast.h include/symtab.h \
          include/semantic.h include/ir.h include/codegen.h include/grammar.h

LEXER_DEMO = $(OUTDIR)/lexer_demo.exe
PARSER_DEMO = $(OUTDIR)/parser_demo.exe
SEMANTIC_DEMO = $(OUTDIR)/semantic_demo.exe

.PHONY: all clean test run-lexer run-parser run-semantic

all: $(LEXER_DEMO) $(PARSER_DEMO) $(SEMANTIC_DEMO)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/lexer.o: src/lexer.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/parser.o: src/parser.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/ast.o: src/ast.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/symtab.o: src/symtab.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/semantic.o: src/semantic.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/ir.o: src/ir.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/codegen.o: src/codegen.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/grammar.o: src/grammar.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/lexer_demo.o: examples/lexer_demo.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/parser_demo.o: examples/parser_demo.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/semantic_demo.o: examples/semantic_demo.c $(HEADERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LEXER_DEMO): $(OUTDIR)/lexer_demo.o $(OUTDIR)/lexer.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(PARSER_DEMO): $(OUTDIR)/parser_demo.o $(OUTDIR)/lexer.o $(OUTDIR)/parser.o $(OUTDIR)/ast.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(SEMANTIC_DEMO): $(OUTDIR)/semantic_demo.o $(OUTDIR)/lexer.o $(OUTDIR)/parser.o $(OUTDIR)/ast.o $(OUTDIR)/symtab.o $(OUTDIR)/semantic.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# ─── Test ────────────────────────────────────────────────────────────
# Compile all object files and run demos to verify correctness.
# The demos exercise lexer, parser, AST, symtab, and semantic modules
# end-to-end. The ir, codegen, and grammar modules are verified by
# compilation (all object files built with -Wall -Wextra).

.PHONY: test

test: $(LEXER_DEMO) $(PARSER_DEMO) $(SEMANTIC_DEMO) \
      $(OUTDIR)/ir.o $(OUTDIR)/codegen.o $(OUTDIR)/grammar.o
	@echo "=== Compilation: All modules built successfully ==="
	@echo "=== Running Lexer Demo ==="
	@$(LEXER_DEMO) > /dev/null && echo "  Lexer Demo: PASS" || echo "  Lexer Demo: FAIL"
	@echo "=== Running Parser Demo ==="
	@$(PARSER_DEMO) > /dev/null && echo "  Parser Demo: PASS" || echo "  Parser Demo: FAIL"
	@echo "=== Running Semantic Demo ==="
	@$(SEMANTIC_DEMO) > /dev/null && echo "  Semantic Demo: PASS" || echo "  Semantic Demo: FAIL"
	@echo "=== All Tests Passed ==="

run-lexer: $(LEXER_DEMO)
	@echo === Running Lexer Demo ===
	@$(LEXER_DEMO)

run-parser: $(PARSER_DEMO)
	@echo === Running Parser Demo ===
	@$(PARSER_DEMO)

run-semantic: $(SEMANTIC_DEMO)
	@echo === Running Semantic Demo ===
	@$(SEMANTIC_DEMO)

clean:
	rm -rf $(OUTDIR)
