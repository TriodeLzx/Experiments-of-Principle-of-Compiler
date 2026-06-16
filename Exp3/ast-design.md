# AST 节点设计

本文档描述实验三语法分析器输出的抽象语法树结构。语法分析命令为：

```bash
./compiler parse <源码文件>
```

顶层输出固定为一个 JSON 对象：

```json
{
  "ast": {},
  "errors": []
}
```

当不存在词法错误和语法错误时，`ast` 为 `Program` 节点，`errors` 为空数组。当存在任意错误时，`ast` 为 `null`，错误信息写入 `errors` 数组。

## 通用规则

所有 AST 节点都包含以下公共字段：

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 节点类型名称 |
| `line` | number | 节点起始 Token 的行号 |
| `col` | number | 节点起始 Token 的列号 |

本文档中使用以下类型别名：

| 名称 | 含义 |
|---|---|
| `Stmt` | 任意语句节点 |
| `Expr` | 任意表达式节点 |
| `Type` | 类型节点 |
| `Block` | 语句块节点 |

可选字段只在源码中确实出现对应结构时输出。例如没有参数的函数不输出 `params` 字段；没有初始化表达式的变量声明项不输出 `init` 字段。

## 顶层节点

### Program

表示整个程序。

| 字段 | 类型 | 说明 |
|---|---|---|
| `body` | array of `FuncDecl` or `Stmt` | 顶层函数定义或顶层语句列表 |

示例：

```json
{
  "type": "Program",
  "line": 1,
  "col": 1,
  "body": []
}
```

## 声明节点

### FuncDecl

表示函数定义。

| 字段 | 类型 | 说明 |
|---|---|---|
| `name` | string | 函数名 |
| `body` | `Block` | 函数体 |
| `returnType` | `Type` | 返回类型，可选 |
| `params` | array of `ParamDecl` | 参数列表，可选 |

对应源码形式：

```text
fn add(a:int, b:int) -> int:
    ret a + b
```

### ParamDecl

表示函数参数。

| 字段 | 类型 | 说明 |
|---|---|---|
| `name` | string | 参数名 |
| `paramType` | `Type` | 参数类型 |

### Type

表示类型名称。

| 字段 | 类型 | 说明 |
|---|---|---|
| `name` | string | 类型名 |

支持的基础类型为 `int`、`bool`、`float`、`str`。数组类型直接拼接 `[]`，例如 `int[]`、`int[][]`。

## 语句节点

### Block

表示语句块。函数体、`if` 分支、`while` 循环体和 `for` 循环体都会统一输出为 `Block`。

| 字段 | 类型 | 说明 |
|---|---|---|
| `stmts` | array of `Stmt` | 块内语句列表 |

### VarDecl

表示变量声明语句。

| 字段 | 类型 | 说明 |
|---|---|---|
| `varType` | `Type` | 变量类型 |
| `declarators` | array of `VarDeclarator` | 声明项列表 |

对应源码形式：

```text
int a = 1, b = 2
```

### VarDeclarator

表示单个变量声明项。

| 字段 | 类型 | 说明 |
|---|---|---|
| `name` | string | 变量名 |
| `init` | `Expr` | 初始化表达式，可选 |

### AssignStmt

表示赋值语句。

| 字段 | 类型 | 说明 |
|---|---|---|
| `target` | `Expr` | 赋值目标，通常为 `Identifier` 或 `IndexExpr` |
| `value` | `Expr` | 赋值表达式 |

对应源码形式：

```text
x = 1
nums[i] = nums[i] * 2
```

### ReturnStmt

表示返回语句。

| 字段 | 类型 | 说明 |
|---|---|---|
| `value` | `Expr` | 返回值表达式，可选 |

对应源码形式：

```text
ret x
ret
```

### ExprStmt

表示表达式语句。

| 字段 | 类型 | 说明 |
|---|---|---|
| `expr` | `Expr` | 表达式 |

### IfStmt

表示条件语句。

| 字段 | 类型 | 说明 |
|---|---|---|
| `cond` | `Expr` | `if` 条件 |
| `then` | `Block` | `if` 分支 |
| `elif` | array of `ElifClause` | `elif` 分支列表，可选 |
| `elseBody` | `Block` | `else` 分支，可选 |

### ElifClause

表示单个 `elif` 分支。

| 字段 | 类型 | 说明 |
|---|---|---|
| `cond` | `Expr` | `elif` 条件 |
| `body` | `Block` | `elif` 分支语句块 |

### WhileStmt

表示 `while` 循环。

| 字段 | 类型 | 说明 |
|---|---|---|
| `body` | `Block` | 循环体 |
| `cond` | `Expr` | 循环条件 |

### ForStmt

表示 `for` 循环。

| 字段 | 类型 | 说明 |
|---|---|---|
| `var` | string | 循环变量名 |
| `body` | `Block` | 循环体 |
| `end` | `Expr` | 结束表达式 |
| `start` | `Expr` | 起始表达式 |
| `step` | `Expr` | 步长表达式，可选 |

