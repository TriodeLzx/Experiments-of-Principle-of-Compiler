#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <functional>

using namespace std;

struct Token {
	string type;
	string value;
	int line;
	int col;
};

struct LexError {
	string type;
	string value;
	int line;
	int col;
};

static bool isWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static bool isDigit(char c) {
	return c >= '0' && c <= '9';
}

static bool isLetter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isIdentifierStart(char c) {
	return isLetter(c) || c == '_';
}

static bool isIdentifierPart(char c) {
	return isIdentifierStart(c) || isDigit(c);
}

static string escapeJson(const string& value) {
	string escaped;
	escaped.reserve(value.size() + 8);
	for (unsigned char c : value) {
		switch (c) {
			case '"': escaped += "\\\""; break;
			case '\\': escaped += "\\\\"; break;
			case '\b': escaped += "\\b"; break;
			case '\f': escaped += "\\f"; break;
			case '\n': escaped += "\\n"; break;
			case '\r': escaped += "\\r"; break;
			case '\t': escaped += "\\t"; break;
			default:
				if (c < 0x20) {
					static const char* hex = "0123456789abcdef";
					escaped += "\\u00";
					escaped += hex[(c >> 4) & 0xF];
					escaped += hex[c & 0xF];
				} else {
					escaped += static_cast<char>(c);
				}
				break;
		}
	}
	return escaped;
}

class Lexer {
public:
	explicit Lexer(string input) : src(move(input)) {}

	void run() {
		while (true) {
			skipWhitespaceAndComments();
			if (eof()) {
				break;
			}

			if (scanString()) {
				continue;
			}
			if (scanNumber()) {
				continue;
			}
			if (scanIdentifierOrKeyword()) {
				continue;
			}
			if (scanOperatorOrDelimiter()) {
				continue;
			}

			int startLine = line;
			int startCol = col;
			addError("ILLEGAL_CHAR", string(1, peek()), startLine, startCol);
			advanceOne();
		}

		tokens.push_back(Token{"EOF", "", line, col});
	}

	const vector<Token>& getTokens() const {
		return tokens;
	}

	const vector<LexError>& getErrors() const {
		return errors;
	}

private:
	string src;
	size_t pos = 0;
	int line = 1;
	int col = 1;
	vector<Token> tokens;
	vector<LexError> errors;

	char peek(size_t offset = 0) const {
		if (pos + offset >= src.size()) {
			return '\0';
		}
		return src[pos + offset];
	}

	bool eof() const {
		return pos >= src.size();
	}

	void advanceOne() {
		if (eof()) {
			return;
		}

		char c = src[pos++];
		if (c == '\r') {
			if (pos < src.size() && src[pos] == '\n') {
				++pos;
			}
			++line;
			col = 1;
		} else if (c == '\n') {
			++line;
			col = 1;
		} else {
			++col;
		}
	}

	void addToken(const string& type, const string& value, int startLine, int startCol) {
		tokens.push_back(Token{type, value, startLine, startCol});
	}

	void addError(const string& type, const string& value, int startLine, int startCol) {
		errors.push_back(LexError{type, value, startLine, startCol});
	}

	bool startsWith(const string& prefix) const {
		return src.compare(pos, prefix.size(), prefix) == 0;
	}

	void skipLineComment() {
		while (!eof() && peek() != '\n' && peek() != '\r') {
			advanceOne();
		}
	}

	void skipWhitespaceAndComments() {
		while (true) {
			while (!eof() && isWhitespace(peek())) {
				advanceOne();
			}

			if (eof()) {
				return;
			}

			if (peek() == '#') {
				skipLineComment();
				continue;
			}

			break;
		}
	}

	bool canStartSignedNumber() const {
		if (tokens.empty()) {
			return true;
		}

		const Token& previous = tokens.back();
		if (previous.type == "ASSIGN" || previous.type == "OP") {
			return true;
		}

		if (previous.type == "DELIM") {
			static const set<string> openingDelims = {"(", "[", "{", ",", ":", ";"};
			return openingDelims.count(previous.value) > 0;
		}

		if (previous.type == "KEYWORD") {
			static const set<string> expressionKeywords = {"if", "elif", "while", "for", "ret", "return", "and", "or", "not"};
			return expressionKeywords.count(previous.value) > 0;
		}

		return false;
	}

