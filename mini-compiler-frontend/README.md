# Mini Compiler Frontend — 迷你编译器前端

> 参考 Stanford CS143, Crafting Interpreters, Dragon Book

一个手写的 C 语言子集编译器前端，包含词法分析、语法分析、AST 构建、符号表管理和语义检查五个模块。

## 模块总览

| 模块 | 头文件 | 源文件 | 功能 |
|------|--------|--------|------|
| **词法分析** | `include/lexer.h` | `src/lexer.c` | 字符流 → Token 流：关键字、标识符、字面量、操作符、注释处理 |
| **语法分析** | `include/parser.h` | `src/parser.c` | Token 流 → AST：递归下降解析，运算符优先级，LL(1) 预测分析 |
| **AST 树** | `include/ast.h` | `src/ast.c` | 抽象语法树构建、遍历（前序/后序）、访问者模式、打印 |
| **符号表** | `include/symtab.h` | `src/symtab.c` | 哈希表 + 链表实现，嵌套作用域，插入/查找/作用域进出 |
| **语义分析** | `include/semantic.h` | `src/semantic.c` | 标识符解析、类型检查、返回路径检查、main 函数检查 |

## 快速开始

```
make                    # 编译所有示例
make run-lexer          # 词法分析演示
make run-parser         # 语法分析演示
make run-semantic       # 语义分析演示
make clean              # 清理生成文件
```

## 语言子集

```
program     → function*
function    → 'int' IDENT '(' params? ')' block
block       → '{' statement* '}'
statement   → if | while | return | var_decl | block | expr ';'
if_stmt     → 'if' '(' expr ')' statement
while_stmt  → 'while' '(' expr ')' statement
return_stmt → 'return' expr? ';'
var_decl    → 'int' IDENT ';'
expr        → assignment
assignment  → logical_or ('=' assignment)?
logical_or  → logical_and ('||' logical_and)*
logical_and → equality ('&&' equality)*
equality    → relational (('==' | '!=') relational)*
relational  → additive (('<' | '>' | '<=' | '>=') additive)*
additive    → multiplicative (('+' | '-') multiplicative)*
multiplicative → unary (('*' | '/') unary)*
unary       → ('-' | '!') unary | primary
primary     → INT_LIT | STRING | IDENT ('(' args? ')')? | '(' expr ')'
```

## Token 类型

| Token | 值 | 说明 |
|-------|-----|------|
| TOK_INT | `int` | 类型关键字 |
| TOK_IF | `if` | 条件语句 |
| TOK_WHILE | `while` | 循环语句 |
| TOK_RETURN | `return` | 返回语句 |
| TOK_IDENT | 标识符 | 变量/函数名 |
| TOK_INT_LIT | 数字 | 整数字面量 |
| TOK_STRING | 字符串 | 字符串字面量 |
| TOK_PLUS / TOK_MINUS / TOK_STAR / TOK_SLASH | `+` `-` `*` `/` | 算术操作符 |
| TOK_EQ | `=` | 赋值 |
| TOK_EQ_EQ / TOK_NEQ | `==` `!=` | 相等比较 |
| TOK_LT / TOK_GT / TOK_LE / TOK_GE | `<` `>` `<=` `>=` | 关系比较 |
| TOK_AND / TOK_OR / TOK_NOT | `&&` `\|\|` `!` | 逻辑操作符 |
| TOK_LPAREN / TOK_RPAREN | `(` `)` | 括号 |
| TOK_LBRACE / TOK_RBRACE | `{` `}` | 大括号 |
| TOK_SEMI | `;` | 语句结束 |
| TOK_COMMA | `,` | 参数分隔 |
| TOK_EOF | - | 文件结束 |
| TOK_ERROR | - | 词法错误 |

## AST 节点类型

| 节点类型 | 说明 | 子节点 |
|---------|------|--------|
| AST_PROGRAM | 程序根节点 | function* |
| AST_FUNC_DEF | 函数定义 | param*, block |
| AST_BLOCK | 代码块 | statement* |
| AST_IF_STMT | if 语句 | cond, then, [else] |
| AST_WHILE_STMT | while 循环 | cond, body |
| AST_RETURN_STMT | return 语句 | [expr] |
| AST_BINARY_OP | 二元运算 | left, right |
| AST_UNARY_OP | 一元运算 | operand |
| AST_INT_LIT | 整数字面量 | - |
| AST_IDENT | 标识符引用 | - |
| AST_ASSIGN | 赋值 | target, value |
| AST_CALL | 函数调用 | args* |
| AST_PARAM | 参数声明 | - |
| AST_VAR_DECL | 变量声明 | - |
| AST_STRING_LIT | 字符串字面量 | - |

## 运算符优先级

| 优先级 | 操作符 | 结合性 |
|--------|--------|--------|
| 0 (最低) | `=` | 右结合 |
| 1 | `\|\|` | 左结合 |
| 2 | `&&` | 左结合 |
| 3 | `==` `!=` | 左结合 |
| 4 | `<` `>` `<=` `>=` | 左结合 |
| 5 | `+` `-` | 左结合 |
| 6 | `*` `/` | 左结合 |
| 7 | `-` `!` (一元) | 右结合 |
| 8 (最高) | 字面量, `()`, 调用 | - |

## 语义检查项目

1. **声明检查**: 所有使用的标识符必须声明
2. **重复声明**: 同一作用域内不允许重复定义
3. **类型检查**: 表达式类型一致性（int-only）
4. **函数调用**: 调用的目标必须是已声明的函数
5. **main 函数**: 程序必须包含 `main()` 函数
6. **返回路径**: 有返回值的函数必须在所有路径上返回

## 项目结构

```
mini-compiler-frontend/
├── include/
│   ├── lexer.h          # 词法分析器
│   ├── parser.h         # 递归下降解析器
│   ├── ast.h            # 抽象语法树
│   ├── symtab.h         # 符号表
│   └── semantic.h       # 语义分析
├── src/
│   ├── lexer.c          # 词法分析实现 (~300 行)
│   ├── parser.c         # 解析器实现 (~300 行)
│   ├── ast.c            # AST 实现 (~110 行)
│   ├── symtab.c         # 符号表实现 (~105 行)
│   └── semantic.c       # 语义分析实现 (~250 行)
├── examples/
│   ├── lexer_demo.c     # 词法分析演示
│   ├── parser_demo.c    # 语法分析演示
│   └── semantic_demo.c  # 语义分析演示
├── demos/
│   ├── mini-c-frontend/
│   │   └── README.md    # 前端完整走读
│   └── mini-recursive-descent/
│       └── README.md    # 递归下降深度解析
├── docs/
│   ├── course-alignment.md       # 课程对照
│   └── compiler-frontend-primer.md # 前端入门指南
├── Makefile
└── README.md
```

## 设计原则

- **C99 标准**: 仅使用 C99 标准库 (libc + libm)，无外部依赖
- **单遍词法**: 字符流一次遍历，按需生成 Token
- **LL(1) 递归下降**: 一个前瞻 Token，无回溯
- **不变式 AST**: 语义分析只读 AST，不修改结构
- **错误恢复**: 解析遇到错误后继续，尽可能多报告问题
