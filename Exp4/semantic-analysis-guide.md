# 语义分析流程说明

本文结合 [compiler.cpp](/home/triode/Homework/Compilers/Exp4/compiler.cpp) 的实现，详细说明本实验中语义分析器的工作流程。写法采用“解释 + 对应代码段”的形式，方便直接对照源码阅读，也方便后续写实验报告。

## 1. 整体位置与执行入口

本项目的语义分析不是改写原有语法分析器，而是在“词法分析 -> 语法分析 -> AST 构建”之后，新增一个独立阶段。这样做的好处是：

- 原有 `tokenize` 和 `parse` 功能几乎不受影响
- 语义分析可以直接基于 AST 遍历实现
- 结构更清晰，便于后续继续扩展中间代码生成

真正触发语义分析的是 `check` 命令。程序先完成词法和语法分析；只有当前两步都没有错误时，才会创建 `SemanticAnalyzer` 并分析 AST。

```cpp
int main(int argc, char* argv[]) {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	if (argc != 3 || (string(argv[1]) != "tokenize" && string(argv[1]) != "parse" && string(argv[1]) != "check")) {
		cerr << "Usage: ./compiler <tokenize|parse|check> <input_file>\n";
		return 1;
	}

	ifstream input(argv[2], ios::in | ios::binary);
	if (!input) {
		cerr << "Failed to open input file: " << argv[2] << '\n';
		return 1;
	}

	string source((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
	Lexer lexer(move(source));
	lexer.run();
	if (string(argv[1]) == "tokenize") {
		printJson(lexer.getTokens(), lexer.getErrors());
	} else if (string(argv[1]) == "parse") {
		Parser parser(lexer.getTokens());
		Json ast = parser.parseProgram();
		printParseJson(ast, lexer.getErrors(), parser.getErrors());
	} else {
		Parser parser(lexer.getTokens());
		Json ast = parser.parseProgram();
		if (!lexer.getErrors().empty() || !parser.getErrors().empty()) {
			vector<SemanticError> semanticErrors;
			vector<SymbolRecord> symbols;
			printCheckJson(lexer.getErrors(), parser.getErrors(), semanticErrors, symbols);
		} else {
			SemanticAnalyzer analyzer;
			analyzer.analyze(ast);
			printCheckJson(lexer.getErrors(), parser.getErrors(), analyzer.getErrors(), analyzer.getSymbols());
		}
	}
	return 0;
}
```

这里可以看出，语义分析器的输入是 AST，输出是两部分：

- `errors`：语义错误列表
- `symbols`：符号表信息

## 2. 为什么先写 AST 访问辅助函数

语法分析器构造出来的 AST 用的是自定义 `Json` 结构，而不是单独定义很多 `ASTNode` 子类。因此在语义分析阶段，首先要解决一个问题：怎样方便地从 `Json` 里取出字段。

为此，代码先实现了一组轻量辅助函数：

- `jsonField`：按字段名取出对象成员
- `jsonStringField`：读取字符串字段
- `jsonIntField`：读取整数字段
- `jsonArrayField`：读取数组字段
- `jsonNodeType/jsonNodeLine/jsonNodeCol`：快速获取节点类型和位置信息

```cpp
static const Json* jsonField(const Json& object, const string& key) {
	for (size_t i = 0; i < object.objectItems.size(); ++i) {
		if (object.objectItems[i].first == key) {
			return &object.objectItems[i].second;
		}
	}
	return nullptr;
}

static string jsonStringField(const Json& object, const string& key, const string& fallback = "") {
	const Json* value = jsonField(object, key);
	if (!value) {
		return fallback;
	}
	if (value->kind == Json::STRING || value->kind == Json::NUMBER) {
		return value->text;
	}
	if (value->kind == Json::BOOL) {
		return value->boolValue ? "true" : "false";
	}
	return fallback;
}

static const vector<Json>& jsonArrayField(const Json& object, const string& key) {
	static const vector<Json> empty;
	const Json* value = jsonField(object, key);
	if (!value || value->kind != Json::ARRAY) {
		return empty;
	}
	return value->arrayItems;
}
```