	bool isNumberStart() const {
		if (isDigit(peek())) {
			return true;
		}

		if (peek() == '-' && canStartSignedNumber()) {
			if (isDigit(peek(1))) {
				return true;
			}
			if (peek(1) == '.' && isDigit(peek(2))) {
				return true;
			}
		}

		if (peek() == '.' && isDigit(peek(1))) {
			return true;
		}

		return false;
	}

	bool isValidNumberLexeme(const string& lexeme) const {
		if (lexeme.empty()) {
			return false;
		}

		size_t start = 0;
		bool negative = false;
		if (lexeme[start] == '-') {
			negative = true;
			++start;
		} else if (lexeme[start] == '.') {
			return false;
		}

		if (start >= lexeme.size()) {
			return false;
		}

		string body = lexeme.substr(start);
		for (char c : body) {
			if (!isDigit(c) && c != '.') {
				return false;
			}
		}

		size_t dotPos = body.find('.');
		if (dotPos == string::npos) {
			if (negative) {
				return body[0] != '0';
			}
			return body == "0" || (body[0] != '0' && body.size() > 0);
		}

		if (body.find('.', dotPos + 1) != string::npos) {
			return false;
		}

		string intPart = body.substr(0, dotPos);
		string fracPart = body.substr(dotPos + 1);
		if (intPart.empty() || fracPart.empty()) {
			return false;
		}

		for (char c : intPart) {
			if (!isDigit(c)) {
				return false;
			}
		}
		for (char c : fracPart) {
			if (!isDigit(c)) {
				return false;
			}
		}

		return true;
	}

	bool scanNumber() {
		if (!isNumberStart()) {
			return false;
		}

		int startLine = line;
		int startCol = col;
		size_t startPos = pos;

		if (peek() == '-') {
			advanceOne();
		}

		while (!eof()) {
			char c = peek();
			if (isDigit(c) || isLetter(c) || c == '_' || c == '.') {
				advanceOne();
			} else {
				break;
			}
		}

		string lexeme = src.substr(startPos, pos - startPos);
		if (isValidNumberLexeme(lexeme)) {
			addToken("NUM", lexeme, startLine, startCol);
		} else {
			addError("INVALID_NUMBER", lexeme, startLine, startCol);
		}
		return true;
	}

	bool scanString() {
		if (peek() != '"') {
			return false;
		}

		int startLine = line;
		int startCol = col;
		size_t startPos = pos;

		advanceOne();
		while (!eof()) {
			char c = peek();
			if (c == '\\') {
				advanceOne();
				if (!eof()) {
					advanceOne();
				}
				continue;
			}

			if (c == '"') {
				advanceOne();
				addToken("STR", src.substr(startPos, pos - startPos), startLine, startCol);
				return true;
			}

			if (c == '\n' || c == '\r') {
				addError("UNCLOSED_STRING", src.substr(startPos, pos - startPos), startLine, startCol);
				advanceOne();
				return true;
			}

			advanceOne();
		}

		addError("UNCLOSED_STRING", src.substr(startPos, pos - startPos), startLine, startCol);
		return true;
	}

	bool scanIdentifierOrKeyword() {
		if (!isIdentifierStart(peek())) {
			return false;
		}

		int startLine = line;
		int startCol = col;
		size_t startPos = pos;

		while (!eof() && isIdentifierPart(peek())) {
			advanceOne();
		}

		string lexeme = src.substr(startPos, pos - startPos);

		static const set<string> keywords = {
			"fn", "if", "elif", "else", "while", "for", "ret", "return",
			"int", "bool", "float", "str", "true", "false"
		};
		static const set<string> operatorWords = {"and", "or", "not"};

		if (operatorWords.count(lexeme) > 0) {
			addToken("OP", lexeme, startLine, startCol);
		} else if (keywords.count(lexeme) > 0) {
			addToken("KEYWORD", lexeme, startLine, startCol);
		} else {
			addToken("ID", lexeme, startLine, startCol);
		}
		return true;
	}

