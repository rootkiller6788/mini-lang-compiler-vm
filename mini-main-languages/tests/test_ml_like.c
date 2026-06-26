/* test_ml_like.c */
#include "ml_like.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define T(n,e) do { passed++; if(!(e)){ printf("FAIL: %s\n",n); failed++; passed--; } } while(0)

int main(void) {
    printf("\n=== ML-like Lambda Calculus Tests ===\n\n");

    /* L1: Parse literals */
    {
        int pos=0; MLExpr *e=ml_parse("42",&pos);
        T("parse int", e && e->type==ML_INT && e->data.int_val==42);
        ml_free_expr(e);
    }
    {
        int pos=0; MLExpr *e=ml_parse("true",&pos);
        T("parse true", e && e->type==ML_BOOL && e->data.bool_val);
        ml_free_expr(e);
    }

    /* L2: Lambda evaluation */
    {
        int pos=0;
        MLExpr *e=ml_parse("((lambda (x) (+ x 1)) 5)",&pos);
        MLValue v=ml_eval(e,NULL);
        T("lambda apply", v.type==MLV_INT && v.data.int_val==6);
        ml_free_expr(e);
    }

    /* L3: Let binding */
    {
        int pos=0;
        MLExpr *e=ml_parse("(let (x 42) (* x 2))",&pos);
        MLValue v=ml_eval(e,NULL);
        T("let bind", v.type==MLV_INT && v.data.int_val==84);
        ml_free_expr(e);
    }

    /* L3: Letrec factorial */
    {
        int pos=0;
        MLExpr *e=ml_parse("(letrec (fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1)))))) (fact 5))",&pos);
        MLValue v=ml_eval(e,NULL);
        T("letrec fact 5=120", v.type==MLV_INT && v.data.int_val==120);
        ml_free_expr(e);
    }

    /* L3: Cons/car/cdr */
    {
        int pos=0;
        MLExpr *e=ml_parse("(car (cons 1 2))",&pos);
        MLValue v=ml_eval(e,NULL);
        T("car of cons", v.type==MLV_INT && v.data.int_val==1);
        ml_free_expr(e);
    }
    {
        int pos=0;
        MLExpr *e=ml_parse("(cdr (cons 1 2))",&pos);
        MLValue v=ml_eval(e,NULL);
        T("cdr of cons", v.type==MLV_INT && v.data.int_val==2);
        ml_free_expr(e);
    }

    /* L3: Nil/null? */
    {
        int pos=0;
        MLExpr *e=ml_parse("(null? nil)",&pos);
        MLValue v=ml_eval(e,NULL);
        T("null? nil = true", v.type==MLV_BOOL && v.data.bool_val);
        ml_free_expr(e);
    }

    /* L4: De Bruijn conversion */
    {
        int pos=0;
        MLExpr *e=ml_parse("(lambda (x) (+ x 1))",&pos);
        MLDBExpr *db=ml_to_de_bruijn(e,NULL);
        T("de Bruijn not null", db != NULL);
        T("de Bruijn abs type", db->type == DB_ABS);
        ml_free_db_expr(db);
        ml_free_expr(e);
    }

    /* L4: Type inference */
    {
        int pos=0;
        MLExpr *e=ml_parse("42",&pos);
        MLType *t=ml_type_infer(e,NULL);
        T("type infer int", t && t->tag==TY_INT);
        ml_type_free(t);
        ml_free_expr(e);
    }

    /* L5: Church encoding */
    {
        int pos=0;
        MLExpr *e=ml_parse("((lambda (x) (lambda (y) x)) 1 2)",&pos);
        MLValue v=ml_eval(e,NULL);
        T("TRUE 1 2 = 1", v.type==MLV_INT && v.data.int_val==1);
        ml_free_expr(e);
    }

    /* L7: CPS transformation */
    {
        int pos=0;
        MLExpr *e=ml_parse("42",&pos);
        MLExpr *cps=ml_to_cps(e);
        T("CPS not null", cps != NULL);
        ml_free_expr(cps);
        ml_free_expr(e);
    }

    printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
