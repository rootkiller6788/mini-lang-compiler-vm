CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99 -I include
LDFLAGS = -lm
OUTDIR  = bin

OBJ_BYTECODE = src\bytecode.o
OBJ_JIT      = src\jit_method.o
OBJ_IC       = src\inline_cache.o
OBJ_GC       = src\gc.o
OBJ_CLOSURE  = src\closure_values.o

EXE_BYTECODE = bin\bytecode_vm_demo.exe
EXE_JIT      = bin\jit_demo.exe
EXE_GC       = bin\gc_demo.exe

.PHONY: all clean bytecode_vm jit_demo gc_demo

all: $(EXE_BYTECODE) $(EXE_JIT) $(EXE_GC)

bin:
	@if not exist bin mkdir bin

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

$(EXE_BYTECODE): $(OBJ_BYTECODE) examples\bytecode_vm_demo.c | bin
	$(CC) $(CFLAGS) examples\bytecode_vm_demo.c $(OBJ_BYTECODE) -o $(EXE_BYTECODE) $(LDFLAGS)

$(EXE_JIT): $(OBJ_BYTECODE) $(OBJ_JIT) examples\jit_demo.c | bin
	$(CC) $(CFLAGS) examples\jit_demo.c $(OBJ_BYTECODE) $(OBJ_JIT) -o $(EXE_JIT) $(LDFLAGS)

$(EXE_GC): $(OBJ_GC) examples\gc_demo.c | bin
	$(CC) $(CFLAGS) examples\gc_demo.c $(OBJ_GC) -o $(EXE_GC) $(LDFLAGS)

bytecode_vm: $(EXE_BYTECODE)

jit_demo: $(EXE_JIT)

gc_demo: $(EXE_GC)

clean:
	@if exist src\*.o del /q src\*.o
	@if exist bin rmdir /s /q bin
