#define _CRT_SECURE_NO_WARNINGS
#include "build_toolchain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * ============================================================================
 * Build Toolchain Implementation
 *
 * L1: Core Definitions - ToolchainProfile, TargetTriple, ToolSpec
 * L2: Core Concept - Cross-compilation, Build Variants
 * L3: Engineering Structure - Command generation from metadata
 * L4: Standards - GNU target triple specification (autoconf)
 * L7: Application - CI/CD toolchain auto-detection
 *
 * Reference: GNU Autoconf Manual, CMake toolchain files
 * Course: MIT 6.004, CMU 15-410
 * ============================================================================
 */

void tc_init(ToolchainProfile *tc, const char *name) {
    memset(tc, 0, sizeof(*tc));
    strncpy(tc->name, name, TC_NAME_LEN - 1);
    tc->variant = BUILD_DEBUG;
    tc->num_tools = 0;
    tc->num_includes = 0;
    tc->num_libs = 0;
    tc->num_defines = 0;
    tc->is_valid = false;
    strcpy(tc->host.machine, "x86_64");
    strcpy(tc->host.vendor, "pc");
    strcpy(tc->host.os, "windows");
    strcpy(tc->host.abi, "msvc");
    memcpy(&tc->target, &tc->host, sizeof(TargetTriple));
}

void tc_set_variant(ToolchainProfile *tc, BuildVariant v) {
    tc->variant = v;
    tc->is_valid = false;
}

bool tc_add_tool(ToolchainProfile *tc, ToolKind kind, const char *path,
                 const char *flags) {
    if (tc->num_tools >= 8) return false;
    ToolSpec *ts = &tc->tools[tc->num_tools];
    memset(ts, 0, sizeof(*ts));
    ts->kind = kind;
    strncpy(ts->path, path, TC_PATH_LEN - 1);
    strncpy(ts->name, tc_tool_kind_name(kind), TC_NAME_LEN - 1);
    strncpy(ts->flags, flags, TC_FLAGS_LEN - 1);
    ts->is_cross = false;
    tc->num_tools++;
    tc->is_valid = false;
    return true;
}

bool tc_add_include(ToolchainProfile *tc, const char *path) {
    if (tc->num_includes >= TC_MAX_INCLUDES) return false;
    strncpy(tc->include_paths[tc->num_includes], path, TC_PATH_LEN - 1);
    tc->num_includes++;
    return true;
}

bool tc_add_lib(ToolchainProfile *tc, const char *path) {
    if (tc->num_libs >= TC_MAX_LIBS) return false;
    strncpy(tc->lib_paths[tc->num_libs], path, TC_PATH_LEN - 1);
    tc->num_libs++;
    return true;
}

bool tc_add_define(ToolchainProfile *tc, const char *define) {
    if (tc->num_defines >= TC_MAX_DEFINES) return false;
    strncpy(tc->defines[tc->num_defines], define, 127);
    tc->num_defines++;
    return true;
}

ToolSpec *tc_find_tool(ToolchainProfile *tc, ToolKind kind) {
    for (int i = 0; i < tc->num_tools; i++) {
        if (tc->tools[i].kind == kind) return &tc->tools[i];
    }
    return NULL;
}

bool tc_validate(ToolchainProfile *tc) {
    bool has_compiler = tc_find_tool((ToolchainProfile *)tc, TOOL_CC) != NULL ||
                        tc_find_tool((ToolchainProfile *)tc, TOOL_CXX) != NULL;
    bool has_linker   = tc_find_tool((ToolchainProfile *)tc, TOOL_LD) != NULL;
    tc->is_valid = has_compiler && has_linker;
    return tc->is_valid;
}

/* L7: Application - Detect host toolchain for CI/CD environments.
 * In production, this would scan PATH; here we use sensible defaults. */
bool tc_detect_host(ToolchainProfile *tc) {
    tc_init(tc, "host");
    tc_add_tool(tc, TOOL_CC, "cc", "-Wall -Wextra");
    tc_add_tool(tc, TOOL_LD, "cc", "");
    return tc_validate(tc);
}

/* L4: Standards - Parse GNU target triples.
 *
 * Format: machine-vendor-os[-abi]
 * Examples:
 *   x86_64-pc-linux-gnu  (64-bit Linux on x86)
 *   arm-none-eabi        (ARM bare-metal embedded)
 *   aarch64-apple-darwin (Apple Silicon macOS)
 *   wasm32-unknown-unknown (WebAssembly)
 *
 * Reference: GNU Autoconf Manual, Section "Specifying Target Triplets"
 *            LLVM Triple.h specification
 */
