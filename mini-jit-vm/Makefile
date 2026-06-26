CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-misleading-indentation -Wno-unused-function -Wno-stringop-truncation -O2 -std=c99 -I include
LDFLAGS = -lm
OUTDIR  = bin

OBJ_BYTECODE = src\bytecode.o
OBJ_JIT      = src\jit_method.o
OBJ_IC       = src\inline_cache.o
OBJ_GC       = src\gc.o
OBJ_CLOSURE  = src\closure_values.o
OBJ_COMPILER = src\compiler.o
OBJ_OPT      = src\optimizer.o
OBJ_RA       = src\reg_alloc.o

EXE_BYTECODE = bin\bytecode_vm_demo.exe
EXE_JIT      = bin\jit_demo.exe
EXE_GC       = bin\gc_demo.exe
EXE_COMPILER = bin\compiler_demo.exe
EXE_TEST     = bin\test_all.exe

.PHONY: all clean bytecode_vm jit_demo gc_demo test

all: $(EXE_BYTECODE) $(EXE_JIT) $(EXE_GC)

bin:
	@mkdir -p bin

$(OBJ_BYTECODE): src\bytecode.c include\bytecode.h
	$(CC) $(CFLAGS) -c src\bytecode.c -o $(OBJ_BYTECODE)

$(OBJ_JIT): src\jit_method.c include\jit_method.h include\bytecode.h
	$(CC) $(CFLAGS) -c src\jit_method.c -o $(OBJ_JIT)

$(OBJ_IC): src\inline_cache.c include\inline_cache.h
	$(CC) $(CFLAGS) -c src\inline_cache.c -o $(OBJ_IC)

$(OBJ_GC): src\gc.c include\gc.h
	$(CC) $(CFLAGS) -c src\gc.c -o $(OBJ_GC)

$(OBJ_CLOSURE): src\closure_values.c include\closure_values.h include\gc.h include\bytecode.h
	$(CC) $(CFLAGS) -c src\closure_values.c -o $(OBJ_CLOSURE)

$(OBJ_COMPILER): src\compiler.c include\compiler.h include\bytecode.h include\closure_values.h
	$(CC) $(CFLAGS) -c src\compiler.c -o $(OBJ_COMPILER)

$(OBJ_OPT): src\optimizer.c include\optimizer.h include\bytecode.h
	$(CC) $(CFLAGS) -c src\optimizer.c -o $(OBJ_OPT)

$(OBJ_RA): src\reg_alloc.c include\reg_alloc.h include\bytecode.h
	$(CC) $(CFLAGS) -c src\reg_alloc.c -o $(OBJ_RA)

$(EXE_BYTECODE): $(OBJ_BYTECODE) examples\bytecode_vm_demo.c | bin
	$(CC) $(CFLAGS) examples\bytecode_vm_demo.c $(OBJ_BYTECODE) -o $(EXE_BYTECODE) $(LDFLAGS)

$(EXE_JIT): $(OBJ_BYTECODE) $(OBJ_JIT) examples\jit_demo.c | bin
	$(CC) $(CFLAGS) examples\jit_demo.c $(OBJ_BYTECODE) $(OBJ_JIT) -o $(EXE_JIT) $(LDFLAGS)

$(EXE_GC): $(OBJ_GC) examples\gc_demo.c | bin
	$(CC) $(CFLAGS) examples\gc_demo.c $(OBJ_GC) -o $(EXE_GC) $(LDFLAGS)

$(EXE_COMPILER): $(OBJ_BYTECODE) $(OBJ_COMPILER) $(OBJ_CLOSURE) examples\compiler_demo.c | bin
	$(CC) $(CFLAGS) examples\compiler_demo.c $(OBJ_BYTECODE) $(OBJ_COMPILER) $(OBJ_CLOSURE) -o $(EXE_COMPILER) $(LDFLAGS)

# Test target — comprehensive test suite
$(EXE_TEST): $(OBJ_BYTECODE) $(OBJ_COMPILER) $(OBJ_CLOSURE) $(OBJ_GC) $(OBJ_JIT) $(OBJ_IC) $(OBJ_OPT) $(OBJ_RA) tests\test_all.c | bin
	$(CC) $(CFLAGS) tests\test_all.c $(OBJ_BYTECODE) $(OBJ_COMPILER) $(OBJ_CLOSURE) $(OBJ_GC) $(OBJ_JIT) $(OBJ_IC) $(OBJ_OPT) $(OBJ_RA) -o $(EXE_TEST) $(LDFLAGS)

test: $(EXE_TEST)
	@echo "Running tests..."
	@$(EXE_TEST)
	@echo "Tests complete."

bytecode_vm: $(EXE_BYTECODE)

jit_demo: $(EXE_JIT)

gc_demo: $(EXE_GC)

clean:
	@rm -f src/*.o
	@rm -rf bin
