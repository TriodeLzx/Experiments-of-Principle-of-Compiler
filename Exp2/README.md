# 编译原理实验二：词法分析

本项目是词法分析器的C++实现，实现能够读取标准格式的词法文件，并输出Token序列

## 环境依赖
* **操作系统**：Linux (或兼容 UNIX 的环境)
* **编译器**：GCC (要求支持 C++11 或以上标准)

## 构建说明 (build)
使用 `g++` 编译源代码文件：

```bash
g++ -std=c++11 compiler.cpp -o compiler
```

编译成功后，将在当前目录下生成可执行文件 `compiler`。

## 运行说明 (run)
命令行的基本调用格式：

```bash
./compiler tokenize <input_file>
```