这一层的作用很重要：它把“手工遍历 JSON 结构”的复杂性收口到少数几个函数里。后面的语义分析逻辑就可以像操作普通 AST 一样，直接关注节点内容。

## 3. 语义分析器内部维护了哪些信息

语义分析不仅要找错，还要记住“当前处于哪个作用域”“这个名字声明过没有”“它是什么类型”。因此，代码定义了几组核心结构。

### 3.1 语义错误结构

`SemanticError` 用来统一保存语义错误。不同类型的错误需要的字段不同，所以这里用了多个布尔值标记某些字段是否有效。

```cpp
struct SemanticError {
	string type;
	string symbol;
	string expected;
	string found;
	int expectedCount;
	int foundCount;
	int line;
	int col;
	bool hasSymbol;
	bool hasExpected;
	bool hasFound;
	bool hasExpectedCount;
	bool hasFoundCount;
};
```

这样设计的好处是，所有语义错误都可以共用一个数组保存，最后统一输出为 JSON。

### 3.2 符号表项结构

代码里区分了两层信息：

- `SymbolEntry`：分析期间内部使用，字段更完整
- `SymbolRecord`：最终输出给用户的精简符号表记录

```cpp
struct SymbolRecord {
	string name;
	string type;
	string scope;
	int line;
	int col;
};

struct SymbolEntry {
	string name;
	string type;
	string scope;
	string returnType;
	vector<string> paramTypes;
	int line;
	int col;
	bool isFunction;
	bool builtin;
};
```

其中：

- 变量主要使用 `name/type/scope`
- 函数额外需要 `returnType/paramTypes`
- `isFunction` 用来区分“变量”和“函数”
- `builtin` 用来区分用户定义符号和内建转换函数

### 3.3 作用域结构

作用域本质上是一个链式环境。每个作用域都有：

- 自己的名字
- 父作用域指针
- 当前作用域里的符号表

```cpp
struct Scope {
	string name;
	Scope* parent;
	map<string, SymbolEntry> symbols;
};
```

这意味着名字查找时可以从当前作用域开始，逐层向外找；而重复声明只需要检查当前作用域。

## 4. 初始化阶段：建立全局环境和内建函数

`SemanticAnalyzer` 的构造函数先创建全局作用域，然后把四个内建类型转换函数加入全局符号表：

- `int`
- `float`
- `bool`
- `str`

```cpp
SemanticAnalyzer() {
	globalScope = createScope("global", nullptr);
	currentScope = globalScope;
	declareBuiltin("int", "int");
	declareBuiltin("float", "float");
	declareBuiltin("bool", "bool");
	declareBuiltin("str", "str");
}
```

对应的内建声明函数如下：

```cpp
void declareBuiltin(const string& name, const string& returnType) {
	SymbolEntry entry;
	entry.name = name;
	entry.type = returnType + "(scalar)";
	entry.scope = "global";
	entry.returnType = returnType;
	entry.paramTypes.push_back("scalar");
	entry.line = 0;
	entry.col = 0;
	entry.isFunction = true;
	entry.builtin = true;
	globalScope->symbols[name] = entry;
}
```

这里的意思是：例如 `int(x)` 被视为一个返回 `int`、接收一个标量参数的函数。这样语义分析阶段就能直接复用普通函数调用检查逻辑。

## 5. 总控制流程：为什么要“先预声明函数，再分析函数体”

整个语义分析的入口是 `analyze`：

```cpp
void analyze(const Json& program) {
	const vector<Json>& body = jsonArrayField(program, "body");
	for (size_t i = 0; i < body.size(); ++i) {
		if (jsonNodeType(body[i]) == "FuncDecl") {
			predeclareFunction(body[i]);
		}
	}

	for (size_t i = 0; i < body.size(); ++i) {
		if (jsonNodeType(body[i]) == "FuncDecl") {
			analyzeFunction(body[i]);
		} else {
			analyzeStatement(body[i]);
		}
	}

	stable_sort(records.begin(), records.end(), symbolLess);
}
```

这里分成了两个阶段。

