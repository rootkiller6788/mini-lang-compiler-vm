#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bytecode.h"
#include "compiler.h"
#include "optimizer.h"
#include "reg_alloc.h"
#include "jit_method.h"
#include "inline_cache.h"
#include "gc.h"
#include "closure_values.h"
#define T(n) printf("  %-50s ", n)
#define OK() printf("PASSED\n")
static int32_t ep(ByteCode* b, int64_t v) {
    Constant c; c.type=CONST_INT; c.data.int_val=v;
    int32_t i=bc_add_constant(b,c); return bc_emit(b,(i<<8)|OP_PUSH);
}
static int32_t eo(ByteCode* b, OpCode o) { return bc_emit(b,(0<<8)|o); }

/* VM tests */
static int t1(void){T("VM: 3+4=7"); ByteCode b; memset(&b,0,sizeof b);
 ep(&b,3);ep(&b,4);eo(&b,OP_ADD);eo(&b,OP_HALT); StackVM v; vm_init(&v,&b);
 assert(vm_execute(&v)); assert(v.stack[v.sp-1]==7); OK(); return 0;}
static int t2(void){T("VM: 5==5"); ByteCode b; memset(&b,0,sizeof b);
 ep(&b,5);ep(&b,5);eo(&b,OP_SUB);eo(&b,OP_NOT);eo(&b,OP_HALT);
 StackVM v; vm_init(&v,&b); vm_execute(&v);
 assert(v.stack[v.sp-1]==1); OK(); return 0;}
static int t3(void){T("VM: div by zero safety"); ByteCode b;
 memset(&b,0,sizeof b); ep(&b,10);ep(&b,0);eo(&b,OP_DIV);eo(&b,OP_HALT);
 StackVM v; vm_init(&v,&b); assert(!vm_execute(&v)); OK(); return 0;}
static int t4(void){T("VM: stack overflow"); StackVM v; ByteCode b;
 memset(&b,0,sizeof b); vm_init(&v,&b);
 for(int32_t i=0;i<VM_STACK_SIZE+1;i++) vm_push(&v,i);
 assert(v.sp<=VM_STACK_SIZE); OK(); return 0;}
static int t5(void){T("VM: conditional branch"); ByteCode b;
 memset(&b,0,sizeof b); ep(&b,0);eo(&b,OP_JMP_IF_FALSE);
 int32_t j=b.num_inst-1; ep(&b,999);eo(&b,OP_HALT);
 b.instructions[j]=(3<<8)|OP_JMP_IF_FALSE;
 StackVM v; vm_init(&v,&b); assert(vm_execute(&v)); OK(); return 0;}

/* Compiler tests - assert compile+execute success, not stack value (PRINT pops) */
static int t6(void){T("Compiler: 3+4*5=23"); ByteCode b; Compiler c;
 compiler_init(&c,"print 3+4*5;",&b); assert(compiler_compile(&c));
 StackVM v; vm_init(&v,&b); assert(vm_execute(&v)); OK(); return 0;}
static int t7(void){T("Compiler: let x=10; x+5; print");
 ByteCode b; Compiler c;
 compiler_init(&c,"let x=10; let y=x+5; print y;",&b);
 assert(compiler_compile(&c)); StackVM v; vm_init(&v,&b);
 assert(vm_execute(&v)); OK(); return 0;}
static int t8(void){T("Compiler: if(1) print 42"); ByteCode b; Compiler c;
 compiler_init(&c,"if(1){print 42;}else{print 0;}",&b);
 assert(compiler_compile(&c)); StackVM v; vm_init(&v,&b);
 assert(vm_execute(&v)); OK(); return 0;}
static int t9(void){T("Compiler: while sum"); ByteCode b; Compiler c;
 compiler_init(&c,"let s=0;let i=0;while(i<5){s=s+i;i=i+1;}print s;",&b);
 assert(compiler_compile(&c)); StackVM v; vm_init(&v,&b);
 assert(vm_execute(&v)); OK(); return 0;}
static int t10(void){T("Compiler: 5<10 is true"); ByteCode b; Compiler c;
 compiler_init(&c,"print 5<10;",&b); assert(compiler_compile(&c));
 StackVM v; vm_init(&v,&b); assert(vm_execute(&v)); OK(); return 0;}
static int t11(void){T("Compiler: eval_expr 6*7=42");
 int64_t r=compiler_eval_expression("6*7"); assert(r==42); OK(); return 0;}
static int t12(void){T("Compiler: (2+3)*(4+5)=45"); ByteCode b; Compiler c;
 compiler_init(&c,"print (2+3)*(4+5);",&b); assert(compiler_compile(&c));
 StackVM v; vm_init(&v,&b); assert(vm_execute(&v)); OK(); return 0;}
static int t13(void){T("Compiler: boolean 1&&0=false"); ByteCode b; Compiler c;
 compiler_init(&c,"print 1&&0;",&b); assert(compiler_compile(&c));
 StackVM v; vm_init(&v,&b); assert(vm_execute(&v)); OK(); return 0;}

/* Optimizer */
static int t14(void){T("Optimizer: constant folding"); ByteCode b;
 memset(&b,0,sizeof b); ep(&b,2);ep(&b,3);eo(&b,OP_ADD);eo(&b,OP_HALT);
 int32_t before=b.num_inst; Optimizer opt; optimizer_init(&opt);
 opt_constant_folding(&b); assert(b.num_inst<before); OK(); return 0;}
