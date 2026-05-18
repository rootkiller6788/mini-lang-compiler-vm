# Make Engine — 构建规则引擎

> 参考 GNU Make 语法实现，支持规则解析、变量展开、自动变量、模式匹配与拓扑构建

---

## 1. 概述

`make_engine` 模块实现了一个简化的 Make 构建系统核心。它能够：

- 解析类 Makefile 语法的构建规则
- 维护目标之间的依赖关系图
- 按拓扑顺序遍历依赖关系
- 检测文件时间戳以判定是否需要重新构建
- 支持变量引用展开和自动变量替换
- 支持模式规则（implicit rules）匹配
- 支持 `.PHONY` 伪目标声明

### 核心数据结构

```
MakeRule      ── 单条构建规则：目标、前置条件、配方命令
MakeVariable  ── 变量名/值对
MakeTarget    ── 构建目标：名称、关联规则、脏标记、时间戳
MakeFile      ── 完整的 Make 描述文件对象
```

---

## 2. Rule 语法

### 基本规则格式

```make
target: prerequisite1 prerequisite2 ...
	command1
	command2
```

- **target**: 构建目标（文件名或伪目标）
- **prerequisites**: 前置依赖列表，以空格分隔
- **commands**: 以 TAB 字符开头的 Shell 命令序列

### 示例

```make
hello: hello.o main.o
	gcc -o hello hello.o main.o

hello.o: hello.c hello.h
	gcc -c hello.c -o hello.o

main.o: main.c
	gcc -c main.c -o main.o
```

### 规则语义

1. 当目标文件不存在时，需要构建
2. 当任一前置文件比目标文件更新时，需要构建
3. 前置文件可以依赖其他规则，形成依赖链
4. 构建前先递归构建所有前置依赖

---

## 3. 变量系统

### 变量定义

```make
CC = gcc
CFLAGS = -Wall -O2 -g
OBJS = hello.o main.o
```

变量定义语法：`NAME = VALUE`，等号两边可以有空格。

### 变量引用

```make
hello: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
```

使用 `$(VAR_NAME)` 语法引用变量。变量展开在规则解析后、命令执行前进行。

### 实现细节

- `make_resolve_vars()` 函数遍历所有变量，对每个变量的值进行 `$(...)` 引用的递归展开
- 变量名区分大小写
- 未定义变量引用保持原样（不展开）
- 变量值的最大长度为 `MAX_VAR_VALUE` (256 字节)

### 使用场景

```make
# 编译器与标志集中管理
CC = gcc
CXX = g++
CFLAGS = -Wall -O2

# 源文件列表
SRCS = main.c util.c io.c
OBJS = $(SRCS:.c=.o)

# 目标程序名
PROG = myapp
```

---

## 4. 自动变量

在执行规则的配方命令时，自动变量会被替换为实际值：

| 变量 | 含义 | 示例 |
|------|------|------|
| `$@` | 当前目标文件名 | `hello` |
| `$<` | 第一个前置条件 | `hello.c` |
| `$^` | 所有前置条件（去重） | `hello.o main.o` |
| `$?` | 比目标新的前置条件 | `hello.o` |
| `$*` | 模式匹配的 stem 部分 | `hello` |

### 实现

`make_expand_auto_vars()` 在命令执行前将 `$@` 和 `$<` 替换为目标名和第一个前置名。

### 示例

```make
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
```

当构建 `hello.o` 时，展开为：
```
gcc -c -Wall -O2 hello.c -o hello.o
```

---

## 5. 模式规则 (Pattern Rules)

模式规则使用 `%` 通配符匹配一类文件。

### 格式

```make
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
```

- `%.o` 匹配任何 `.o` 文件的目标
- `%.c` 提取对应的源文件作为前置条件

### 匹配算法

`make_match_pattern()` 函数实现：

1. 在模式串中找到 `%` 位置
2. 提取前缀和后缀部分
3. 在目标名中匹配前缀 + 通配部分 + 后缀
4. 通配部分即为 stem（可被 `$*` 引用）

### 匹配示例

| 模式 | 目标 | stem | 匹配 |
|------|------|------|------|
| `%.o` | `hello.o` | `hello` | yes |
| `%.o` | `main.o` | `main` | yes |
| `lib%.a` | `libmath.a` | `math` | yes |
| `%.o` | `hello.c` | — | no |
| `dir/%.h` | `dir/config.h` | `config` | yes |

### 优先级

1. 精确匹配规则优先于模式规则
2. 当多个模式规则匹配时，选择 stem 最短的
3. 显式规则优先于隐式（模式）规则

---

## 6. .PHONY 伪目标

`.PHONY` 声明不会对应实际文件的构建目标：

```make
.PHONY: clean all test

clean:
	rm -f *.o hello

all: hello test

test: hello
	./hello --test
```

### 特性

- `.PHONY` 目标总被视为 "dirty"（需要执行）
- 伪目标不会检查文件时间戳
- 常用于 `clean`、`install`、`all` 等管理任务
- 伪目标可以有依赖关系

### 实现细节

