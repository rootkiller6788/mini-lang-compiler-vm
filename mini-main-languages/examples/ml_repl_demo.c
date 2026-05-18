#include "ml_like.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== ML-like Lambda Calculus Interpreter Demo ===\n\n");

    printf("--- Test 1: Simple application ((lambda (x) (+ x 1)) 5) ---\n");
    {
        const char *src = "((lambda (x) (+ x 1)) 5)";
        int pos = 0;
        MLExpr *expr = ml_parse(src, &pos);
        if (expr) {
            MLValue result = ml_eval(expr, NULL);
            printf("Result: ");
            ml_print_value(result);
            printf("\n");
            ml_free_expr(expr);
        }
    }

    printf("\n--- Test 2: Recursive factorial with letrec ---\n");
    {
        const char *src =
            "(letrec (fact (lambda (n)"
            "  (if (= n 0) 1 (* n (fact (- n 1))))))"
            "  (fact 5))";
        int pos = 0;
        MLExpr *expr = ml_parse(src, &pos);
        if (expr) {
            printf("Parsed expression: ");
            ml_print_expr(expr);
            printf("\n");
            MLValue result = ml_eval(expr, NULL);
            printf("fact(5) = ");
            ml_print_value(result);
            printf("\n");
            ml_free_expr(expr);
        }
    }

    printf("\n--- Test 3: Church encoding: TRUE = (lambda (x) (lambda (y) x)) ---\n");
    {
        const char *src = "((lambda (x) (lambda (y) x)) 1 2)";
        int pos = 0;
        MLExpr *expr = ml_parse(src, &pos);
        if (expr) {
            MLValue result = ml_eval(expr, NULL);
            printf("TRUE 1 2 = ");
            ml_print_value(result);
            printf("\n");
            ml_free_expr(expr);
        }
    }

    printf("\n--- Test 4: Application (((lambda (x) (lambda (y) (+ x y))) 10) 20) ---\n");
    {
        const char *src = "(((lambda (x) (lambda (y) (+ x y))) 10) 20)";
        int pos = 0;
        MLExpr *expr = ml_parse(src, &pos);
        if (expr) {
            printf("Parsed: ");
            ml_print_expr(expr);
            printf("\n");
            MLValue result = ml_eval(expr, NULL);
            printf("Result: ");
            ml_print_value(result);
            printf("\n");
            ml_free_expr(expr);
        }
    }

    printf("\n--- Test 5: Let binding (let (x 42) (* x 2)) ---\n");
    {
        const char *src = "(let (x 42) (* x 2))";
        int pos = 0;
        MLExpr *expr = ml_parse(src, &pos);
        if (expr) {
            MLValue result = ml_eval(expr, NULL);
            printf("Result: ");
            ml_print_value(result);
            printf("\n");
            ml_free_expr(expr);
        }
    }

    printf("\n--- Interactive REPL ---\n");
    ml_repl();

    return 0;
}
