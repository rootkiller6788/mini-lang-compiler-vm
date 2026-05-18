CC = gcc
CFLAGS = -Wall -Wextra -O2 -I include
LDFLAGS = -lm
OUTDIR = bin

SRCS = src/c_subset.c src/ml_like.c src/script_lang.c src/protobuf_lang.c src/regex_lang.c
OBJS = $(patsubst src/%.c,$(OUTDIR)/%.o,$(SRCS))

DEMOS = $(OUTDIR)/c_interp_demo $(OUTDIR)/ml_repl_demo $(OUTDIR)/proto_demo

.PHONY: all clean

all: $(OUTDIR) $(DEMOS)

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

clean:
	rm -rf "$(OUTDIR)"