在 `build_target_list()` 中，遇到 `.PHONY` 规则时：
- 将其所有前置条件标记为 `TARGET_PHONY` 类型
- 构建时跳过时间戳检查
- 始终执行配方命令

---

## 7. 构建流程

### make_build() 流程

```
make_build(target)
  │
  ├── 1. 解析 Makefile → 内部 MakeFile 结构
  ├── 2. 展开所有变量引用
  ├── 3. 建立目标列表 (build_target_list)
  ├── 4. 从目标开始深度优先遍历
  │     ├── 递归构建所有前置条件
  │     ├── 检查目标是否 dirty (make_target_is_dirty)
  │     ├── 如果 dirty → 执行配方命令
  │     └── 更新目标时间戳
  └── 5. 输出构建日志
```

### 脏检查 (make_target_is_dirty)

- 伪目标 → 总是 dirty
- 目标文件不存在 → dirty
- 任一前置文件的修改时间晚于目标 → dirty
- 任一前置目标本身 dirty → dirty

### 拓扑保证

通过深度优先递归确保前置依赖先于目标构建，自然形成拓扑排序。

---

## 8. 完整示例

### Makefile 内容

```make
CC = gcc
CFLAGS = -Wall -O2

hello: hello.o main.o util.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

hello.o: hello.c hello.h
main.o: main.c main.h config.h
util.o: util.c util.h

.PHONY: clean

clean:
	rm -f *.o hello
```

### 解析后结构

```
Rules:
  [hello]       -> hello.o, main.o, util.o
  [hello.o]     -> hello.c, hello.h
  [main.o]      -> main.c, main.h, config.h
  [util.o]      -> util.c, util.h
  [%.o]         -> %.c              (implicit, pattern)
  [.PHONY]      -> clean

Variables:
  CC = gcc
  CFLAGS = -Wall -O2
```

### 构建输出

```
[make] Building target: hello.o
    $ gcc -Wall -O2 -c hello.c -o hello.o
[make] Building target: main.o
    $ gcc -Wall -O2 -c main.c -o main.o
[make] Building target: util.o
    $ gcc -Wall -O2 -c util.c -o util.o
[make] Building target: hello
    $ gcc -Wall -O2 -o hello hello.o main.o util.o
```

---

## 9. 高级特性

### 变量嵌套引用

```make
CC_TYPE = gcc
CC = $(CC_TYPE)
# $(CC) -> gcc
```

### 条件构建（未来扩展）

```
ifeq ($(DEBUG),1)
CFLAGS += -g -DDEBUG
endif
```

### 函数调用（未来扩展）

```
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
```

### 多目标规则

```make
%.tab.c %.tab.h: %.y
	bison -d $<
```

---

## 10. API 参考

| 函数 | 描述 |
|------|------|
| `make_parse(mf, path)` | 解析 Makefile 文件 |
| `make_build(mf, target)` | 从目标开始执行构建 |
| `make_resolve_vars(mf)` | 展开所有变量引用 |
| `make_print_graph(mf)` | 打印依赖图 |
| `make_expand_auto_vars(rule, tgt, prq)` | 展开自动变量 |
| `make_match_pattern(pattern, tgt, stem, sz)` | 模式匹配 |
| `make_find_rule_for_target(mf, tgt)` | 查找匹配的规则 |
| `make_target_is_dirty(mf, tgt)` | 检查目标是否脏 |
| `make_execute_recipe(rule)` | 执行配方命令 |

### 常量

| 常量 | 值 | 描述 |
|------|------|------|
| `MAX_PREREQS` | 16 | 每个规则最大前置条件数 |
| `MAX_COMMANDS` | 8 | 每个规则最大命令数 |
| `MAX_VARIABLES` | 64 | 最大变量数 |
| `MAX_RULES` | 256 | 最大规则数 |
| `MAX_TARGETS` | 256 | 最大目标数 |
| `MAX_LINE_LEN` | 512 | 最大行长度 |

---

## 11. 与 GNU Make 的差异

| 特性 | mini-make | GNU Make |
|------|-----------|----------|
| 变量引用 | `$(VAR)` | `$(VAR)` / `${VAR}` |
| 自动变量 | `$@`, `$<` | `$@`, `$<`, `$^`, `$?`, `$*` |
| 模式规则 | 支持 | 支持 |
| .PHONY | 支持 | 支持 |
| 函数调用 | 不支持 | 支持 |
| 条件语句 | 不支持 | 支持 |
| 并行构建 | 不支持 | `-j N` |
| include 指令 | 不支持 | 支持 |
| 隐式规则链 | 基本支持 | 完整支持 |

---

## 12. 限制与改进方向

### 当前限制

1. 命令不实际执行（仅打印模拟）
2. 不支持条件判断和分支
3. 不支持文件通配函数
4. 自动变量仅实现 `$@` 和 `$<`
5. 不支持多目标规则

### 改进方向

1. `system()` 调用执行真实命令
2. 支持 `$(wildcard ...)` 和 `$(patsubst ...)`
3. 支持 `include` 指令拆分 Makefile
4. 并行构建支持 (`-j` 选项)
5. 与任务调度器集成

---

> 参考 GNU Make Manual 4.2+ 实现核心解析与依赖跟踪逻辑
