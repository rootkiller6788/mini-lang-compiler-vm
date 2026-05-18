#define _CRT_SECURE_NO_WARNINGS
#include "make_engine.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    static MakeFile mf;
    memset(&mf, 0, sizeof(mf));

    printf("=== mini-make parser demo ===\n\n");

    /* Define variables */
    mf.num_vars = 3;
    strcpy(mf.variables[0].name, "CC");
    strcpy(mf.variables[0].value, "gcc");
    strcpy(mf.variables[1].name, "CFLAGS");
    strcpy(mf.variables[1].value, "-Wall -O2");
    strcpy(mf.variables[2].name, "OBJS");
    strcpy(mf.variables[2].value, "hello.o main.o");

    /* Define rules */
    mf.num_rules = 5;

    /* hello: hello.o main.o */
    strcpy(mf.rules[0].target, "hello");
    strcpy(mf.rules[0].prerequisites[0], "hello.o");
    strcpy(mf.rules[0].prerequisites[1], "main.o");
    mf.rules[0].num_prereqs = 2;
    strcpy(mf.rules[0].commands[0], "gcc -o hello hello.o main.o");
    mf.rules[0].num_commands = 1;
    mf.rules[0].implicit = false;
    mf.rules[0].is_pattern = false;

    /* hello.o: hello.c */
    strcpy(mf.rules[1].target, "hello.o");
    strcpy(mf.rules[1].prerequisites[0], "hello.c");
    mf.rules[1].num_prereqs = 1;
    strcpy(mf.rules[1].commands[0], "gcc -c hello.c -o hello.o");
    mf.rules[1].num_commands = 1;
    mf.rules[1].implicit = false;
    mf.rules[1].is_pattern = false;

    /* main.o: main.c */
    strcpy(mf.rules[2].target, "main.o");
    strcpy(mf.rules[2].prerequisites[0], "main.c");
    mf.rules[2].num_prereqs = 1;
    strcpy(mf.rules[2].commands[0], "gcc -c main.c -o main.o");
    mf.rules[2].num_commands = 1;
    mf.rules[2].implicit = false;
    mf.rules[2].is_pattern = false;

    /* Pattern rule: %.o: %.c */
    strcpy(mf.rules[3].target, "%.o");
    strcpy(mf.rules[3].prerequisites[0], "%.c");
    mf.rules[3].num_prereqs = 1;
    strcpy(mf.rules[3].commands[0], "$(CC) -c $(CFLAGS) $< -o $@");
    mf.rules[3].num_commands = 1;
    mf.rules[3].implicit = true;
    mf.rules[3].is_pattern = true;

    /* .PHONY target */
    strcpy(mf.rules[4].target, ".PHONY");
    strcpy(mf.rules[4].prerequisites[0], "clean");
    strcpy(mf.rules[4].prerequisites[1], "all");
    mf.rules[4].num_prereqs = 2;
    mf.rules[4].num_commands = 0;
    mf.rules[4].implicit = false;
    mf.rules[4].is_pattern = false;

    strcpy(mf.default_target, "hello");

    /* Resolve variables */
    make_resolve_vars(&mf);

    /* Print dependency graph */
    make_print_graph(&mf);

    /* Expand auto variables for specific rules */
    printf("\n=== Auto-variable expansion demo ===\n");
    make_expand_auto_vars(&mf.rules[1], "hello.o", "hello.c");
    printf("  Expanded command: %s\n", mf.rules[1].commands[0]);

    /* Test pattern matching */
    printf("\n=== Pattern matching demo ===\n");
    char stem[128] = {0};
    if (make_match_pattern("%.o", "world.o", stem, sizeof(stem))) {
        printf("  'world.o' matches '%%.o' -> stem='%s'\n", stem);
    }
    if (make_match_pattern("lib%.a", "libmath.a", stem, sizeof(stem))) {
        printf("  'libmath.a' matches 'lib%%.a' -> stem='%s'\n", stem);
    }

    /* Build target list */
    printf("\n=== Building targets ===\n");
    mf.num_targets = 0;
    for (int i = 0; i < mf.num_rules; i++) {
        if (strcmp(mf.rules[i].target, ".PHONY") == 0) continue;
        MakeTarget *t = &mf.targets[mf.num_targets++];
        strncpy(t->name, mf.rules[i].target, 127);
        t->rule = &mf.rules[i];
        t->dirty = false;
        t->type = mf.rules[i].is_pattern ? TARGET_PATTERN : TARGET_NORMAL;
    }

    /* Build hello */
    make_build(&mf, "hello");

    /* Find rule for target */
    printf("\n=== Rule lookup ===\n");
    MakeRule *found = make_find_rule_for_target(&mf, "hello.o");
    if (found) {
        printf("  Found rule for hello.o (%d prereqs, %d commands)\n",
               found->num_prereqs, found->num_commands);
    }

    printf("\n=== Demo complete ===\n");
    return 0;
}
