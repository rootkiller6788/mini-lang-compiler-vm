# Knowledge Graph — mini-build-system

## 九层知识覆盖表 (L1-L9)

### L1: Definitions (Complete ✅)
| 定义 | 位置 | 类型 |
|------|------|------|
| DepNode, DepGraph | dep_graph.h | DAG 节点和图 |
| CacheEntry, BuildCache, TrackedFile, BuildTracker | incremental.h | 缓存和文件追踪 |
| MakeRule, MakeVariable, MakeTarget, MakeFile | make_engine.h | Make 规则/变量/目标 |
| NinjaNode, NinjaEdge, NinjaBuild | ninja_graph.h | Ninja 图结构 |
| BuildTask, TaskScheduler | task_scheduler.h | 任务和调度器 |
| TargetTriple, ToolSpec, ToolchainProfile | build_toolchain.h | 工具链定义 |
| BuildWorkspace, ProjectSpec, TargetSpec | build_manifest.h | 工作空间/项目/目标 |
| BuildEvent, BuildLogger | build_logger.h | 构建事件日志 |
| HashValue, ContentHash, HashCache, MerkleNode, MerkleTree | hash_cache.h | 内容寻址缓存 |
| FlowShopJob, WorkItem, WorkStealingPool, BuildFarm | scheduler_advanced.h | 高级调度 |
| CriticalPathSegment, BuildSpeedupResult | build_theorem.h | 性能分析 |

### L2: Core Concepts (Complete ✅)
| 概念 | 实现 |
|------|------|
| DAG 依赖图 | dep_graph.c — 节点/边模型 |
| 增量构建 | incremental.c — 缓存查找/存储/失效 |
| Make 规则求值 | make_engine.c — 目标/依赖/配方 |
| Ninja 显式图 | ninja_graph.c — 节点/边/脏标记传播 |
| 任务依赖调度 | task_scheduler.c — 任务状态机 |
| 交叉编译 | build_toolchain.c — tc_set_cross_target |
| Build Variants | build_toolchain.c — BUILD_DEBUG/RELEASE 等 |
| Workspace 管理 | build_manifest.c — 多项目容器 |
| 内容寻址存储 | hash_cache.c — 哈希键查找 |
| 工作窃取调度 | scheduler_advanced.c — 分布式负载均衡 |

### L3: Engineering Structures (Complete ✅)
| 工程结构 | 实现 |
|---------|------|
| 命令行生成 | build_toolchain.c — tc_compile/link/archive_cmd |
| 依赖解析 | build_manifest.c — manifest_resolve_deps |
| 结构化日志 | build_logger.c — 事件流/JSON 输出 |
| Work-Stealing Pool | scheduler_advanced.c — 双端队列模拟 |
| 构建场拓扑 | scheduler_advanced.c — 多机任务分配 |

### L4: Standards/Theorems (Complete ✅)
| 定理 | 公式 | 实现 |
|------|------|------|
| Amdahl's Law | Speedup = 1/(S + (1-S)/P) | build_theorem.c: amdahl_speedup |
| Gustafson's Law | Speedup = P - α(P-1) | build_theorem.c: gustafson_speedup |
| Makespan Lower Bound | T ≥ max(CPL, W/P) | build_theorem.c: makespan_lower_bound |
| Graham's List Bound | T ≤ (2-1/P)·T_opt | build_theorem.c: graham_list_bound |
| GNU Target Triple | machine-vendor-os-abi | build_toolchain.c: tc_parse_triple |
| Deterministic Build | reproducible outputs | build_theorem.c: verify_build_determinism |

### L5: Algorithms/Methods (Complete ✅)
| 算法 | 复杂度 | 实现 |
|------|--------|------|
| Kahn 拓扑排序 | O(V+E) | dep_graph.c: dep_topological_sort |
| Tarjan SCC | O(V+E) | dep_graph.c: dep_find_sccs |
| DFS 环检测 (三色标记) | O(V+E) | dep_graph.c: dep_detect_cycle |
| 关键路径 | O(V+E) | dep_graph.c: dep_compute_critical_path |
| Kahn 层级分配 | O(V+E) | dep_graph.c: dep_parallel_schedule |
| Johnson 2-Machine | O(n log n) | scheduler_advanced.c: johnson_2machine |
| SPT/LPT/EDD 调度 | O(n log n) | scheduler_advanced.c |
| Critical Ratio | O(n log n) | scheduler_advanced.c: sched_critical_ratio |
| Merkle Tree | O(n) | hash_cache.c: merkle_compute_root |
| djb2 Hash | O(n) | hash_cache.c: hash_string |
| Rolling Hash (Rabin-Karp) | O(n) | hash_cache.c: rh_update |
| Dominator Tree (CHK2001) | O(N²) | dep_graph.c: dep_compute_dominators |

### L6: Canonical Problems (Complete ✅)
| 问题 | 示例/实现 |
|------|----------|
| Makefile 解析与执行 | examples/make_parse_demo.c |
| Ninja 构建图模拟 | examples/ninja_demo.c |
| 依赖图求解器 | examples/dep_solver_demo.c |
| 多项目构建编排 | build_manifest.c + test |

### L7: Applications (Complete ✅)
| 应用 | 实现 |
|------|------|
| CI/CD 日志输出 | build_logger.c: blog_print_json |
| 主机工具链检测 | build_toolchain.c: tc_detect_host |
| 构建瓶颈分析 | build_theorem.c: analyze_build_bottleneck |

### L8: Advanced Topics (Complete ✅)
| 进阶主题 | 实现 |
|---------|------|
| Tarjan 强连通分量 | dep_graph.c: dep_find_sccs |
| 支配树 | dep_graph.c: dep_compute_dominators |
| Work-Stealing 调度 | scheduler_advanced.c: wsp_run |
| Merkle 树完整性 | hash_cache.c: merkle_verify |
| 滚动哈希 | hash_cache.c: RollingHash |
| 分布式构建场 | scheduler_advanced.c: bf_simulate |

### L9: Industry Frontiers (Partial ✅)
| 前沿主题 | 文档位置 |
|---------|---------|
| Bazel Skyframe 模型 | docs/build-system-architecture.md §4 |
| 远程执行 (REAPI) | docs/build-system-architecture.md §9 |
| 内容寻址存储 (CAS) | hash_cache.h 文档 |
| AI 辅助构建优化 | 待扩展 |