bool tc_parse_triple(const char *triple_str, TargetTriple *triple) {
    char buf[128];
    strncpy(buf, triple_str, 127);
    buf[127] = '\0';
    memset(triple, 0, sizeof(*triple));

    char *parts[4] = {NULL};
    int pc = 0;
    char *tok = buf;
    char *next;

    while ((next = strchr(tok, '-')) != NULL && pc < 4) {
        *next = '\0';
        parts[pc++] = tok;
        tok = next + 1;
    }
    if (*tok && pc < 4) parts[pc++] = tok;

    if (pc >= 1) strncpy(triple->machine, parts[0], 31);
    if (pc >= 2) strncpy(triple->vendor,   parts[1], 31);
    if (pc >= 3) strncpy(triple->os,       parts[2], 31);
    if (pc >= 4) strncpy(triple->abi,      parts[3], 15);

    return pc >= 3;
}

/* L2: Core Concept - Cross-compilation setup.
 * Cross-compilation enables building software for a platform different
 * from the host. Essential for embedded systems, game consoles,
 * and OS kernel development. */
bool tc_set_cross_target(ToolchainProfile *tc, const char *triple_str) {
    TargetTriple cross_triple;
    if (!tc_parse_triple(triple_str, &cross_triple)) return false;

    memcpy(&tc->target, &cross_triple, sizeof(TargetTriple));

    bool is_cross = (strcmp(tc->host.machine, tc->target.machine) != 0)
                 || (strcmp(tc->host.os, tc->target.os) != 0);

    for (int i = 0; i < tc->num_tools; i++) {
        tc->tools[i].is_cross = is_cross;
        snprintf(tc->tools[i].target_triple, 63, "%s-%s-%s",
                 tc->target.machine, tc->target.vendor, tc->target.os);
    }
    tc->is_valid = false;
    return true;
}

void tc_triple_to_string(const TargetTriple *triple, char *buf, size_t buf_size) {
    if (triple->abi[0]) {
        snprintf(buf, buf_size, "%s-%s-%s-%s",
                 triple->machine, triple->vendor, triple->os, triple->abi);
    } else {
        snprintf(buf, buf_size, "%s-%s-%s",
                 triple->machine, triple->vendor, triple->os);
    }
}

/* GCC optimization level mapping (L3: Engineering Structure)
 * Reference: GCC manual, section "Options That Control Optimization"
 *   -O0: no optimization (default, fastest compilation)
 *   -O1: basic optimizations
 *   -O2: standard optimization (good compile-time/performance tradeoff)
 *   -O3: aggressive optimization (auto-vectorization, inlining)
 *   -Os: optimize for size
 *   -Ofast: -O3 + non-standard-compliant optimizations */
static const char *variant_flags(BuildVariant v) {
    switch (v) {
        case BUILD_DEBUG:       return "-O0 -g";
        case BUILD_RELEASE:     return "-O2 -DNDEBUG";
        case BUILD_RELWITHDEB:  return "-O2 -g";
        case BUILD_MINSIZE:     return "-Os -DNDEBUG";
        case BUILD_PROFILE:     return "-O2 -pg";
        case BUILD_LTO:         return "-O2 -flto";
        default:                return "-O0";
    }
}

/* Space-delimited flag accumulator with buffer overflow protection */
static void append_flag(char *buf, size_t *pos, size_t buf_size, const char *flag) {
    size_t flen = strlen(flag);
    if (flen == 0) return;
    if (*pos + flen + 2 >= buf_size) return;
    if (*pos > 0) buf[(*pos)++] = ' ';
    memcpy(buf + *pos, flag, flen);
    *pos += flen;
    buf[*pos] = '\0';
}

/* L3: Engineering Structure - Compile command generation.
 *
 * Constructs a complete compiler command line from toolchain profile metadata.
 * This is the bridge between declarative build descriptions and executable
 * shell commands, analogous to how CMake generates Ninja files. */
int tc_compile_cmd(const ToolchainProfile *tc, const char *source,
                   const char *output, char *cmd_buf, size_t buf_size) {
    ToolSpec *cc = tc_find_tool((ToolchainProfile *)tc, TOOL_CC);
    if (!cc) cc = tc_find_tool((ToolchainProfile *)tc, TOOL_CXX);
    if (!cc) return 0;

    size_t pos = 0;
    cmd_buf[0] = '\0';

    append_flag(cmd_buf, &pos, buf_size, cc->path);
    append_flag(cmd_buf, &pos, buf_size, variant_flags(tc->variant));

    for (int i = 0; i < tc->num_includes; i++) {
        char inc_flag[TC_PATH_LEN + 3];
        snprintf(inc_flag, sizeof(inc_flag), "-I%s", tc->include_paths[i]);
        append_flag(cmd_buf, &pos, buf_size, inc_flag);
    }

    for (int i = 0; i < tc->num_defines; i++) {
        char def_flag[140];
        snprintf(def_flag, sizeof(def_flag), "-D%s", tc->defines[i]);
        append_flag(cmd_buf, &pos, buf_size, def_flag);
    }

    append_flag(cmd_buf, &pos, buf_size, cc->flags);
    append_flag(cmd_buf, &pos, buf_size, "-c");
    append_flag(cmd_buf, &pos, buf_size, source);
    append_flag(cmd_buf, &pos, buf_size, "-o");
    append_flag(cmd_buf, &pos, buf_size, output);

    return (int)strlen(cmd_buf);
}