### 第一阶段：函数预声明

先扫描一遍所有顶层 `FuncDecl`，把函数名、返回类型、参数类型记入全局符号表。这能解决一个常见问题：函数可以在其定义之前被调用。

```cpp
void predeclareFunction(const Json& func) {
	SymbolEntry entry;
	entry.name = jsonStringField(func, "name");
	entry.scope = "global";
	entry.returnType = jsonHasField(func, "returnType")
		? jsonStringField(*jsonField(func, "returnType"), "name", "void")
		: "void";
	entry.line = jsonNodeLine(func);
	entry.col = jsonNodeCol(func);
	entry.isFunction = true;
	entry.builtin = false;

	const vector<Json>& params = jsonArrayField(func, "params");
	for (size_t i = 0; i < params.size(); ++i) {
		const Json* paramType = jsonField(params[i], "paramType");
		entry.paramTypes.push_back(paramType ? jsonStringField(*paramType, "name") : "<unknown>");
	}
	entry.type = functionSignature(entry.returnType, entry.paramTypes);

	Scope* saved = currentScope;
	currentScope = globalScope;
	declareSymbol(entry);
	currentScope = saved;
}
```

### 第二阶段：深入分析函数体和语句

在所有函数都已经登记后，再逐个分析函数体、变量声明、表达式和控制流。这种“先建接口，再查实现”的顺序是语义分析中很常见的做法。

## 6. 符号表管理：声明、查找、重复定义检测

### 6.1 创建新作用域

每进入一个函数、`if`、`while`、`for` 或额外代码块，都可能需要创建一个新作用域。代码通过 `createScope` 和 `makeScopeName` 完成这件事。

```cpp
Scope* createScope(const string& name, Scope* parent) {
	Scope* scope = new Scope();
	scope->name = name;
	scope->parent = parent;
	ownedScopes.push_back(scope);
	return scope;
}

string makeScopeName(const string& prefix, const Json& node) {
	ostringstream out;
	if (!currentFunctionName.empty()) {
		out << currentFunctionName << "::";
	} else {
		out << "global::";
	}
	out << prefix << "@" << jsonNodeLine(node) << "_" << jsonNodeCol(node) << "_" << ++scopeCounter;
	return out.str();
}
```

生成带位置信息的作用域名有两个好处：

- 输出的符号表可读性更强
- 即使多个同类块同时存在，也不会重名

### 6.2 向当前作用域登记符号

```cpp
bool declareSymbol(const SymbolEntry& entry) {
	if (existsInCurrentScope(entry.name)) {
		addRedeclared(entry.name, entry.line, entry.col);
		return false;
	}
	currentScope->symbols[entry.name] = entry;
	if (!entry.builtin) {
		records.push_back(SymbolRecord{entry.name, entry.type, entry.scope, entry.line, entry.col});
	}
	return true;
}
```

逻辑很直接：

1. 先只检查当前作用域中有没有同名符号
2. 如果有，报 `REDECLARED`
3. 如果没有，则登记进当前作用域

注意这里只检查“当前作用域”，不检查父作用域。这正是变量遮蔽能够成立的原因：内层作用域允许声明与外层同名的变量。

### 6.3 按作用域链查找符号

```cpp
const SymbolEntry* lookup(const string& name) const {
	for (Scope* scope = currentScope; scope != nullptr; scope = scope->parent) {
		map<string, SymbolEntry>::const_iterator it = scope->symbols.find(name);
		if (it != scope->symbols.end()) {
			return &it->second;
		}
	}
	return nullptr;
}
```

查找顺序是“从内到外”：

- 先查当前块
- 找不到再查外层块
- 最后查全局

这正对应高级语言中的静态作用域规则。

## 7. 语句级分析：每类语句如何处理

### 7.1 统一分发入口

不同语句由 `analyzeStatement` 分派给不同函数。