	bool scanOperatorOrDelimiter() {
		int startLine = line;
		int startCol = col;

		if (startsWith(":=")) {
			addToken("ASSIGN", ":=", startLine, startCol);
			advanceOne();
			advanceOne();
			return true;
		}

		if (startsWith("==") || startsWith("!=") || startsWith("<=") || startsWith(">=") ||
			startsWith("<<") || startsWith(">>") || startsWith("->")) {
			addToken("OP", src.substr(pos, 2), startLine, startCol);
			advanceOne();
			advanceOne();
			return true;
		}

		char c = peek();
		switch (c) {
			case '=':
				addToken("ASSIGN", "=", startLine, startCol);
				advanceOne();
				return true;
			case '+':
			case '-':
			case '*':
			case '/':
			case '%':
			case '~':
			case '|':
			case '^':
			case '&':
			case '<':
			case '>':
				addToken("OP", string(1, c), startLine, startCol);
				advanceOne();
				return true;
			case '(':
			case ')':
			case '[':
			case ']':
			case '{':
			case '}':
			case ',':
			case ':':
			case ';':
				addToken("DELIM", string(1, c), startLine, startCol);
				advanceOne();
				return true;
			default:
				return false;
		}
	}
};

struct Json {
	enum Kind { NIL, OBJECT, ARRAY, STRING, NUMBER, BOOL } kind;
	vector<pair<string, Json> > objectItems;
	vector<Json> arrayItems;
	string text;
	bool boolValue;

	Json(Kind k = NIL) : kind(k), boolValue(false) {}

	static Json object() { return Json(OBJECT); }
	static Json array() { return Json(ARRAY); }
	static Json str(const string& value) { Json j(STRING); j.text = value; return j; }
	static Json num(const string& value) { Json j(NUMBER); j.text = value; return j; }
	static Json boolean(bool value) { Json j(BOOL); j.boolValue = value; return j; }

	Json& add(const string& key, const Json& value) {
		objectItems.push_back(make_pair(key, value));
		return *this;
	}

	Json& push(const Json& value) {
		arrayItems.push_back(value);
		return *this;
	}
};

struct ParseError {
	string expected;
	string found;
	int line;
	int col;
	string message;
};

static void printIndent(int spaces) {
	for (int i = 0; i < spaces; ++i) {
		cout << ' ';
	}
}

static void printJsonValue(const Json& value, int indent) {
	switch (value.kind) {
		case Json::NIL:
			cout << "null";
			break;
		case Json::STRING:
			cout << "\"" << escapeJson(value.text) << "\"";
			break;
		case Json::NUMBER:
			cout << value.text;
			break;
		case Json::BOOL:
			cout << (value.boolValue ? "true" : "false");
			break;
		case Json::ARRAY:
			if (value.arrayItems.empty()) {
				cout << "[]";
				break;
			}
			cout << "[\n";
			for (size_t i = 0; i < value.arrayItems.size(); ++i) {
				printIndent(indent + 2);
				printJsonValue(value.arrayItems[i], indent + 2);
				if (i + 1 < value.arrayItems.size()) {
					cout << ",";
				}
				cout << "\n";
			}
			printIndent(indent);
			cout << "]";
			break;
		case Json::OBJECT:
			if (value.objectItems.empty()) {
				cout << "{}";
				break;
			}
			cout << "{\n";
			for (size_t i = 0; i < value.objectItems.size(); ++i) {
				printIndent(indent + 2);
				cout << "\"" << escapeJson(value.objectItems[i].first) << "\": ";
				printJsonValue(value.objectItems[i].second, indent + 2);
				if (i + 1 < value.objectItems.size()) {
					cout << ",";
				}
				cout << "\n";
			}
			printIndent(indent);
			cout << "}";
			break;
	}
}

class Parser {
public:
	explicit Parser(const vector<Token>& tokenStream) : tokens(tokenStream) {}