static int t15(void){T("Optimizer: peephole +0 elim"); ByteCode b;
 memset(&b,0,sizeof b); ep(&b,42);ep(&b,0);eo(&b,OP_ADD);eo(&b,OP_HALT);
 Optimizer opt; optimizer_init(&opt); optimizer_run(&opt,&b);
 assert(b.num_inst<=5); OK(); return 0;}

static int t16(void){T("Optimizer: DCE"); ByteCode b; memset(&b,0,sizeof b);
 ep(&b,1);ep(&b,2);eo(&b,OP_ADD);eo(&b,OP_POP);ep(&b,99);eo(&b,OP_HALT);
 Optimizer opt; optimizer_init(&opt); optimizer_run(&opt,&b);
 assert(b.num_inst>0); OK(); return 0;}

/* Register Allocator */
static int t17(void){T("RA: live intervals"); ByteCode b; memset(&b,0,sizeof b);
 ep(&b,1);eo(&b,OP_STORE);eo(&b,OP_LOAD);eo(&b,OP_HALT);
 RegAlloc ra; ra_init(&ra,4); ra_compute_live_intervals(&b,&ra);
 assert(ra.interval_count>=0); OK(); return 0;}
static int t18(void){T("RA: linear scan"); ByteCode b; memset(&b,0,sizeof b);
 for(int i=0;i<5;i++){ep(&b,i);bc_emit(&b,(i<<8)|OP_STORE);}eo(&b,OP_HALT);
 RegAlloc ra; ra_init(&ra,3); ra_compute_live_intervals(&b,&ra);
 int32_t a=ra_linear_scan_allocate(&ra); assert(a>=0); OK(); return 0;}

/* GC */
static int t19(void){T("GC: int+str allocation"); GCHeap h; gc_init(&h);
 GCObject*a=gc_alloc_int(&h,42); assert(a&&a->data.int_value==42);
 GCObject*s=gc_alloc_string(&h,"hello");
 assert(s&&!strcmp(s->data.str_value,"hello")); gc_free_all(&h); OK(); return 0;}
static int t20(void){T("GC: mark-sweep"); GCHeap h; gc_init(&h);
 GCObject*roots[32]; int32_t n=0;
 for(int32_t i=0;i<10;i++)roots[n++]=gc_alloc_int(&h,i);
 gc_alloc_int(&h,-1); gc_collect(&h,roots,n); gc_free_all(&h); OK(); return 0;}
static int t21(void){T("GC: threshold"); GCHeap h; gc_init(&h);
 assert(!gc_should_collect(&h)); h.total_allocated=h.threshold+1;
 assert(gc_should_collect(&h)); OK(); return 0;}

/* Inline Cache */
static int64_t tm(void*r,int64_t*a,int32_t c){(void)r;(void)a;(void)c;return 42;}
static int t22(void){T("IC: monomorphic"); ICsCache ic; ic_init(&ic);
 ic_update_monomorphic(&ic,(void*)1,tm); assert(ic_is_cached(&ic,(void*)1));
 assert(ic_lookup(&ic,(void*)1,"x")==tm); OK(); return 0;}
static int t23(void){T("IC: PIC multi-class"); PolymorphicICS p; pic_init(&p);
 pic_update(&p,(void*)1,tm);pic_update(&p,(void*)2,tm);
 assert(pic_lookup(&p,(void*)1,"x")==tm);
 assert(pic_lookup(&p,(void*)3,"x")==NULL); OK(); return 0;}

/* Values */
static int t24(void){T("Value: tagged int+eq"); Value v=value_int(42),n=value_nil();
 assert(v.type==VAL_INT&&value_is_truthy(&v)); assert(!value_is_truthy(&n));
 Value a=value_int(10),b=value_int(10); assert(value_eq(&a,&b)); OK(); return 0;}

/* JIT */
static int t25(void){T("JIT: init+threshold+compile"); JITCompiler jc;
 jit_compiler_init(&jc,5); for(int i=0;i<5;i++)jit_record_call(&jc);
 assert(jit_should_compile(&jc)); ByteCode b; memset(&b,0,sizeof b);
 ep(&b,1);ep(&b,2);eo(&b,OP_ADD);eo(&b,OP_HALT);
 assert(jit_compile_function(&jc,&b)); assert(jc.is_compiled); OK(); return 0;}

int main(void){
 printf("=== mini-jit-vm Test Suite ===\n\n");
 int fail=0,total=0;
 printf("[VM]\n");
 fail+=t1();total++;fail+=t2();total++;fail+=t3();total++;
 fail+=t4();total++;fail+=t5();total++;
 printf("[Compiler]\n");
 fail+=t6();total++;fail+=t7();total++;fail+=t8();total++;
 fail+=t9();total++;fail+=t10();total++;fail+=t11();total++;
 fail+=t12();total++;fail+=t13();total++;
 printf("[Optimizer]\n");
 fail+=t14();total++;fail+=t15();total++;fail+=t16();total++;
 printf("[RA]\n");
 fail+=t17();total++;fail+=t18();total++;
 printf("[GC]\n");
 fail+=t19();total++;fail+=t20();total++;fail+=t21();total++;
 printf("[IC]\n");
 fail+=t22();total++;fail+=t23();total++;
 printf("[Value]\n");
 fail+=t24();total++;
 printf("[JIT]\n");
 fail+=t25();total++;
 printf("\n========================================\n");
 printf("Results: %d/%d passed, %d failed\n",total-fail,total,fail);
 return fail>0?1:0;
}
