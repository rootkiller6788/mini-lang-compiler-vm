# mini-build-system — 构建系统 (C 语言实现)

> 参考 GNU Make, Ninja, Buck, Bazel

---

## 模块一览

| # | 模块 | 头文件 | 源文件 | 功能 |
|---|------|--------|--------|------|
| 1 | **Make引擎** | `make_engine.h` | `src/make_engine.c` | Makefile解析、规则求值、变量展开、模式匹配、自动变量 |
| 2 | **Ninja图** | `ninja_graph.h` | `src/ninja_graph.c` | .ninja解析、依赖图、脏标记传播、关键路径调度 |
| 3 | **依赖图** | `dep_graph.h` | `src/dep_graph.c` | DAG拓扑排序、环检测、并行调度分层、关键路径计算 |
| 4 | **增量缓存** | `incremental.h` | `src/incremental.c` | 内容寻址缓存、文件变更追踪、读写/失效 |
| 5 | **任务调度** | `task_scheduler.h` | `src/task_scheduler.c` | 任务状态机、依赖计数、串行/并行模拟调度 |

---

## 快速开始

### 构建

```bash
make          # 编译所有示例到 bin/
make clean    # 清理构建产物
```

### 运行示例

```bash
bin/make_parse_demo      # Make规则解析与构建演示
bin/ninja_demo           # Ninja构建图演示
bin/dep_solver_demo      # 依赖图求解器演示
```

---

## 目录结构

```
mini-build-system/
├── include/
│   ├── make_engine.h         # Make规则引擎
│   ├── ninja_graph.h         # Ninja构建图
│   ├── dep_graph.h           # 依赖图求解器
│   ├── incremental.h         # 增量构建与缓存
│   └── task_scheduler.h      # 任务调度器
├── src/
│   ├── make_engine.c
│   ├── ninja_graph.c
│   ├── dep_graph.c
│   ├── incremental.c
│   └── task_scheduler.c
├── examples/
│   ├── make_parse_demo.c     # Make演示
│   ├── ninja_demo.c          # Ninja演示
│   └── dep_solver_demo.c     # 依赖图演示
├── demos/
│   ├── mini-make-engine/
│   │   └── README.md         # Make引擎详细文档
│   └── mini-ninja-build/
│       └── README.md         # Ninja构建详细文档
├── docs/
│   ├── course-alignment.md   # 课程概念映射
│   └── build-system-architecture.md
├── Makefile
└── README.md
```

---

## 设计理念

### 分层架构

```
            ┌─────────────┐
            │  解析层      │  make_engine, ninja_graph
            ├─────────────┤
            │  图分析层    │  dep_graph
            ├─────────────┤
            │  缓存层      │  incremental
            ├─────────────┤
            │  执行层      │  task_scheduler
            └─────────────┘
```

### 核心数据结构

| 结构 | 所属 | 用途 |
|------|------|------|
| `MakeRule` / `MakeFile` | make_engine | Make风格的规则描述 |
| `NinjaNode` / `NinjaEdge` | ninja_graph | Ninja风格的显式图 |
| `DepNode` / `DepGraph` | dep_graph | 通用依赖DAG |
| `CacheEntry` / `BuildCache` | incremental | 内容寻址构建缓存 |
| `BuildTask` / `TaskScheduler` | task_scheduler | 并行任务调度 |

---

## Make引擎示例

```c
#include "make_engine.h"

MakeFile mf;
make_parse(&mf, "Makefile");

// 解析后:
//   hello -> [hello.o, main.o]
//   hello.o -> [hello.c, hello.h]
//   main.o -> [main.c]

make_resolve_vars(&mf);      // 展开 $(CC), $(CFLAGS)
make_print_graph(&mf);       // 打印依赖图
make_build(&mf, "hello");    // 拓扑构建
```

### Makefile语法

```make
CC = gcc
CFLAGS = -Wall -O2

hello: hello.o main.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f *.o hello
```

---

## Ninja图示例

```c
#include "ninja_graph.h"

NinjaBuild nb;
ninja_parse(&nb, "build.ninja");
ninja_compute_dirty(&nb);    // 脏标记传播
ninja_schedule(&nb);          // 关键路径调度
ninja_execute(&nb, "hello"); // 模拟构建
ninja_print_graph(&nb);
```

### build.ninja语法

```ninja
cxx = g++
rule cxx
  command = $cxx -c $in -o $out
build hello.obj: cxx hello.cpp | hello.h
build hello: link hello.obj main.obj
default hello
```

---

## 依赖图示例

```c
#include "dep_graph.h"

DepGraph dg;
dep_add_node(&dg, "source");
dep_add_node(&dg, "compile");
dep_add_node(&dg, "link");
dep_add_edge_by_name(&dg, "source", "compile");
dep_add_edge_by_name(&dg, "compile", "link");

// 拓扑排序
int order[256], len;
dep_topological_sort(&dg, order, &len);

// 环检测
if (dep_detect_cycle(&dg)) {
    printf("Cycle found!\n");
}

// 并行调度 (2个层级可并行)
int schedule[256], num_levels;
dep_parallel_schedule(&dg, schedule, &num_levels);
```

---

## 增量缓存示例

```c
#include "incremental.h"

BuildCache cache;
cache_init(&cache, ".build-cache");

char key[64];
cache_compute_key("abc123", "gcc -c foo.c", key, 64);

char outputs[16][256];
int num_out;
if (cache_lookup(&cache, key, outputs, &num_out)) {
    printf("Cache hit!\n");
} else {
    // Perform build...
    const char *built[] = {"foo.o"};
    cache_store(&cache, key, built, 1);
}
```

---

## 任务调度示例

```c
#include "task_scheduler.h"

TaskScheduler ts;
sched_init(&ts, 4);  // max 4 parallel

int t0 = sched_add_task(&ts, "gcc -c a.c -o a.o");
int t1 = sched_add_task(&ts, "gcc -c b.c -o b.o");
int t2 = sched_add_task(&ts, "gcc -o prog a.o b.o");
sched_add_dependency(&ts, t2, t0);
sched_add_dependency(&ts, t2, t1);

sched_run_serial(&ts);    // Sequential: 300ms
sched_run_parallel(&ts);  // Parallel: 200ms (2x speedup)
```

---

## API约定

- **命名**: `snake_case` 函数, `PascalCase` 类型, `UPPER_SNAKE_CASE` 常量
- **头文件**: `#ifndef X_H` / `#define X_H` / `#endif`
- **标准**: C99, libc + libm
- **布尔**: 所有头文件 `#include <stdbool.h>`

---

## 许可

Educational use.