	Json parseProgram() {
		Token start = peek();
		if (start.type == "EOF") {
			start.line = 1;
			start.col = 1;
		}

		Json body = Json::array();
		while (!eof()) {
			size_t before = pos;
			if (isKeyword("fn")) {
				body.push(parseFunction());
			} else {
				body.push(parseStatement());
			}
			if (pos == before) {
				advance();
			}
		}

		return node("Program", start).add("body", body);
	}

	const vector<ParseError>& getErrors() const {
		return errors;
	}

private:
	const vector<Token>& tokens;
	size_t pos = 0;
	vector<ParseError> errors;

	const Token& peek(size_t offset = 0) const {
		size_t index = pos + offset;
		if (index < tokens.size()) {
			return tokens[index];
		}
		return tokens.back();
	}

	bool eof() const {
		return peek().type == "EOF";
	}

	const Token& advance() {
		const Token& token = peek();
		if (!eof()) {
			++pos;
		}
		return token;
	}

	bool at(const string& type) const {
		return peek().type == type;
	}

	bool at(const string& type, const string& value) const {
		return peek().type == type && peek().value == value;
	}

	bool isKeyword(const string& value) const {
		return at("KEYWORD", value);
	}

	bool isOp(const string& value) const {
		return at("OP", value);
	}

	bool isDelim(const string& value) const {
		return at("DELIM", value);
	}

	bool isBaseTypeToken(const Token& token) const {
		return token.type == "KEYWORD" &&
			(token.value == "int" || token.value == "bool" ||
			 token.value == "float" || token.value == "str");
	}

	string foundText(const Token& token) const {
		if (token.type == "EOF") {
			return "EOF";
		}
		return token.value.empty() ? token.type : token.value;
	}

	void syntaxError(const string& expected, const Token& token) {
		string found = foundText(token);
		errors.push_back(ParseError{
			expected,
			found,
			token.line,
			token.col,
			"expected '" + expected + "' before '" + found + "'"
		});
	}

	Token consume(const string& type, const string& value, const string& expected) {
		if (at(type, value)) {
			return advance();
		}
		Token token = peek();
		syntaxError(expected, token);
		return token;
	}

	Token consumeType(const string& type, const string& expected) {
		if (at(type)) {
			return advance();
		}
		Token token = peek();
		syntaxError(expected, token);
		return token;
	}

	Json node(const string& type, const Token& token) const {
		return Json::object()
			.add("type", Json::str(type))
			.add("line", Json::num(to_string(token.line)))
			.add("col", Json::num(to_string(token.col)));
	}

	void skipLine(int line) {
		while (!eof() && peek().line == line) {
			advance();
		}
	}

	void finishLine(int line) {
		if (!eof() && peek().line == line) {
			syntaxError("end of statement", peek());
			skipLine(line);
		}
	}

	Json parseFunction() {
		Token start = consume("KEYWORD", "fn", "fn");
		Token name = consumeType("ID", "identifier");
		consume("DELIM", "(", "(");

		Json params = Json::array();
		if (!isDelim(")") && !eof()) {
			params.push(parseParam());
			while (isDelim(",")) {
				advance();
				params.push(parseParam());
			}
		}
		consume("DELIM", ")", ")");

		bool hasReturnType = false;
		Json returnType;
		if (isOp("->")) {
			advance();
			returnType = parseType();
			hasReturnType = true;
		}

		consume("DELIM", ":", ":");
		Json body = parseSuite(start.line, start.col);
		Json result = node("FuncDecl", start).add("name", Json::str(name.value)).add("body", body);
		if (hasReturnType) {
			result.add("returnType", returnType);
		}
		if (!params.arrayItems.empty()) {
			result.add("params", params);
		}
		return result;
	}

	Json parseParam() {
		Token name = consumeType("ID", "identifier");
		consume("DELIM", ":", ":");
		return node("ParamDecl", name)
			.add("name", Json::str(name.value))
			.add("paramType", parseType());
	}

