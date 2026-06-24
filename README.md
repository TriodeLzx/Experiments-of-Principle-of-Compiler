# 编译原理实验项目

本仓库是深圳大学《编译原理》课程实验项目，根目录提供统一编译器入口 `compiler.cpp`。

## 编译

```bash
g++ -std=c++11 compiler.cpp -o compiler
```

编译完成后会生成可执行文件 `compiler`。

## 用法

命令格式：

```bash
./compiler <tokenize|parse|check|codegen|run> <input_file> [--optimize]
```

说明：

- `tokenize`：输出词法分析结果
- `parse`：输出语法分析结果
- `check`：输出语义检查结果
- `codegen`：生成中间代码
- `run`：运行程序
- `--optimize`：可选，仅用于 `codegen` 和 `run`，表示先做优化再输出或执行

## 示例

```bash
./compiler tokenize Exp5/tests/test_01.src
./compiler parse Exp5/tests/test_01.src
./compiler check Exp5/tests/test_01.src
./compiler codegen Exp5/tests/test_01.src
./compiler codegen Exp5/tests/test_01.src --optimize
./compiler run Exp5/tests/test_01.src
./compiler run Exp5/tests/test_01.src --optimize
```

也可以直接运行场景样例：

```bash
./compiler codegen Exp5/scenarios/S1.tlang
./compiler codegen Exp5/scenarios/S1.tlang --optimize
./compiler run Exp5/scenarios/S1.tlang
```

## 目录

- `Exp2/`：词法分析
- `Exp3/`：语法分析
- `Exp4/`：语义分析
- `Exp5/`：统一编译器入口与中间代码生成/优化/执行

如果只关心程序怎么用，直接在仓库根目录编译 `compiler.cpp`，然后运行上面的命令即可。
