CC = gcc
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS = -lm
OUTDIR = bin

HEADERS = include/lexer.h include/parser.h include/ast.h include/symtab.h include/semantic.h

LEXER_DEMO = $(OUTDIR)/lexer_demo
PARSER_DEMO = $(OUTDIR)/parser_demo
SEMANTIC_DEMO = $(OUTDIR)/semantic_demo

.PHONY: all clean run-lexer run-parser run-semantic

all: $(LEXER_DEMO) $(PARSER_DEMO) $(SEMANTIC_DEMO)

$(OUTDIR):
	if not exist $(OUTDIR) mkdir $(OUTDIR)

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
	@if exist $(OUTDIR) rmdir /s /q $(OUTDIR)