	Json parseType() {
		Token start = peek();
		if (!isBaseTypeToken(start)) {
			syntaxError("type", start);
			if (!eof()) {
				advance();
			}
			return node("Type", start).add("name", Json::str(""));
		}

		advance();
		string name = start.value;
		while (isDelim("[")) {
			advance();
			consume("DELIM", "]", "]");
			name += "[]";
		}
		return node("Type", start).add("name", Json::str(name));
	}

	Json parseSuite(int headerLine, int headerCol) {
		if (eof()) {
			syntaxError("suite", peek());
			Token token = peek();
			return node("Block", token).add("stmts", Json::array());
		}

		if (peek().line == headerLine) {
			Json stmts = Json::array();
			Token start = peek();
			Json stmt = parseSimpleStatement();
			finishLine(headerLine);
			stmts.push(stmt);
			return node("Block", start).add("stmts", stmts);
		}

		if (peek().col <= headerCol) {
			syntaxError("indented block", peek());
			return node("Block", peek()).add("stmts", Json::array());
		}

		Token start = peek();
		int indent = start.col;
		Json stmts = Json::array();
		while (!eof() && peek().col >= indent) {
			size_t before = pos;
			stmts.push(parseStatement());
			if (pos == before) {
				advance();
			}
		}
		return node("Block", start).add("stmts", stmts);
	}

	Json parseStatement() {
		if (isKeyword("if")) {
			return parseIf();
		}
		if (isKeyword("while")) {
			return parseWhile();
		}
		if (isKeyword("for")) {
			return parseFor();
		}

		int line = peek().line;
		Json stmt = parseSimpleStatement();
		finishLine(line);
		return stmt;
	}

	Json parseSimpleStatement() {
		if (isBaseTypeToken(peek())) {
			return parseVarDecl();
		}
		if (isKeyword("ret") || isKeyword("return")) {
			return parseReturn();
		}
		return parseAssignOrExpr();
	}

	Json parseVarDecl() {
		Token start = peek();
		Json type = parseType();
		Json declarators = Json::array();
		declarators.push(parseDeclarator());
		while (isDelim(",")) {
			advance();
			declarators.push(parseDeclarator());
		}
		return node("VarDecl", start).add("varType", type).add("declarators", declarators);
	}

	Json parseDeclarator() {
		Token name = consumeType("ID", "identifier");
		Json result = node("VarDeclarator", name).add("name", Json::str(name.value));
		if (at("ASSIGN", "=")) {
			advance();
			result.add("init", parseExpression(name.line));
		}
		return result;
	}

	Json parseReturn() {
		Token start = advance();
		Json result = node("ReturnStmt", start);
		if (!eof() && peek().line == start.line) {
			result.add("value", parseExpression(start.line));
		}
		return result;
	}

	Json parseAssignOrExpr() {
		Token start = peek();
		Json left = parseExpression(start.line);
		if (at("ASSIGN", "=") && peek().line == start.line) {
			advance();
			return node("AssignStmt", start)
				.add("target", left)
				.add("value", parseExpression(start.line));
		}
		return node("ExprStmt", start).add("expr", left);
	}

	Json parseIf() {
		Token start = consume("KEYWORD", "if", "if");
		Json cond = parseExpression(start.line);
		consume("DELIM", ":", ":");
		Json thenBlock = parseSuite(start.line, start.col);
		Json elifs = Json::array();

		while (isKeyword("elif") && peek().col == start.col) {
			Token elifToken = advance();
			Json elifCond = parseExpression(elifToken.line);
			consume("DELIM", ":", ":");
			elifs.push(node("ElifClause", elifToken)
				.add("cond", elifCond)
				.add("body", parseSuite(elifToken.line, elifToken.col)));
		}

		bool hasElse = false;
		Json elseBlock;
		if (isKeyword("else") && peek().col == start.col) {
			Token elseToken = advance();
			consume("DELIM", ":", ":");
			elseBlock = parseSuite(elseToken.line, elseToken.col);
			hasElse = true;
		}

		if (hasElse) {
			Json result = node("IfStmt", start).add("cond", cond);
			if (!elifs.arrayItems.empty()) {
				result.add("elif", elifs);
			}
			return result.add("elseBody", elseBlock).add("then", thenBlock);
		}

		Json result = node("IfStmt", start).add("cond", cond).add("then", thenBlock);
		if (!elifs.arrayItems.empty()) {
			result.add("elif", elifs);
			return result;
		}
		return result;
	}