对应源码形式：

```text
for i = 0, 10:
    ret i

for i = 0, 10, 1:
    ret i
```

## 表达式节点

### Identifier

表示标识符。

| 字段 | 类型 | 说明 |
|---|---|---|
| `name` | string | 标识符名称 |

### IntLiteral

表示整数字面量。

| 字段 | 类型 | 说明 |
|---|---|---|
| `value` | number | 整数值 |

### FloatLiteral

表示浮点数字面量。

| 字段 | 类型 | 说明 |
|---|---|---|
| `value` | number | 浮点数值 |

### StringLiteral

表示字符串字面量。

| 字段 | 类型 | 说明 |
|---|---|---|
| `value` | string | 字符串内容，不包含外层双引号 |

### BoolLiteral

表示布尔字面量。

| 字段 | 类型 | 说明 |
|---|---|---|
| `value` | boolean | 布尔值 |

### BinaryExpr

表示二元表达式。节点位置使用运算符 Token 的位置。

| 字段 | 类型 | 说明 |
|---|---|---|
| `op` | string | 二元运算符 |
| `left` | `Expr` | 左操作数 |
| `right` | `Expr` | 右操作数 |

支持的二元运算符：

```text
or and | ^ & == != < > <= >= << >> + - * / %
```

### UnaryExpr

表示一元表达式。节点位置使用一元运算符 Token 的位置。

| 字段 | 类型 | 说明 |
|---|---|---|
| `op` | string | 一元运算符 |
| `expr` | `Expr` | 操作数 |

支持的一元运算符：

```text
+ - ~ not
```

### Call

表示函数调用或类型转换调用。

| 字段 | 类型 | 说明 |
|---|---|---|
| `callee` | `Expr` | 被调用对象 |
| `args` | array of `Expr` | 实参列表 |

示例：

```text
add(1, 2)
int(x)
```

### IndexExpr

表示数组下标访问。

| 字段 | 类型 | 说明 |
|---|---|---|
| `array` | `Expr` | 被访问的数组表达式 |
| `index` | `Expr` | 下标表达式 |

连续下标会嵌套生成 `IndexExpr`，例如 `mat[0][1]` 等价于先访问 `mat[0]`，再对结果访问 `[1]`。

### ArrayLiteral

表示数组字面量。

| 字段 | 类型 | 说明 |
|---|---|---|
| `elements` | array of `Expr` | 元素表达式列表 |

示例：

```text
[1, 2, 3]
[[1, 2], [3, 4]]
```

## 表达式优先级与结合性

表达式按优先级分层解析。优先级从低到高为：

```text
or
and
|
^
&
== !=
< > <= >=
<< >>
+ -
* / %
一元运算符: + - ~ not
后缀: 调用，数组索引
基本变量: 字面量、标识符、数组字面量、括号表达式
```

同一优先级的二元运算符按左结合生成 AST。例如：

```text
a - b - c
```

生成结构等价于：

```text
(a - b) - c
```

括号会改变优先级。例如：

```text
(a + b) * c
```

会生成以 `*` 为根节点、左操作数为 `a + b` 的表达式树。

## 错误输出

语法分析器不在最终输出中保留错误占位 AST。只要出现词法错误或语法错误，顶层 `ast` 字段为 `null`，`errors` 数组保存错误详情。

语法错误格式：

```json
{
  "type": "SYNTAX_ERROR",
  "expected": ")",
  "found": "ret",
  "line": 3,
  "col": 2,
  "message": "expected ')' before 'ret'"
}
```

词法错误格式：

```json
{
  "type": "LEX_ERROR",
  "kind": "ILLEGAL_CHAR",
  "found": "@",
  "line": 4,
  "col": 12,
  "message": "lexical error: ILLEGAL_CHAR"
}
```

错误恢复采用行级同步策略：一条语句解析后，如果同一行仍有多余 Token，则报告 `expected 'end of statement'` 并跳过该行剩余 Token，随后继续解析下一行。缩进回退也会作为块结构的同步点。

## 示例

输入：

```text
fn main() -> int:
    int x = 1 + 2 * 3
    ret x
```

输出中的 AST 结构摘要：

```json
{
  "type": "Program",
  "body": [
    {
      "type": "FuncDecl",
      "name": "main",
      "returnType": {
        "type": "Type",
        "name": "int"
      },
      "body": {
        "type": "Block",
        "stmts": [
          {
            "type": "VarDecl",
            "declarators": [
              {
                "type": "VarDeclarator",
                "name": "x",
                "init": {
                  "type": "BinaryExpr",
                  "op": "+",
                  "right": {
                    "type": "BinaryExpr",
                    "op": "*"
                  }
                }
              }
            ]
          },
          {
            "type": "ReturnStmt",
            "value": {
              "type": "Identifier",
              "name": "x"
            }
          }
        ]
      }
    }
  ]
}
```