```cpp
void analyzeStatement(const Json& stmt) {
	string type = jsonNodeType(stmt);
	if (type == "VarDecl") {
		analyzeVarDecl(stmt);
	} else if (type == "AssignStmt") {
		analyzeAssign(stmt);
	} else if (type == "ExprStmt") {
		const Json* expr = jsonField(stmt, "expr");
		if (expr) {
			analyzeExpr(*expr);
		}
	} else if (type == "ReturnStmt") {
		analyzeReturn(stmt);
	} else if (type == "IfStmt") {
		analyzeIf(stmt);
	} else if (type == "WhileStmt") {
		analyzeWhile(stmt);
	} else if (type == "ForStmt") {
		analyzeFor(stmt);
	} else if (type == "Block") {
		analyzeBlock(stmt, true, makeScopeName("block", stmt));
	}
}
```

这样可以把语义分析做成一种“访问者模式”的简化版本：根据节点类型调用对应处理逻辑。

### 7.2 变量声明

变量声明要做两件事：

1. 检查当前作用域是否重复声明
2. 如果有初始化表达式，检查初始化值和声明类型是否兼容

```cpp
void analyzeVarDecl(const Json& stmt) {
	const Json* typeNode = jsonField(stmt, "varType");
	string declaredType = typeNode ? jsonStringField(*typeNode, "name", "<unknown>") : "<unknown>";
	const vector<Json>& declarators = jsonArrayField(stmt, "declarators");
	for (size_t i = 0; i < declarators.size(); ++i) {
		string name = jsonStringField(declarators[i], "name");
		if (existsInCurrentScope(name)) {
			addRedeclared(name, jsonNodeLine(declarators[i]), jsonNodeCol(declarators[i]));
			const Json* init = jsonField(declarators[i], "init");
			if (init) {
				checkType(declaredType, analyzeExpr(*init), *init);
			}
			continue;
		}

		const Json* init = jsonField(declarators[i], "init");
		if (init) {
			checkType(declaredType, analyzeExpr(*init), *init);
		}

		SymbolEntry entry;
		entry.name = name;
		entry.type = declaredType;
		entry.scope = currentScope->name;
		entry.returnType.clear();
		entry.line = jsonNodeLine(declarators[i]);
		entry.col = jsonNodeCol(declarators[i]);
		entry.isFunction = false;
		entry.builtin = false;
		declareSymbol(entry);
	}
}
```

这里还有一个细节：即使当前变量重复声明，代码仍会继续分析初始化表达式。这有助于“一次报告多个语义错误”。

### 7.3 赋值语句

赋值的核心逻辑是：

- 先分析左值的类型
- 再分析右值的类型
- 最后检查二者是否兼容

```cpp
void analyzeAssign(const Json& stmt) {
	const Json* target = jsonField(stmt, "target");
	const Json* value = jsonField(stmt, "value");
	if (!target || !value) {
		return;
	}
	string targetType = analyzeExpr(*target);
	string valueType = analyzeExpr(*value);
	checkType(targetType, valueType, *value);
}
```

因此：

- `x = "abc"` 会触发类型不匹配
- `arr[i] = 1` 会先检查 `arr[i]` 的索引是否合法，再得到元素类型，再检查赋值

### 7.4 return 语句

函数返回语句的任务是：把返回表达式类型与当前函数声明的返回类型比较。

```cpp
void analyzeReturn(const Json& stmt) {
	if (!jsonHasField(stmt, "value")) {
		checkType(currentReturnType, "void", stmt);
		return;
	}
	const Json* value = jsonField(stmt, "value");
	if (value) {
		checkType(currentReturnType, analyzeExpr(*value), *value);
	}
}
```

这里借助成员变量 `currentReturnType` 记住“我现在正在分析哪个函数”。这样 `return` 不需要回头再查所在函数节点。

### 7.5 if / while / for 控制语句

控制语句主要关注两件事：

- 条件表达式是否为 `bool`
- 进入语句体时是否需要新作用域

例如 `if`：