	Json parseWhile() {
		Token start = consume("KEYWORD", "while", "while");
		Json cond = parseExpression(start.line);
		consume("DELIM", ":", ":");
		return node("WhileStmt", start)
			.add("body", parseSuite(start.line, start.col))
			.add("cond", cond);
	}

	Json parseFor() {
		Token start = consume("KEYWORD", "for", "for");
		Token var = consumeType("ID", "identifier");
		consume("ASSIGN", "=", "=");
		Json from = parseExpression(start.line);
		consume("DELIM", ",", ",");
		Json to = parseExpression(start.line);

		bool hasStep = false;
		Json step;
		if (isDelim(",")) {
			advance();
			step = parseExpression(start.line);
			hasStep = true;
		}

		consume("DELIM", ":", ":");
		Json body = parseSuite(start.line, start.col);
		Json result = node("ForStmt", start)
			.add("var", Json::str(var.value))
			.add("body", body)
			.add("end", to)
			.add("start", from);
		if (hasStep) {
			result.add("step", step);
		}
		return result;
	}

	Json binaryNode(const Token& op, const Json& left, const Json& right) {
		return node("BinaryExpr", op)
			.add("op", Json::str(op.value))
			.add("left", left)
			.add("right", right);
	}

	Json parseExpression(int lineLimit) {
		return parseLogicalOr(lineLimit);
	}

	bool sameLine(int lineLimit) const {
		return !eof() && peek().line == lineLimit;
	}

	Json parseLogicalOr(int lineLimit) {
		Json left = parseLogicalAnd(lineLimit);
		while (sameLine(lineLimit) && isOp("or")) {
			Token op = advance();
			left = binaryNode(op, left, parseLogicalAnd(lineLimit));
		}
		return left;
	}

	Json parseLogicalAnd(int lineLimit) {
		Json left = parseBitwiseOr(lineLimit);
		while (sameLine(lineLimit) && isOp("and")) {
			Token op = advance();
			left = binaryNode(op, left, parseBitwiseOr(lineLimit));
		}
		return left;
	}

	Json parseBitwiseOr(int lineLimit) {
		Json left = parseBitwiseXor(lineLimit);
		while (sameLine(lineLimit) && isOp("|")) {
			Token op = advance();
			left = binaryNode(op, left, parseBitwiseXor(lineLimit));
		}
		return left;
	}

	Json parseBitwiseXor(int lineLimit) {
		Json left = parseBitwiseAnd(lineLimit);
		while (sameLine(lineLimit) && isOp("^")) {
			Token op = advance();
			left = binaryNode(op, left, parseBitwiseAnd(lineLimit));
		}
		return left;
	}

	Json parseBitwiseAnd(int lineLimit) {
		Json left = parseEquality(lineLimit);
		while (sameLine(lineLimit) && isOp("&")) {
			Token op = advance();
			left = binaryNode(op, left, parseEquality(lineLimit));
		}
		return left;
	}

	Json parseEquality(int lineLimit) {
		Json left = parseRelational(lineLimit);
		while (sameLine(lineLimit) && (isOp("==") || isOp("!="))) {
			Token op = advance();
			left = binaryNode(op, left, parseRelational(lineLimit));
		}
		return left;
	}

	Json parseRelational(int lineLimit) {
		Json left = parseShift(lineLimit);
		while (sameLine(lineLimit) &&
			   (isOp("<") || isOp(">") || isOp("<=") || isOp(">="))) {
			Token op = advance();
			left = binaryNode(op, left, parseShift(lineLimit));
		}
		return left;
	}

	Json parseShift(int lineLimit) {
		Json left = parseAdditive(lineLimit);
		while (sameLine(lineLimit) && (isOp("<<") || isOp(">>"))) {
			Token op = advance();
			left = binaryNode(op, left, parseAdditive(lineLimit));
		}
		return left;
	}

