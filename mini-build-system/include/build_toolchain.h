#ifndef BUILD_TOOLCHAIN_H
#define BUILD_TOOLCHAIN_H

#include <stdbool.h>

/* ============================================================================
 * L1: Core Definitions - Build Toolchain
 *
 * A toolchain is the set of compilers, linkers, archivers, and their
 * associated flags that translate source code into executables.
 *
 * Reference: GNU Toolchain (gcc/binutils), LLVM (clang/lld)
 * Course: MIT 6.004, CMU 15-410
 * ========================================================================= */

#define TC_NAME_LEN      64
#define TC_PATH_LEN      256
#define TC_FLAGS_LEN     512
#define TC_MAX_INCLUDES  32
#define TC_MAX_LIBS      32
#define TC_MAX_DEFINES   32

/* GNU canonical target triple: machine-vendor-os (e.g. x86_64-pc-linux-gnu) */
typedef struct {
    char machine[32];
    char vendor[32];
    char os[32];
    char abi[16];
} TargetTriple;

/* L2: Core Concept - Build variant determines optimization/debug level */
typedef enum {
    BUILD_DEBUG,
    BUILD_RELEASE,
    BUILD_RELWITHDEB,
    BUILD_MINSIZE,
    BUILD_PROFILE,
    BUILD_LTO
} BuildVariant;

typedef enum {
    TOOL_CC,
    TOOL_CXX,
    TOOL_ASM,
    TOOL_LD,
    TOOL_AR,
    TOOL_STRIP,
    TOOL_OBJCOPY
} ToolKind;

typedef struct {
    char       name[TC_NAME_LEN];
    char       path[TC_PATH_LEN];
    ToolKind   kind;
    char       flags[TC_FLAGS_LEN];
    bool       is_cross;
    char       target_triple[64];
} ToolSpec;

typedef struct {
    char       name[TC_NAME_LEN];
    TargetTriple host;
    TargetTriple target;
    BuildVariant variant;
    ToolSpec   tools[8];
    int        num_tools;
    char       include_paths[TC_MAX_INCLUDES][TC_PATH_LEN];
    int        num_includes;
    char       lib_paths[TC_MAX_LIBS][TC_PATH_LEN];
    int        num_libs;
    char       defines[TC_MAX_DEFINES][128];
    int        num_defines;
    bool       is_valid;
} ToolchainProfile;

void tc_init(ToolchainProfile *tc, const char *name);
void tc_set_variant(ToolchainProfile *tc, BuildVariant v);
bool tc_add_tool(ToolchainProfile *tc, ToolKind kind, const char *path, const char *flags);
bool tc_add_include(ToolchainProfile *tc, const char *path);
bool tc_add_lib(ToolchainProfile *tc, const char *path);
bool tc_add_define(ToolchainProfile *tc, const char *define);
ToolSpec *tc_find_tool(ToolchainProfile *tc, ToolKind kind);
bool tc_validate(ToolchainProfile *tc);
bool tc_detect_host(ToolchainProfile *tc);
bool tc_set_cross_target(ToolchainProfile *tc, const char *triple_str);
bool tc_parse_triple(const char *triple_str, TargetTriple *triple);
void tc_triple_to_string(const TargetTriple *triple, char *buf, size_t buf_size);
int  tc_compile_cmd(const ToolchainProfile *tc, const char *source,
                    const char *output, char *cmd_buf, size_t buf_size);
int  tc_link_cmd(const ToolchainProfile *tc, const char *inputs[],
                 int num_inputs, const char *output, char *cmd_buf, size_t buf_size);
int  tc_archive_cmd(const ToolchainProfile *tc, const char *objects[],
                    int num_objects, const char *output, char *cmd_buf, size_t buf_size);
void tc_print(const ToolchainProfile *tc);
const char *tc_variant_name(BuildVariant v);
const char *tc_tool_kind_name(ToolKind k);

#endif