```cpp
void analyzeIf(const Json& stmt) {
	const Json* cond = jsonField(stmt, "cond");
	if (cond) {
		checkType("bool", analyzeExpr(*cond), *cond);
	}
	const Json* thenBlock = jsonField(stmt, "then");
	if (thenBlock) {
		analyzeBlock(*thenBlock, true, makeScopeName("if", stmt));
	}

	const vector<Json>& elifs = jsonArrayField(stmt, "elif");
	for (size_t i = 0; i < elifs.size(); ++i) {
		const Json* elifCond = jsonField(elifs[i], "cond");
		if (elifCond) {
			checkType("bool", analyzeExpr(*elifCond), *elifCond);
		}
		const Json* elifBody = jsonField(elifs[i], "body");
		if (elifBody) {
			analyzeBlock(*elifBody, true, makeScopeName("elif", elifs[i]));
		}
	}

	const Json* elseBody = jsonField(stmt, "elseBody");
	if (elseBody) {
		analyzeBlock(*elseBody, true, makeScopeName("else", stmt));
	}
}
```

例如 `while`：

```cpp
void analyzeWhile(const Json& stmt) {
	const Json* cond = jsonField(stmt, "cond");
	if (cond) {
		checkType("bool", analyzeExpr(*cond), *cond);
	}
	const Json* body = jsonField(stmt, "body");
	if (body) {
		analyzeBlock(*body, true, makeScopeName("while", stmt));
	}
}
```

`for` 稍微特殊一点，因为循环变量也要视作在新作用域中声明：

```cpp
void analyzeFor(const Json& stmt) {
	const Json* start = jsonField(stmt, "start");
	const Json* end = jsonField(stmt, "end");
	const Json* step = jsonField(stmt, "step");
	if (start) {
		checkType("int", analyzeExpr(*start), *start);
	}
	if (end) {
		checkType("int", analyzeExpr(*end), *end);
	}
	if (step) {
		checkType("int", analyzeExpr(*step), *step);
	}

	Scope* savedScope = currentScope;
	currentScope = createScope(makeScopeName("for", stmt), currentScope);

	SymbolEntry entry;
	entry.name = jsonStringField(stmt, "var");
	entry.type = "int";
	entry.scope = currentScope->name;
	entry.returnType.clear();
	entry.line = jsonNodeLine(stmt);
	entry.col = jsonNodeCol(stmt);
	entry.isFunction = false;
	entry.builtin = false;
	declareSymbol(entry);

	const Json* body = jsonField(stmt, "body");
	if (body) {
		analyzeBlock(*body, false, "");
	}

	currentScope = savedScope;
}
```

也就是说，`for` 同时承担了“控制流检查”和“循环变量建表”的责任。

## 8. 表达式级分析：类型是怎样一步步推导出来的

语义分析的关键任务之一，是为表达式推导出类型。统一入口如下：

```cpp
string analyzeExpr(const Json& expr) {
	string type = jsonNodeType(expr);
	if (type == "IntLiteral") {
		return "int";
	}
	if (type == "FloatLiteral") {
		return "float";
	}
	if (type == "StringLiteral") {
		return "str";
	}
	if (type == "BoolLiteral") {
		return "bool";
	}
	if (type == "Identifier") {
		return analyzeIdentifier(expr);
	}
	if (type == "UnaryExpr") {
		return analyzeUnary(expr);
	}
	if (type == "BinaryExpr") {
		return analyzeBinary(expr);
	}
	if (type == "Call") {
		return analyzeCall(expr);
	}
	if (type == "IndexExpr") {
		return analyzeIndex(expr);
	}
	if (type == "ArrayLiteral") {
		return analyzeArrayLiteral(expr);
	}
	return "<unknown>";
}
```

### 8.1 标识符：先查符号表

```cpp
string analyzeIdentifier(const Json& expr) {
	string name = jsonStringField(expr, "name");
	if (name.empty()) {
		return "<unknown>";
	}
	const SymbolEntry* entry = lookup(name);
	if (!entry) {
		addUndeclared(name, expr);
		return "<unknown>";
	}
	return entry->type;
}
```

这里体现了 `UNDECLARED` 的检测方式：如果当前作用域链中找不到这个名字，就报错并返回 `<unknown>`。

返回 `<unknown>` 的目的是做“错误恢复”：即使某个名字没声明，分析器依然可以继续向下走，而不是整份程序立刻停止分析。

### 8.2 一元表达式