	Json parseAdditive(int lineLimit) {
		Json left = parseMultiplicative(lineLimit);
		while (sameLine(lineLimit) && (isOp("+") || isOp("-"))) {
			Token op = advance();
			left = binaryNode(op, left, parseMultiplicative(lineLimit));
		}
		return left;
	}

	Json parseMultiplicative(int lineLimit) {
		Json left = parseUnary(lineLimit);
		while (sameLine(lineLimit) && (isOp("*") || isOp("/") || isOp("%"))) {
			Token op = advance();
			left = binaryNode(op, left, parseUnary(lineLimit));
		}
		return left;
	}

	Json parseUnary(int lineLimit) {
		if (sameLine(lineLimit) && (isOp("+") || isOp("-") || isOp("~") || isOp("not"))) {
			Token op = advance();
			return node("UnaryExpr", op)
				.add("op", Json::str(op.value))
				.add("expr", parseUnary(lineLimit));
		}
		return parsePostfix(lineLimit);
	}

	Json parsePostfix(int lineLimit) {
		Json expr = parsePrimary(lineLimit);
		while (sameLine(lineLimit)) {
			if (isDelim("(")) {
				Token callStart = findNodeToken(expr);
				advance();
				Json args = Json::array();
				if (!isDelim(")") && sameLine(lineLimit)) {
					args.push(parseExpression(lineLimit));
					while (isDelim(",") && sameLine(lineLimit)) {
						advance();
						args.push(parseExpression(lineLimit));
					}
				}
				consume("DELIM", ")", ")");
				expr = node("Call", callStart).add("callee", expr).add("args", args);
			} else if (isDelim("[")) {
				Token indexStart = findNodeToken(expr);
				advance();
				Json index = parseExpression(lineLimit);
				consume("DELIM", "]", "]");
				expr = node("IndexExpr", indexStart).add("array", expr).add("index", index);
			} else {
				break;
			}
		}
		return expr;
	}

	Token findNodeToken(const Json& expr) const {
		Token token{"", "", 1, 1};
		for (size_t i = 0; i < expr.objectItems.size(); ++i) {
			if (expr.objectItems[i].first == "line") {
				token.line = atoi(expr.objectItems[i].second.text.c_str());
			} else if (expr.objectItems[i].first == "col") {
				token.col = atoi(expr.objectItems[i].second.text.c_str());
			}
		}
		return token;
	}

	string unquote(const string& value) const {
		string result;
		for (size_t i = 1; i + 1 < value.size(); ++i) {
			if (value[i] == '\\' && i + 1 < value.size() - 1) {
				char next = value[++i];
				if (next == 'n') result += '\n';
				else if (next == 't') result += '\t';
				else if (next == 'r') result += '\r';
				else result += next;
			} else {
				result += value[i];
			}
		}
		return result;
	}

	Json parseArrayLiteral(int lineLimit) {
		Token start = consume("DELIM", "[", "[");
		Json elements = Json::array();
		if (!isDelim("]") && sameLine(lineLimit)) {
			elements.push(parseExpression(lineLimit));
			while (isDelim(",") && sameLine(lineLimit)) {
				advance();
				elements.push(parseExpression(lineLimit));
			}
		}
		consume("DELIM", "]", "]");
		return node("ArrayLiteral", start).add("elements", elements);
	}

	Json parsePrimary(int lineLimit) {
		Token token = peek();
		if (token.line != lineLimit && token.type != "EOF") {
			syntaxError("expression", token);
			return node("Identifier", token).add("name", Json::str(""));
		}

		if (at("NUM")) {
			advance();
			if (token.value.find('.') != string::npos) {
				return node("FloatLiteral", token).add("value", Json::num(token.value));
			}
			return node("IntLiteral", token).add("value", Json::num(token.value));
		}
		if (at("STR")) {
			advance();
			return node("StringLiteral", token).add("value", Json::str(unquote(token.value)));
		}
		if (isKeyword("true") || isKeyword("false")) {
			advance();
			return node("BoolLiteral", token).add("value", Json::boolean(token.value == "true"));
		}
		if (at("ID") || isBaseTypeToken(token)) {
			advance();
			return node("Identifier", token).add("name", Json::str(token.value));
		}
		if (isDelim("[")) {
			return parseArrayLiteral(lineLimit);
		}
		if (isDelim("(")) {
			advance();
			Json expr = parseExpression(lineLimit);
			consume("DELIM", ")", ")");
			return expr;
		}

		syntaxError("expression", token);
		if (!eof()) {
			advance();
		}
		return node("Identifier", token).add("name", Json::str(""));
	}
};

