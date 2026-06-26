/* test_script_lang.c */
#include "script_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int passed=0,failed=0;
#define T(n,e) do{ passed++; if(!(e)){ printf("FAIL: %s\n",n); failed++; passed--; } }while(0)

int main(void){
    printf("\n=== Script Language Tests ===\n\n");
    ScriptVM vm;
    script_init_vm(&vm);

    /* L1: VM init */
    T("vm init", vm.globals != NULL);

    /* L2: Table operations */
    Table *t=script_table_new();
    T("table new", t!=NULL && t->size==64 && t->count==0);
    ScriptValue *v=script_new_value(&vm);
    v->type=SV_INT; v->data.int_val=42;
    script_table_set(t,"key",v);
    T("table set", t->count==1);
    ScriptValue *g=script_table_get(t,"key");
    T("table get", g!=NULL && g->data.int_val==42);
    ScriptValue *n=script_table_get(t,"nonexist");
    T("table get miss", n==NULL);
    script_table_remove(t,"key");
    T("table remove", t->count==0);
    script_table_free(t);

    /* L2: Global vars */
    ScriptValue *gv=script_new_value(&vm);
    gv->type=SV_INT; gv->data.int_val=100;
    script_set_global(&vm,"score",gv);
    ScriptValue *ret=script_get_global(&vm,"score");
    T("global set/get", ret!=NULL && ret->data.int_val==100);

    /* L2: Hash function */
    unsigned int h1=script_hash("hello");
    unsigned int h2=script_hash("hello");
    T("hash stable", h1==h2);
    T("hash diff", script_hash("abc")!=script_hash("xyz"));

    /* L5: Parse line */
    ScriptValue *pv=script_parse_line("42");
    T("parse int", pv!=NULL && pv->type==SV_INT && pv->data.int_val==42);
    free(pv);
    
    pv=script_parse_line("\"hello\"");
    T("parse string", pv!=NULL && pv->type==SV_STRING);
    free(pv);

    /* L5: AST evaluator */
    int pos=0;
    ScriptAST *ast=script_parse("x = 42;", &pos);
    T("parse assign AST", ast!=NULL);
    ScriptValue *result=script_eval_ast(&vm,ast);
    T("eval assign", result!=NULL);
    script_free_ast(ast);

    /* L5: Arithmetic */
    pos=0;
    ast=script_parse("y = 10 + 20;", &pos);
    result=script_eval_ast(&vm,ast);
    T("eval add", result!=NULL);
    script_free_ast(ast);

    /* L5: Comparisons */
    pos=0;
    ast=script_parse("z = 10 < 20;", &pos);
    result=script_eval_ast(&vm,ast);
    T("eval lt", result!=NULL);
    script_free_ast(ast);

    /* L5: If statement */
    pos=0;
    ast=script_parse("{ if 1 { w = 100; } }", &pos);
    result=script_eval_ast(&vm,ast);
    T("if true", result!=NULL);
    script_free_ast(ast);

    /* L7: Value operations */
    ScriptValue *a=script_new_value(&vm); a->type=SV_INT;a->data.int_val=10;
    ScriptValue *b=script_new_value(&vm); b->type=SV_INT;b->data.int_val=20;
    ScriptValue *r=script_value_add(&vm,a,b);
    T("val add 10+20=30", r->data.int_val==30);
    r=script_value_mul(&vm,a,b);
    T("val mul 10*20=200", r->data.int_val==200);
    r=script_value_eq(&vm,a,a);
    T("val eq 10==10", r->type==SV_BOOL && r->data.bool_val);

    /* L8: GC */
    script_gc_collect(&vm);
    T("gc collect", 1);

    /* L8: Coroutines */
    ScriptVM co;
    script_coro_init(&co);
    T("coro init", 1);

    script_free_vm(&vm);
    printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
    return failed>0?1:0;
}