```cpp
string analyzeUnary(const Json& expr) {
	string op = jsonStringField(expr, "op");
	const Json* inner = jsonField(expr, "expr");
	if (!inner) {
		return "<unknown>";
	}
	string innerType = analyzeExpr(*inner);
	if (op == "not") {
		checkType("bool", innerType, *inner);
		return "bool";
	}
	if (op == "~") {
		checkType("int", innerType, *inner);
		return "int";
	}
	if (op == "+" || op == "-") {
		if (!isUnknownType(innerType) && !isNumericType(innerType)) {
			addTypeMismatch("numeric", innerType, *inner);
			return "<unknown>";
		}
		return innerType;
	}
	return "<unknown>";
}
```

规则可以概括为：

- `not x` 要求 `x` 是 `bool`
- `~x` 要求 `x` 是 `int`
- `+x` 和 `-x` 要求 `x` 是数值类型

### 8.3 二元表达式

二元表达式的检查最丰富，因为不同运算符的规则不同。

```cpp
string analyzeBinary(const Json& expr) {
	string op = jsonStringField(expr, "op");
	const Json* leftNode = jsonField(expr, "left");
	const Json* rightNode = jsonField(expr, "right");
	if (!leftNode || !rightNode) {
		return "<unknown>";
	}
	string leftType = analyzeExpr(*leftNode);
	string rightType = analyzeExpr(*rightNode);

	if (op == "and" || op == "or") {
		checkType("bool", leftType, *leftNode);
		checkType("bool", rightType, *rightNode);
		return "bool";
	}

	if (op == "|" || op == "^" || op == "&" || op == "<<" || op == ">>" || op == "%") {
		checkType("int", leftType, *leftNode);
		checkType("int", rightType, *rightNode);
		return "int";
	}

	if (op == "+" || op == "-" || op == "*" || op == "/") {
		if (!isUnknownType(leftType) && !isNumericType(leftType)) {
			addTypeMismatch("numeric", leftType, *leftNode);
		}
		if (!isUnknownType(rightType) && !isNumericType(rightType)) {
			addTypeMismatch("numeric", rightType, *rightNode);
		}
		if (isUnknownType(leftType) || isUnknownType(rightType)) {
			return "<unknown>";
		}
		if (!isNumericType(leftType) || !isNumericType(rightType)) {
			return "<unknown>";
		}
		return leftType == "float" || rightType == "float" ? "float" : "int";
	}

	if (op == "==" || op == "!=") {
		if (!areTypesComparable(leftType, rightType)) {
			addTypeMismatch(leftType, rightType, *rightNode);
		}
		return "bool";
	}

	if (op == "<" || op == ">" || op == "<=" || op == ">=") {
		if (!isUnknownType(leftType) && !isNumericType(leftType)) {
			addTypeMismatch("numeric", leftType, *leftNode);
		}
		if (!isUnknownType(rightType) && !isNumericType(rightType)) {
			addTypeMismatch("numeric", rightType, *rightNode);
		}
		return "bool";
	}

	return "<unknown>";
}
```

这段代码体现了几条核心类型规则：

- 逻辑运算：结果一定是 `bool`
- 位运算和移位：操作数必须是 `int`
- 算术运算：操作数必须是数值类型，`int + float` 的结果提升为 `float`
- 比较运算：结果是 `bool`

## 9. 函数调用检查：参数个数、参数类型、是否可调用

函数调用是语义分析中最典型的综合场景。它既要查名字，也要看是不是函数，还要比较参数个数和参数类型。