int tc_link_cmd(const ToolchainProfile *tc, const char *inputs[],
                int num_inputs, const char *output, char *cmd_buf, size_t buf_size) {
    ToolSpec *ld = tc_find_tool((ToolchainProfile *)tc, TOOL_LD);
    if (!ld) { ld = tc_find_tool((ToolchainProfile *)tc, TOOL_CC); }
    if (!ld) { ld = tc_find_tool((ToolchainProfile *)tc, TOOL_CXX); }
    if (!ld) return 0;

    size_t pos = 0;
    cmd_buf[0] = '\0';

    append_flag(cmd_buf, &pos, buf_size, ld->path);
    append_flag(cmd_buf, &pos, buf_size, ld->flags);

    for (int i = 0; i < num_inputs; i++)
        append_flag(cmd_buf, &pos, buf_size, inputs[i]);

    for (int i = 0; i < tc->num_libs; i++) {
        char lib_flag[TC_PATH_LEN + 3];
        snprintf(lib_flag, sizeof(lib_flag), "-L%s", tc->lib_paths[i]);
        append_flag(cmd_buf, &pos, buf_size, lib_flag);
    }

    append_flag(cmd_buf, &pos, buf_size, "-o");
    append_flag(cmd_buf, &pos, buf_size, output);

    return (int)strlen(cmd_buf);
}

int tc_archive_cmd(const ToolchainProfile *tc, const char *objects[],
                   int num_objects, const char *output, char *cmd_buf, size_t buf_size) {
    ToolSpec *ar = tc_find_tool((ToolchainProfile *)tc, TOOL_AR);
    if (!ar) { ar = tc_find_tool((ToolchainProfile *)tc, TOOL_LD); }
    if (!ar) return 0;

    size_t pos = 0;
    cmd_buf[0] = '\0';

    append_flag(cmd_buf, &pos, buf_size, ar->path);
    append_flag(cmd_buf, &pos, buf_size, "rcs");
    append_flag(cmd_buf, &pos, buf_size, output);

    for (int i = 0; i < num_objects; i++)
        append_flag(cmd_buf, &pos, buf_size, objects[i]);

    return (int)strlen(cmd_buf);
}

void tc_print(const ToolchainProfile *tc) {
    char host_str[100], tgt_str[100];
    tc_triple_to_string(&tc->host, host_str, sizeof(host_str));
    tc_triple_to_string(&tc->target, tgt_str, sizeof(tgt_str));

    printf("\n=== Toolchain: %s ===\n", tc->name);
    printf("  Host:    %s\n", host_str);
    printf("  Target:  %s\n", tgt_str);
    printf("  Variant: %s\n", tc_variant_name(tc->variant));
    printf("  Valid:   %s\n", tc->is_valid ? "yes" : "no");
    printf("  Tools (%d):\n", tc->num_tools);
    for (int i = 0; i < tc->num_tools; i++) {
        printf("    [%s] %s %s%s\n",
               tc_tool_kind_name(tc->tools[i].kind),
               tc->tools[i].path,
               tc->tools[i].flags,
               tc->tools[i].is_cross ? " (cross)" : "");
    }
    printf("  Includes (%d):", tc->num_includes);
    for (int i = 0; i < tc->num_includes; i++)
        printf(" %s", tc->include_paths[i]);
    printf("\n  Libs (%d):", tc->num_libs);
    for (int i = 0; i < tc->num_libs; i++)
        printf(" %s", tc->lib_paths[i]);
    printf("\n  Defines (%d):", tc->num_defines);
    for (int i = 0; i < tc->num_defines; i++)
        printf(" -D%s", tc->defines[i]);
    printf("\n==============================\n");
}

const char *tc_variant_name(BuildVariant v) {
    switch (v) {
        case BUILD_DEBUG:       return "debug";
        case BUILD_RELEASE:     return "release";
        case BUILD_RELWITHDEB:  return "relwithdebinfo";
        case BUILD_MINSIZE:     return "minsize";
        case BUILD_PROFILE:     return "profile";
        case BUILD_LTO:         return "lto";
        default:                return "unknown";
    }
}

const char *tc_tool_kind_name(ToolKind k) {
    switch (k) {
        case TOOL_CC:      return "CC";
        case TOOL_CXX:     return "CXX";
        case TOOL_ASM:     return "ASM";
        case TOOL_LD:      return "LD";
        case TOOL_AR:      return "AR";
        case TOOL_STRIP:   return "STRIP";
        case TOOL_OBJCOPY: return "OBJCOPY";
        default:           return "???";
    }
}