static void printParseJson(const Json& ast, const vector<LexError>& lexErrors, const vector<ParseError>& parseErrors) {
	bool hasErrors = !lexErrors.empty() || !parseErrors.empty();
	cout << "{\n";
	cout << "  \"ast\": ";
	if (hasErrors) {
		cout << "null";
	} else {
		printJsonValue(ast, 2);
	}
	cout << ",\n";

	if (!hasErrors) {
		cout << "  \"errors\": []\n";
		cout << "}\n";
		return;
	}

	cout << "  \"errors\": [\n";
	size_t index = 0;
	size_t total = lexErrors.size() + parseErrors.size();
	for (size_t i = 0; i < lexErrors.size(); ++i, ++index) {
		const LexError& error = lexErrors[i];
		cout << "    {\"type\": \"LEX_ERROR\", \"kind\": \"" << escapeJson(error.type)
			 << "\", \"found\": \"" << escapeJson(error.value)
			 << "\", \"line\": " << error.line
			 << ", \"col\": " << error.col
			 << ", \"message\": \"lexical error: " << escapeJson(error.type) << "\"}";
		if (index + 1 < total) {
			cout << ",";
		}
		cout << "\n";
	}
	for (size_t i = 0; i < parseErrors.size(); ++i, ++index) {
		const ParseError& error = parseErrors[i];
		cout << "    {\"type\": \"SYNTAX_ERROR\", \"expected\": \"" << escapeJson(error.expected)
			 << "\", \"found\": \"" << escapeJson(error.found)
			 << "\", \"line\": " << error.line
			 << ", \"col\": " << error.col
			 << ", \"message\": \"" << escapeJson(error.message) << "\"}";
		if (index + 1 < total) {
			cout << ",";
		}
		cout << "\n";
	}
	cout << "  ]\n";
	cout << "}\n";
}

static void printJson(const vector<Token>& tokens, const vector<LexError>& errors) {
	cout << "{\n";
	if (tokens.empty()) {
		cout << "  \"tokens\": [],\n";
	} else {
		cout << "  \"tokens\": [\n";
		for (size_t i = 0; i < tokens.size(); ++i) {
			const Token& token = tokens[i];
			cout << "    {\"type\": \"" << escapeJson(token.type)
				 << "\", \"value\": \"" << escapeJson(token.value)
				 << "\", \"line\": " << token.line
				 << ", \"col\": " << token.col << "}";
			if (i + 1 < tokens.size()) {
				cout << ",";
			}
			cout << "\n";
		}
		cout << "  ],\n";
	}
	if (errors.empty()) {
		cout << "  \"errors\": []\n";
	} else {
		cout << "  \"errors\": [\n";
		for (size_t i = 0; i < errors.size(); ++i) {
			const LexError& error = errors[i];
			cout << "    {\"type\": \"" << escapeJson(error.type)
				 << "\", \"value\": \"" << escapeJson(error.value)
				 << "\", \"line\": " << error.line
				 << ", \"col\": " << error.col << "}";
			if (i + 1 < errors.size()) {
				cout << ",";
			}
			cout << "\n";
		}
		cout << "  ]\n";
	}
	cout << "}\n";
}

int main(int argc, char* argv[]) {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	if (argc != 3 || (string(argv[1]) != "tokenize" && string(argv[1]) != "parse")) {
		cerr << "Usage: ./compiler <tokenize|parse> <input_file>\n";
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
	} else {
		Parser parser(lexer.getTokens());
		Json ast = parser.parseProgram();
		printParseJson(ast, lexer.getErrors(), parser.getErrors());
	}
	return 0;
}