```cpp
string analyzeCall(const Json& expr) {
	const Json* callee = jsonField(expr, "callee");
	const vector<Json>& args = jsonArrayField(expr, "args");
	vector<string> argTypes;
	for (size_t i = 0; i < args.size(); ++i) {
		argTypes.push_back(analyzeExpr(args[i]));
	}
	if (!callee) {
		return "<unknown>";
	}

	if (jsonNodeType(*callee) != "Identifier") {
		string calleeType = analyzeExpr(*callee);
		addTypeMismatch("function", calleeType, *callee);
		return "<unknown>";
	}

	string name = jsonStringField(*callee, "name");
	if (isBuiltinCast(name)) {
		if (static_cast<int>(args.size()) != 1) {
			addArgCountMismatch(name, 1, static_cast<int>(args.size()), expr);
			return name;
		}
		if (!argTypes.empty() && !isUnknownType(argTypes[0]) && !isScalarType(argTypes[0])) {
			addArgTypeMismatch(name, "scalar", argTypes[0], args[0]);
		}
		return name;
	}

	const SymbolEntry* entry = lookup(name);
	if (!entry) {
		addUndeclared(name, *callee);
		return "<unknown>";
	}
	if (!entry->isFunction) {
		addTypeMismatch("function", entry->type, *callee);
		return "<unknown>";
	}

	if (static_cast<int>(args.size()) != static_cast<int>(entry->paramTypes.size())) {
		addArgCountMismatch(name, static_cast<int>(entry->paramTypes.size()), static_cast<int>(args.size()), expr);
	}

	size_t limit = min(args.size(), entry->paramTypes.size());
	for (size_t i = 0; i < limit; ++i) {
		if (!areTypesCompatible(entry->paramTypes[i], argTypes[i])) {
			addArgTypeMismatch(name, entry->paramTypes[i], argTypes[i], args[i]);
		}
	}
	return entry->returnType;
}
```

这个流程可以概括为：

1. 先递归分析每个实参，得到实参类型
2. 检查被调用对象是不是标识符
3. 若是内建转换函数，则走专门规则
4. 否则在符号表里查这个名字
5. 如果名字不存在，报 `UNDECLARED`
6. 如果名字存在但不是函数，报“期望 function 的类型错误”
7. 若是函数，则比较参数个数和参数类型
8. 最后返回函数返回类型，供外层表达式继续使用

因此：

- `f(1)` 但 `f` 需要两个参数，会报 `ARG_COUNT_MISMATCH`
- `f("abc")` 而形参是 `int`，会报 `ARG_TYPE_MISMATCH`

## 10. 数组相关检查

### 10.1 数组下标

```cpp
string analyzeIndex(const Json& expr) {
	const Json* arrayNode = jsonField(expr, "array");
	const Json* indexNode = jsonField(expr, "index");
	if (!arrayNode || !indexNode) {
		return "<unknown>";
	}
	string arrayType = analyzeExpr(*arrayNode);
	string indexType = analyzeExpr(*indexNode);
	checkType("int", indexType, *indexNode);
	if (isUnknownType(arrayType)) {
		return "<unknown>";
	}
	if (!isArrayType(arrayType)) {
		addTypeMismatch("array", arrayType, *arrayNode);
		return "<unknown>";
	}
	return elementType(arrayType);
}
```

这里检查了两个条件：

- 下标必须是 `int`
- 被下标访问的对象必须是数组

若都成立，则返回数组元素类型。例如：

- `int[]` 下标后得到 `int`
- `int[][]` 下标一次后得到 `int[]`

### 10.2 数组字面量

数组字面量的目标是从元素中推导数组类型。

```cpp
string analyzeArrayLiteral(const Json& expr) {
	const vector<Json>& elements = jsonArrayField(expr, "elements");
	if (elements.empty()) {
		return "<unknown>[]";
	}

	string inferred = analyzeExpr(elements[0]);
	for (size_t i = 1; i < elements.size(); ++i) {
		string next = analyzeExpr(elements[i]);
		string merged = commonType(inferred, next);
		if (isUnknownType(merged) && !isUnknownType(inferred) && !isUnknownType(next)) {
			addTypeMismatch(inferred, next, elements[i]);
		}
		inferred = merged;
	}
	return inferred + "[]";
}
```

例如：

- `[1, 2, 3]` 推导为 `int[]`
- `[1, 2.5]` 推导为 `float[]`
- `[1, "abc"]` 无法合并，会报类型错误

## 11. 类型兼容规则是如何定义的

类型兼容性最终由 `areTypesCompatible` 控制：

