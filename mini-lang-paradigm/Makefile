CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -I include
LDFLAGS  = -lm
OUTDIR   = bin
MKDIR    = mkdir -p $(OUTDIR)

SRC_OOP       = src/oop_vtable.c
SRC_FP        = src/fp_closure.c
SRC_LOGIC     = src/logic_unify.c
SRC_TYPE      = src/type_system.c
SRC_PATTERN   = src/pattern_match.c
SRC_LAMBDA    = src/lambda_calc.c
SRC_CONT      = src/continuation.c
SRC_GENERIC   = src/generic_prog.c

ALL_SRC       = $(SRC_FP) $(SRC_OOP) $(SRC_LOGIC) $(SRC_TYPE) $(SRC_PATTERN) \
                $(SRC_LAMBDA) $(SRC_CONT) $(SRC_GENERIC)

EXAMPLE_OOP   = examples/oop_demo.c
EXAMPLE_FP    = examples/fp_demo.c
EXAMPLE_TYPE  = examples/type_infer_demo.c
TEST_MAIN     = tests/test_all.c

TARGET_OOP    = $(OUTDIR)/oop_demo
TARGET_FP     = $(OUTDIR)/fp_demo
TARGET_TYPE   = $(OUTDIR)/type_infer_demo
TARGET_TEST   = $(OUTDIR)/test_all

.PHONY: all clean test oop_demo fp_demo type_infer_demo

all: oop_demo fp_demo type_infer_demo

oop_demo:
	$(MKDIR)
	$(CC) $(CFLAGS) $(EXAMPLE_OOP) $(SRC_OOP) -o $(TARGET_OOP) $(LDFLAGS)

fp_demo:
	$(MKDIR)
	$(CC) $(CFLAGS) $(EXAMPLE_FP) $(SRC_FP) -o $(TARGET_FP) $(LDFLAGS)

type_infer_demo:
	$(MKDIR)
	$(CC) $(CFLAGS) $(EXAMPLE_TYPE) $(SRC_TYPE) -o $(TARGET_TYPE) $(LDFLAGS)

test: $(ALL_SRC) $(TEST_MAIN)
	$(MKDIR)
	$(CC) $(CFLAGS) $(TEST_MAIN) $(ALL_SRC) -o $(TARGET_TEST) $(LDFLAGS)
	$(TARGET_TEST)

clean:
	rm -rf $(OUTDIR)
