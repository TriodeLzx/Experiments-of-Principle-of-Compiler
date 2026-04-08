# 编译原理实验一：文法分析器 (Grammar Analyzer)

本项目是文法分析器的 C++ 实现。程序能够读取标准格式的文法文件，进行格式验证、Chomsky 文法类型判定以及提取相关的文法统计信息。

## 环境依赖
* **操作系统**：Linux (或兼容 UNIX 的环境)
* **编译器**：GCC (要求支持 C++11 或以上标准)

## 构建说明 (Build)
使用 `g++` 编译源代码文件：

```bash
g++ -std=c++11 grammar_analyzer.cpp -o grammar_analyzer
```

编译成功后，将在当前目录下生成可执行文件 `grammar_analyzer`。

## 运行说明 (Run)
命令行的基本调用格式为：
```bash
./grammar_analyzer <input_file.txt> <mode> [--max-length <len>]
```

程序支持以下四种运行模式：

### (1) 文法类型判定 `--check-type`
判定输入文法的 Chomsky 层次结构类型，输出分类结果 (包含：`TYPE_3_RIGHT`, `TYPE_3_LEFT`, `TYPE_2`, `TYPE_1`, `TYPE_0`, `INVALID`)：
```bash
./grammar_analyzer tests/test1_type3_right.txt --check-type
# 输出示例: TYPE_3_RIGHT
```

六种分类结果含义如下：

|输出|含义|
|-----|-----|
|`TYPE_3_RIGHT`|右线性正则文法|
|`TYPE_3_LEFT`|左线性正则文法|
|`TYPE_2`|上下文无关文法（但不是正则文法）|
|`TYPE_1`|上下文相关文法|
|`TYPE_0`|无限制文法|
|`INVALID`|文法不合法|


### (2) 文法合法性验证 `--validate`
验证输入文法是否合法，并以 JSON 格式输出结果。支持检测格式错误、未定义符号 (UNDEFINED_SYMBOL)、不可达符号 (UNREACHABLE)、不可推导/生成符号 (NON_GENERATING) 等语义边界错误：
```bash
./grammar_analyzer tests/test7_invalid_unreachable.txt --validate
# 输出示例: 
# {
#   "valid": false,
#   "errors": [
#     {"type": "UNREACHABLE", "symbol": "B"}
#   ]
# }
```

### (3) 文法统计信息 `--stats`
统计文法的非终结符数目、终结符数目、产生式数目、是否包含空产生式 (`ε`) 及开始符号，以 JSON 格式输出：
```bash
./grammar_analyzer tests/test10_stats.txt --stats
# 输出示例:
# {
#   "num_nonterminals": 4,
#   "num_terminals": 3,
#   "num_productions": 6,
#   "has_epsilon": true,
#   "start_symbol": "S"
# }
```

### (4) 二义性检测 `--ambiguity-check`
基于严格的最左推导（BFS），检查 CFG 文法在指定推导长度内的二义性。可以通过 `--max-length` 指定最大的推导终结符长度（默认为 8）。

```bash
./grammar_analyzer tests/test_ambiguous.txt --ambiguity-check --max-length 5
# 输出示例 (二义性文法):
# {"ambiguous": true, "witness": "n + n + n", "parse_count": 2}

./grammar_analyzer tests/test_non_ambiguous.txt --ambiguity-check
# 输出示例 (无二义性文法):
# {"ambiguous": false, "max_length_checked": 8}
```