```cpp
bool areTypesCompatible(const string& expected, const string& found) const {
	if (isUnknownType(expected) || isUnknownType(found)) {
		return true;
	}
	if (expected == found) {
		return true;
	}
	if (expected == "float" && found == "int") {
		return true;
	}
	if (isArrayType(expected) && isArrayType(found)) {
		return areTypesCompatible(elementType(expected), elementType(found));
	}
	return false;
}
```

当前实现采用了比较朴素但很实用的规则：

- 同类型一定兼容
- `int` 可以赋给 `float`
- 数组类型递归比较元素类型
- 只要某一侧是 `<unknown>`，暂时视为兼容，避免级联报错

这让整个语义分析器既能保持规则明确，又能在出现前置错误时继续往下分析。

## 12. 错误是如何统一记录和输出的

语义分析阶段不直接打印错误，而是先把错误压入 `errors` 数组，最后统一输出。

例如 `UNDECLARED` 的构造：

```cpp
void addUndeclared(const string& symbol, const Json& node) {
	SemanticError error;
	error.type = "UNDECLARED";
	error.symbol = symbol;
	error.expectedCount = 0;
	error.foundCount = 0;
	error.line = jsonNodeLine(node);
	error.col = jsonNodeCol(node);
	error.hasSymbol = true;
	error.hasExpected = false;
	error.hasFound = false;
	error.hasExpectedCount = false;
	error.hasFoundCount = false;
	addError(error);
}
```

最后借助 `semanticErrorJson` 和 `printCheckJson` 转成 JSON：

```cpp
static Json semanticErrorJson(const SemanticError& error) {
	Json item = Json::object()
		.add("type", Json::str(error.type))
		.add("line", Json::num(to_string(error.line)))
		.add("col", Json::num(to_string(error.col)));
	if (error.hasSymbol) {
		item.add("symbol", Json::str(error.symbol));
	}
	if (error.hasExpected) {
		item.add("expected", Json::str(error.expected));
	}
	if (error.hasFound) {
		item.add("found", Json::str(error.found));
	}
	if (error.hasExpectedCount) {
		item.add("expected", Json::num(to_string(error.expectedCount)));
	}
	if (error.hasFoundCount) {
		item.add("found", Json::num(to_string(error.foundCount)));
	}
	return item;
}
```

这种设计的优点是：

- 分析逻辑和输出逻辑分离
- 后面如果要改输出格式，不需要改语义规则本身
- 可以在分析完一整棵 AST 之后，再集中展示所有错误

## 13. 一次完整的语义分析流程总结

把整个过程串起来，可以概括为下面几个步骤：

1. 词法分析器生成 token
2. 语法分析器根据 token 构建 AST
3. 若词法或语法阶段有错误，则直接输出，不进入语义分析
4. 创建 `SemanticAnalyzer`，初始化全局作用域和内建函数
5. 先扫描 AST 顶层函数，完成函数预声明
6. 再递归分析函数体和其他语句
7. 在分析过程中维护作用域栈、符号表和当前函数返回类型
8. 对变量、赋值、返回、函数调用、数组访问、运算表达式进行类型检查
9. 将所有错误保存到 `errors`，将所有用户符号保存到 `symbols`
10. 最后统一输出 `check` 结果 JSON

## 14. 这个实现的特点

当前这版语义分析器有几个比较适合实验作业的特点：

- 与原来的词法、语法代码耦合很低
- 符号表、作用域、类型检查的逻辑比较集中，便于讲解
- 能一次报告多个错误，而不是遇到第一个错误就退出
- 已支持函数、数组、基本类型、返回值和常见表达式

同时也保留了后续扩展空间。例如如果实验五要生成中间代码，可以继续沿用这里已经推导出的类型信息和作用域结构。

## 15. 如果在答辩中要一句话概括

可以这样描述这份实现：

> 语义分析器以实验三构造出的 AST 为输入，采用“预声明函数 + 递归遍历语句和表达式”的方式，在作用域栈上维护符号表，并通过类型推导与兼容性判断实现未声明、重复声明、类型不匹配以及函数调用参数错误等语义检查，最后统一输出错误列表和符号表。
