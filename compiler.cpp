#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <cmath>
#include <iomanip>

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

static int jsonIntField(const Json& object, const string& key, int fallback = 0) {
	const Json* value = jsonField(object, key);
	if (!value || value->kind != Json::NUMBER) {
		return fallback;
	}
	return atoi(value->text.c_str());
}

static const vector<Json>& jsonArrayField(const Json& object, const string& key) {
	static const vector<Json> empty;
	const Json* value = jsonField(object, key);
	if (!value || value->kind != Json::ARRAY) {
		return empty;
	}
	return value->arrayItems;
}

static string jsonNodeType(const Json& node) {
	return jsonStringField(node, "type");
}

static int jsonNodeLine(const Json& node) {
	return jsonIntField(node, "line", 0);
}

static int jsonNodeCol(const Json& node) {
	return jsonIntField(node, "col", 0);
}

static bool jsonHasField(const Json& object, const string& key) {
	return jsonField(object, key) != nullptr;
}

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

struct Scope {
	string name;
	Scope* parent;
	map<string, SymbolEntry> symbols;
};

static bool endsWith(const string& value, const string& suffix) {
	return value.size() >= suffix.size() &&
		value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool isUnknownType(const string& type) {
	return type.empty() || type.find("<unknown>") != string::npos;
}

static bool isArrayType(const string& type) {
	return endsWith(type, "[]");
}

static string elementType(const string& type) {
	if (!isArrayType(type)) {
		return "<unknown>";
	}
	return type.substr(0, type.size() - 2);
}

static bool isNumericType(const string& type) {
	return type == "int" || type == "float";
}

static bool isScalarType(const string& type) {
	return type == "int" || type == "float" || type == "bool" || type == "str";
}

static string joinTypes(const vector<string>& types) {
	string result;
	for (size_t i = 0; i < types.size(); ++i) {
		if (i > 0) {
			result += ",";
		}
		result += types[i];
	}
	return result;
}

static string functionSignature(const string& returnType, const vector<string>& paramTypes) {
	return returnType + "(" + joinTypes(paramTypes) + ")";
}

class SemanticAnalyzer {
public:
	SemanticAnalyzer() {
		globalScope = createScope("global", nullptr);
		currentScope = globalScope;
		declareBuiltin("int", "int");
		declareBuiltin("float", "float");
		declareBuiltin("bool", "bool");
		declareBuiltin("str", "str");
	}

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

	const vector<SemanticError>& getErrors() const {
		return errors;
	}

	const vector<SymbolRecord>& getSymbols() const {
		return records;
	}

private:
	static bool symbolLess(const SymbolRecord& a, const SymbolRecord& b) {
		if (a.line != b.line) {
			return a.line < b.line;
		}
		if (a.col != b.col) {
			return a.col < b.col;
		}
		if (a.scope != b.scope) {
			return a.scope < b.scope;
		}
		return a.name < b.name;
	}

	vector<SemanticError> errors;
	vector<SymbolRecord> records;
	vector<Scope*> ownedScopes;
	Scope* globalScope;
	Scope* currentScope;
	string currentFunctionName;
	string currentReturnType = "void";
	int scopeCounter = 0;

	Scope* createScope(const string& name, Scope* parent) {
		Scope* scope = new Scope();
		scope->name = name;
		scope->parent = parent;
		ownedScopes.push_back(scope);
		return scope;
	}

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

	const SymbolEntry* lookup(const string& name) const {
		for (Scope* scope = currentScope; scope != nullptr; scope = scope->parent) {
			map<string, SymbolEntry>::const_iterator it = scope->symbols.find(name);
			if (it != scope->symbols.end()) {
				return &it->second;
			}
		}
		return nullptr;
	}

	bool existsInCurrentScope(const string& name) const {
		return currentScope->symbols.find(name) != currentScope->symbols.end();
	}

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

	void addError(const SemanticError& error) {
		errors.push_back(error);
	}

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

	void addRedeclared(const string& symbol, int line, int col) {
		SemanticError error;
		error.type = "REDECLARED";
		error.symbol = symbol;
		error.expectedCount = 0;
		error.foundCount = 0;
		error.line = line;
		error.col = col;
		error.hasSymbol = true;
		error.hasExpected = false;
		error.hasFound = false;
		error.hasExpectedCount = false;
		error.hasFoundCount = false;
		addError(error);
	}

	void addTypeMismatch(const string& expected, const string& found, const Json& node) {
		if (isUnknownType(found)) {
			return;
		}
		SemanticError error;
		error.type = "TYPE_MISMATCH";
		error.expected = expected;
		error.found = found;
		error.expectedCount = 0;
		error.foundCount = 0;
		error.line = jsonNodeLine(node);
		error.col = jsonNodeCol(node);
		error.hasSymbol = false;
		error.hasExpected = true;
		error.hasFound = true;
		error.hasExpectedCount = false;
		error.hasFoundCount = false;
		addError(error);
	}

	void addArgCountMismatch(const string& symbol, int expected, int found, const Json& node) {
		SemanticError error;
		error.type = "ARG_COUNT_MISMATCH";
		error.symbol = symbol;
		error.expectedCount = expected;
		error.foundCount = found;
		error.line = jsonNodeLine(node);
		error.col = jsonNodeCol(node);
		error.hasSymbol = true;
		error.hasExpected = false;
		error.hasFound = false;
		error.hasExpectedCount = true;
		error.hasFoundCount = true;
		addError(error);
	}

	void addArgTypeMismatch(const string& symbol, const string& expected, const string& found, const Json& node) {
		if (isUnknownType(found)) {
			return;
		}
		SemanticError error;
		error.type = "ARG_TYPE_MISMATCH";
		error.symbol = symbol;
		error.expected = expected;
		error.found = found;
		error.expectedCount = 0;
		error.foundCount = 0;
		error.line = jsonNodeLine(node);
		error.col = jsonNodeCol(node);
		error.hasSymbol = true;
		error.hasExpected = true;
		error.hasFound = true;
		error.hasExpectedCount = false;
		error.hasFoundCount = false;
		addError(error);
	}

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

	string commonType(const string& left, const string& right) const {
		if (isUnknownType(left)) {
			return right;
		}
		if (isUnknownType(right)) {
			return left;
		}
		if (left == right) {
			return left;
		}
		if (isNumericType(left) && isNumericType(right)) {
			return left == "float" || right == "float" ? "float" : "int";
		}
		if (isArrayType(left) && isArrayType(right)) {
			string inner = commonType(elementType(left), elementType(right));
			return isUnknownType(inner) ? string("<unknown>") : inner + "[]";
		}
		return "<unknown>";
	}

	bool checkType(const string& expected, const string& found, const Json& node) {
		if (!areTypesCompatible(expected, found)) {
			addTypeMismatch(expected, found, node);
			return false;
		}
		return true;
	}

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

	void analyzeFunction(const Json& func) {
		string savedFunction = currentFunctionName;
		string savedReturnType = currentReturnType;

		currentFunctionName = jsonStringField(func, "name");
		currentReturnType = jsonHasField(func, "returnType")
			? jsonStringField(*jsonField(func, "returnType"), "name", "void")
			: "void";

		Scope* savedScope = currentScope;
		currentScope = createScope(currentFunctionName, globalScope);

		const vector<Json>& params = jsonArrayField(func, "params");
		for (size_t i = 0; i < params.size(); ++i) {
			const Json* paramType = jsonField(params[i], "paramType");
			SymbolEntry entry;
			entry.name = jsonStringField(params[i], "name");
			entry.type = paramType ? jsonStringField(*paramType, "name", "<unknown>") : "<unknown>";
			entry.scope = currentScope->name;
			entry.returnType.clear();
			entry.line = jsonNodeLine(params[i]);
			entry.col = jsonNodeCol(params[i]);
			entry.isFunction = false;
			entry.builtin = false;
			declareSymbol(entry);
		}

		const Json* body = jsonField(func, "body");
		if (body) {
			analyzeBlock(*body, false, "");
		}

		currentScope = savedScope;
		currentFunctionName = savedFunction;
		currentReturnType = savedReturnType;
	}

	void analyzeBlock(const Json& block, bool createScopeNow, const string& scopeName) {
		Scope* savedScope = currentScope;
		if (createScopeNow) {
			currentScope = createScope(scopeName, currentScope);
		}

		const vector<Json>& stmts = jsonArrayField(block, "stmts");
		for (size_t i = 0; i < stmts.size(); ++i) {
			analyzeStatement(stmts[i]);
		}

		if (createScopeNow) {
			currentScope = savedScope;
		}
	}

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

	bool areTypesComparable(const string& left, const string& right) const {
		if (isUnknownType(left) || isUnknownType(right)) {
			return true;
		}
		if (left == right) {
			return true;
		}
		return isNumericType(left) && isNumericType(right);
	}

	bool isBuiltinCast(const string& name) const {
		return name == "int" || name == "float" || name == "bool" || name == "str";
	}

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
};

struct IRInstruction {
	enum Kind {
		NOP,
		LABEL,
		ASSIGN,
		BINARY,
		UNARY,
		GOTO,
		IF_FALSE,
		RETURN,
		CALL,
		INDEX_LOAD,
		INDEX_STORE,
		ARRAY_LITERAL
	} kind;

	string label;
	string target;
	string dest;
	string arg1;
	string arg2;
	string op;
	string callee;
	vector<string> args;

	IRInstruction() : kind(NOP) {}
};

struct IRFunction {
	string name;
	string returnType;
	vector<pair<string, string> > params;
	vector<IRInstruction> instructions;
};

struct IRProgram {
	vector<IRFunction> functions;
};

struct ConstValue {
	enum Kind {
		UNKNOWN,
		INT,
		FLOAT,
		BOOL,
		STRING
	} kind;

	long long intValue;
	double floatValue;
	bool boolValue;
	string stringValue;

	ConstValue() : kind(UNKNOWN), intValue(0), floatValue(0.0), boolValue(false) {}

	static ConstValue makeInt(long long value) {
		ConstValue c;
		c.kind = INT;
		c.intValue = value;
		c.floatValue = static_cast<double>(value);
		return c;
	}

	static ConstValue makeFloat(double value) {
		ConstValue c;
		c.kind = FLOAT;
		c.floatValue = value;
		return c;
	}

	static ConstValue makeBool(bool value) {
		ConstValue c;
		c.kind = BOOL;
		c.boolValue = value;
		return c;
	}

	static ConstValue makeString(const string& value) {
		ConstValue c;
		c.kind = STRING;
		c.stringValue = value;
		return c;
	}

	bool known() const {
		return kind != UNKNOWN;
	}
};

static string trimFloatLiteral(const string& text) {
	string result = text;
	size_t ePos = result.find_first_of("eE");
	string mantissa = ePos == string::npos ? result : result.substr(0, ePos);
	string exponent = ePos == string::npos ? "" : result.substr(ePos);
	size_t dotPos = mantissa.find('.');
	if (dotPos != string::npos) {
		while (!mantissa.empty() && mantissa[mantissa.size() - 1] == '0') {
			mantissa.erase(mantissa.size() - 1);
		}
		if (!mantissa.empty() && mantissa[mantissa.size() - 1] == '.') {
			mantissa += '0';
		}
	}
	return mantissa + exponent;
}

static string formatDoubleLiteral(double value) {
	ostringstream out;
	out << setprecision(15) << value;
	return trimFloatLiteral(out.str());
}

static string quoteStringLiteral(const string& value) {
	return "\"" + escapeJson(value) + "\"";
}

static bool isBoolLiteralText(const string& text) {
	return text == "true" || text == "false";
}

static bool isNumericLiteralText(const string& text) {
	if (text.empty()) {
		return false;
	}
	size_t pos = 0;
	if (text[pos] == '-') {
		++pos;
	}
	bool hasDigit = false;
	bool hasDot = false;
	for (; pos < text.size(); ++pos) {
		char c = text[pos];
		if (isDigit(c)) {
			hasDigit = true;
		} else if (c == '.' && !hasDot) {
			hasDot = true;
		} else {
			return false;
		}
	}
	return hasDigit;
}

static bool isStringLiteralText(const string& text) {
	return text.size() >= 2 && text[0] == '"' && text[text.size() - 1] == '"';
}

static string unescapeStringLiteral(const string& value) {
	string result;
	for (size_t i = 1; i + 1 < value.size(); ++i) {
		if (value[i] == '\\' && i + 1 < value.size() - 1) {
			char next = value[++i];
			if (next == 'n') {
				result += '\n';
			} else if (next == 't') {
				result += '\t';
			} else if (next == 'r') {
				result += '\r';
			} else {
				result += next;
			}
		} else {
			result += value[i];
		}
	}
	return result;
}

static ConstValue parseScalarConstant(const string& operand) {
	if (isBoolLiteralText(operand)) {
		return ConstValue::makeBool(operand == "true");
	}
	if (isStringLiteralText(operand)) {
		return ConstValue::makeString(unescapeStringLiteral(operand));
	}
	if (isNumericLiteralText(operand)) {
		if (operand.find('.') != string::npos) {
			return ConstValue::makeFloat(strtod(operand.c_str(), nullptr));
		}
		return ConstValue::makeInt(atoll(operand.c_str()));
	}
	return ConstValue();
}

static string constToOperand(const ConstValue& value) {
	switch (value.kind) {
		case ConstValue::INT:
			return to_string(value.intValue);
		case ConstValue::FLOAT:
			return formatDoubleLiteral(value.floatValue);
		case ConstValue::BOOL:
			return value.boolValue ? "true" : "false";
		case ConstValue::STRING:
			return quoteStringLiteral(value.stringValue);
		default:
			return "";
	}
}

static bool isZeroConst(const ConstValue& value) {
	if (value.kind == ConstValue::INT) {
		return value.intValue == 0;
	}
	if (value.kind == ConstValue::FLOAT) {
		return fabs(value.floatValue) < 1e-12;
	}
	return false;
}

static bool isOneConst(const ConstValue& value) {
	if (value.kind == ConstValue::INT) {
		return value.intValue == 1;
	}
	if (value.kind == ConstValue::FLOAT) {
		return fabs(value.floatValue - 1.0) < 1e-12;
	}
	return false;
}

static bool sameConstant(const ConstValue& a, const ConstValue& b) {
	if (a.kind != b.kind) {
		if ((a.kind == ConstValue::INT || a.kind == ConstValue::FLOAT) &&
			(b.kind == ConstValue::INT || b.kind == ConstValue::FLOAT)) {
			double left = a.kind == ConstValue::INT ? static_cast<double>(a.intValue) : a.floatValue;
			double right = b.kind == ConstValue::INT ? static_cast<double>(b.intValue) : b.floatValue;
			return fabs(left - right) < 1e-12;
		}
		return false;
	}
	switch (a.kind) {
		case ConstValue::INT:
			return a.intValue == b.intValue;
		case ConstValue::FLOAT:
			return fabs(a.floatValue - b.floatValue) < 1e-12;
		case ConstValue::BOOL:
			return a.boolValue == b.boolValue;
		case ConstValue::STRING:
			return a.stringValue == b.stringValue;
		default:
			return false;
	}
}

static bool isVariableOperand(const string& operand) {
	if (operand.empty()) {
		return false;
	}
	return !parseScalarConstant(operand).known();
}

static bool evaluateUnaryConstant(const string& op, const ConstValue& value, ConstValue& out) {
	if (!value.known()) {
		return false;
	}
	if (op == "+") {
		if (value.kind == ConstValue::INT || value.kind == ConstValue::FLOAT) {
			out = value;
			return true;
		}
		return false;
	}
	if (op == "-") {
		if (value.kind == ConstValue::INT) {
			out = ConstValue::makeInt(-value.intValue);
			return true;
		}
		if (value.kind == ConstValue::FLOAT) {
			out = ConstValue::makeFloat(-value.floatValue);
			return true;
		}
		return false;
	}
	if (op == "not" && value.kind == ConstValue::BOOL) {
		out = ConstValue::makeBool(!value.boolValue);
		return true;
	}
	if (op == "~" && value.kind == ConstValue::INT) {
		out = ConstValue::makeInt(~value.intValue);
		return true;
	}
	return false;
}

static bool evaluateNumericBinary(const string& op, const ConstValue& left, const ConstValue& right, ConstValue& out) {
	bool useFloat = left.kind == ConstValue::FLOAT || right.kind == ConstValue::FLOAT;
	double leftFloat = left.kind == ConstValue::FLOAT ? left.floatValue : static_cast<double>(left.intValue);
	double rightFloat = right.kind == ConstValue::FLOAT ? right.floatValue : static_cast<double>(right.intValue);
	long long leftInt = left.kind == ConstValue::INT ? left.intValue : static_cast<long long>(left.floatValue);
	long long rightInt = right.kind == ConstValue::INT ? right.intValue : static_cast<long long>(right.floatValue);

	if (op == "+") {
		out = useFloat ? ConstValue::makeFloat(leftFloat + rightFloat) : ConstValue::makeInt(leftInt + rightInt);
		return true;
	}
	if (op == "-") {
		out = useFloat ? ConstValue::makeFloat(leftFloat - rightFloat) : ConstValue::makeInt(leftInt - rightInt);
		return true;
	}
	if (op == "*") {
		out = useFloat ? ConstValue::makeFloat(leftFloat * rightFloat) : ConstValue::makeInt(leftInt * rightInt);
		return true;
	}
	if (op == "/") {
		if ((useFloat && fabs(rightFloat) < 1e-12) || (!useFloat && rightInt == 0)) {
			return false;
		}
		out = useFloat ? ConstValue::makeFloat(leftFloat / rightFloat) : ConstValue::makeInt(leftInt / rightInt);
		return true;
	}
	if (op == "<") {
		out = ConstValue::makeBool(leftFloat < rightFloat);
		return true;
	}
	if (op == "<=") {
		out = ConstValue::makeBool(leftFloat <= rightFloat);
		return true;
	}
	if (op == ">") {
		out = ConstValue::makeBool(leftFloat > rightFloat);
		return true;
	}
	if (op == ">=") {
		out = ConstValue::makeBool(leftFloat >= rightFloat);
		return true;
	}
	return false;
}

static bool evaluateEqualityBinary(const string& op, const ConstValue& left, const ConstValue& right, ConstValue& out) {
	bool equal = false;
	if ((left.kind == ConstValue::INT || left.kind == ConstValue::FLOAT) &&
		(right.kind == ConstValue::INT || right.kind == ConstValue::FLOAT)) {
		double leftValue = left.kind == ConstValue::INT ? static_cast<double>(left.intValue) : left.floatValue;
		double rightValue = right.kind == ConstValue::INT ? static_cast<double>(right.intValue) : right.floatValue;
		equal = fabs(leftValue - rightValue) < 1e-12;
	} else if (left.kind == right.kind) {
		equal = sameConstant(left, right);
	} else {
		return false;
	}
	out = ConstValue::makeBool(op == "==" ? equal : !equal);
	return true;
}

static bool evaluateBitwiseBinary(const string& op, const ConstValue& left, const ConstValue& right, ConstValue& out) {
	if (left.kind != ConstValue::INT || right.kind != ConstValue::INT) {
		return false;
	}
	if (op == "%") {
		if (right.intValue == 0) {
			return false;
		}
		out = ConstValue::makeInt(left.intValue % right.intValue);
		return true;
	}
	if (op == "|") {
		out = ConstValue::makeInt(left.intValue | right.intValue);
		return true;
	}
	if (op == "&") {
		out = ConstValue::makeInt(left.intValue & right.intValue);
		return true;
	}
	if (op == "^") {
		out = ConstValue::makeInt(left.intValue ^ right.intValue);
		return true;
	}
	if (op == "<<") {
		out = ConstValue::makeInt(left.intValue << right.intValue);
		return true;
	}
	if (op == ">>") {
		out = ConstValue::makeInt(left.intValue >> right.intValue);
		return true;
	}
	return false;
}

static bool evaluateLogicalBinary(const string& op, const ConstValue& left, const ConstValue& right, ConstValue& out) {
	if (left.kind != ConstValue::BOOL || right.kind != ConstValue::BOOL) {
		return false;
	}
	if (op == "and") {
		out = ConstValue::makeBool(left.boolValue && right.boolValue);
		return true;
	}
	if (op == "or") {
		out = ConstValue::makeBool(left.boolValue || right.boolValue);
		return true;
	}
	return false;
}

static bool evaluateBinaryConstant(const string& op, const ConstValue& left, const ConstValue& right, ConstValue& out) {
	if (!left.known() || !right.known()) {
		return false;
	}
	if (op == "==" || op == "!=") {
		return evaluateEqualityBinary(op, left, right, out);
	}
	if (op == "and" || op == "or") {
		return evaluateLogicalBinary(op, left, right, out);
	}
	if (op == "|" || op == "&" || op == "^" || op == "<<" || op == ">>" || op == "%") {
		return evaluateBitwiseBinary(op, left, right, out);
	}
	return evaluateNumericBinary(op, left, right, out);
}

static bool evaluateCastConstant(const string& callee, const ConstValue& value, ConstValue& out) {
	if (!value.known()) {
		return false;
	}
	if (callee == "int") {
		if (value.kind == ConstValue::INT) {
			out = value;
		} else if (value.kind == ConstValue::FLOAT) {
			out = ConstValue::makeInt(static_cast<long long>(value.floatValue));
		} else if (value.kind == ConstValue::BOOL) {
			out = ConstValue::makeInt(value.boolValue ? 1 : 0);
		} else if (value.kind == ConstValue::STRING) {
			out = ConstValue::makeInt(atoi(value.stringValue.c_str()));
		} else {
			return false;
		}
		return true;
	}
	if (callee == "float") {
		if (value.kind == ConstValue::INT) {
			out = ConstValue::makeFloat(static_cast<double>(value.intValue));
		} else if (value.kind == ConstValue::FLOAT) {
			out = value;
		} else if (value.kind == ConstValue::BOOL) {
			out = ConstValue::makeFloat(value.boolValue ? 1.0 : 0.0);
		} else if (value.kind == ConstValue::STRING) {
			out = ConstValue::makeFloat(strtod(value.stringValue.c_str(), nullptr));
		} else {
			return false;
		}
		return true;
	}
	if (callee == "bool") {
		if (value.kind == ConstValue::INT) {
			out = ConstValue::makeBool(value.intValue != 0);
		} else if (value.kind == ConstValue::FLOAT) {
			out = ConstValue::makeBool(fabs(value.floatValue) > 1e-12);
		} else if (value.kind == ConstValue::BOOL) {
			out = value;
		} else if (value.kind == ConstValue::STRING) {
			out = ConstValue::makeBool(!value.stringValue.empty() &&
				value.stringValue != "0" && value.stringValue != "false");
		} else {
			return false;
		}
		return true;
	}
	if (callee == "str") {
		if (value.kind == ConstValue::INT) {
			out = ConstValue::makeString(to_string(value.intValue));
		} else if (value.kind == ConstValue::FLOAT) {
			out = ConstValue::makeString(formatDoubleLiteral(value.floatValue));
		} else if (value.kind == ConstValue::BOOL) {
			out = ConstValue::makeString(value.boolValue ? "true" : "false");
		} else if (value.kind == ConstValue::STRING) {
			out = value;
		} else {
			return false;
		}
		return true;
	}
	return false;
}

static string joinOperands(const vector<string>& items, const string& separator) {
	string result;
	for (size_t i = 0; i < items.size(); ++i) {
		if (i > 0) {
			result += separator;
		}
		result += items[i];
	}
	return result;
}

static string renderIndexedOperand(const string& base, const vector<string>& indices) {
	string result = base;
	for (size_t i = 0; i < indices.size(); ++i) {
		result += "[" + indices[i] + "]";
	}
	return result;
}

static bool isPureBuiltinCall(const string& callee) {
	return callee == "int" || callee == "float" || callee == "bool" || callee == "str";
}

class TacGenerator {
public:
	TacGenerator() : currentFunction(nullptr), symbolCounter(0), tempCounter(0), labelCounter(0) {}

	IRProgram generate(const Json& program) {
		output.functions.clear();
		const vector<Json>& body = jsonArrayField(program, "body");
		vector<const Json*> topLevel;
		for (size_t i = 0; i < body.size(); ++i) {
			if (jsonNodeType(body[i]) == "FuncDecl") {
				generateFunction(body[i]);
			} else {
				topLevel.push_back(&body[i]);
			}
		}
		if (!topLevel.empty()) {
			generateTopLevel(topLevel);
		}
		return output;
	}

private:
	struct IndexedAccess {
		string base;
		vector<string> indices;
	};

	struct ConditionalClause {
		const Json* cond;
		const Json* body;
	};

	IRProgram output;
	IRFunction* currentFunction;
	vector< map<string, string> > scopes;
	int symbolCounter;
	int tempCounter;
	int labelCounter;

	void pushScope() {
		scopes.push_back(map<string, string>());
	}

	void popScope() {
		if (!scopes.empty()) {
			scopes.pop_back();
		}
	}

	string declareVariable(const string& sourceName) {
		string unique = sourceName + "__" + to_string(++symbolCounter);
		if (scopes.empty()) {
			pushScope();
		}
		scopes.back()[sourceName] = unique;
		return unique;
	}

	string lookupVariable(const string& sourceName) const {
		for (size_t i = scopes.size(); i > 0; --i) {
			map<string, string>::const_iterator it = scopes[i - 1].find(sourceName);
			if (it != scopes[i - 1].end()) {
				return it->second;
			}
		}
		return sourceName;
	}

	string newTemp() {
		return "__t" + to_string(++tempCounter);
	}

	string newLabel(const string& prefix) {
		return prefix + "_" + to_string(++labelCounter);
	}

	void emit(const IRInstruction& instruction) {
		if (currentFunction) {
			currentFunction->instructions.push_back(instruction);
		}
	}

	void emitLabel(const string& label) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::LABEL;
		instruction.label = label;
		emit(instruction);
	}

	void emitGoto(const string& target) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::GOTO;
		instruction.target = target;
		emit(instruction);
	}

	void emitIfFalse(const string& cond, const string& target) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::IF_FALSE;
		instruction.arg1 = cond;
		instruction.target = target;
		emit(instruction);
	}

	void emitReturn(const string& value) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::RETURN;
		instruction.arg1 = value;
		emit(instruction);
	}

	void emitAssign(const string& dest, const string& value) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::ASSIGN;
		instruction.dest = dest;
		instruction.arg1 = value;
		emit(instruction);
	}

	void emitUnary(const string& dest, const string& op, const string& operand) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::UNARY;
		instruction.dest = dest;
		instruction.op = op;
		instruction.arg1 = operand;
		emit(instruction);
	}

	void emitBinary(const string& dest, const string& left, const string& op, const string& right) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::BINARY;
		instruction.dest = dest;
		instruction.arg1 = left;
		instruction.arg2 = right;
		instruction.op = op;
		emit(instruction);
	}

	void emitCall(const string& dest, const string& callee, const vector<string>& args) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::CALL;
		instruction.dest = dest;
		instruction.callee = callee;
		instruction.args = args;
		emit(instruction);
	}

	void emitArrayLiteral(const string& dest, const vector<string>& elements) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::ARRAY_LITERAL;
		instruction.dest = dest;
		instruction.args = elements;
		emit(instruction);
	}

	void emitIndexLoad(const string& dest, const string& base, const vector<string>& indices) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::INDEX_LOAD;
		instruction.dest = dest;
		instruction.arg1 = base;
		instruction.args = indices;
		emit(instruction);
	}

	void emitIndexStore(const string& base, const vector<string>& indices, const string& value) {
		IRInstruction instruction;
		instruction.kind = IRInstruction::INDEX_STORE;
		instruction.dest = base;
		instruction.arg1 = value;
		instruction.args = indices;
		emit(instruction);
	}

	void generateFunction(const Json& func) {
		IRFunction irFunction;
		irFunction.name = jsonStringField(func, "name");
		irFunction.returnType = jsonHasField(func, "returnType")
			? jsonStringField(*jsonField(func, "returnType"), "name", "void")
			: "void";
		output.functions.push_back(irFunction);
		currentFunction = &output.functions.back();

		pushScope();
		const vector<Json>& params = jsonArrayField(func, "params");
		for (size_t i = 0; i < params.size(); ++i) {
			string unique = declareVariable(jsonStringField(params[i], "name"));
			const Json* paramType = jsonField(params[i], "paramType");
			string typeName = paramType ? jsonStringField(*paramType, "name", "<unknown>") : "<unknown>";
			currentFunction->params.push_back(make_pair(unique, typeName));
		}

		const Json* body = jsonField(func, "body");
		if (body) {
			generateBlock(*body, false);
		}
		if (currentFunction->instructions.empty() ||
			currentFunction->instructions.back().kind != IRInstruction::RETURN) {
			emitReturn("");
		}

		popScope();
		currentFunction = nullptr;
	}

	void generateTopLevel(const vector<const Json*>& statements) {
		IRFunction irFunction;
		irFunction.name = "__top_level";
		irFunction.returnType = "void";
		output.functions.push_back(irFunction);
		currentFunction = &output.functions.back();

		pushScope();
		for (size_t i = 0; i < statements.size(); ++i) {
			generateStatement(*statements[i]);
		}
		if (currentFunction->instructions.empty() ||
			currentFunction->instructions.back().kind != IRInstruction::RETURN) {
			emitReturn("");
		}
		popScope();
		currentFunction = nullptr;
	}

	void generateBlock(const Json& block, bool createScope) {
		if (createScope) {
			pushScope();
		}
		const vector<Json>& stmts = jsonArrayField(block, "stmts");
		for (size_t i = 0; i < stmts.size(); ++i) {
			generateStatement(stmts[i]);
		}
		if (createScope) {
			popScope();
		}
	}

	void generateStatement(const Json& stmt) {
		string type = jsonNodeType(stmt);
		if (type == "VarDecl") {
			generateVarDecl(stmt);
		} else if (type == "AssignStmt") {
			generateAssign(stmt);
		} else if (type == "ExprStmt") {
			const Json* expr = jsonField(stmt, "expr");
			if (expr) {
				generateExpr(*expr);
			}
		} else if (type == "ReturnStmt") {
			const Json* value = jsonField(stmt, "value");
			emitReturn(value ? generateExpr(*value) : "");
		} else if (type == "IfStmt") {
			generateIf(stmt);
		} else if (type == "WhileStmt") {
			generateWhile(stmt);
		} else if (type == "ForStmt") {
			generateFor(stmt);
		} else if (type == "Block") {
			generateBlock(stmt, true);
		}
	}

	void generateVarDecl(const Json& stmt) {
		const vector<Json>& declarators = jsonArrayField(stmt, "declarators");
		for (size_t i = 0; i < declarators.size(); ++i) {
			const Json* init = jsonField(declarators[i], "init");
			string initValue;
			bool hasInit = init != nullptr;
			if (init) {
				initValue = generateExpr(*init);
			}
			string unique = declareVariable(jsonStringField(declarators[i], "name"));
			if (hasInit) {
				emitAssign(unique, initValue);
			}
		}
	}

	void generateAssign(const Json& stmt) {
		const Json* target = jsonField(stmt, "target");
		const Json* value = jsonField(stmt, "value");
		if (!target || !value) {
			return;
		}
		string assignedValue = generateExpr(*value);
		if (jsonNodeType(*target) == "Identifier") {
			emitAssign(lookupVariable(jsonStringField(*target, "name")), assignedValue);
			return;
		}
		if (jsonNodeType(*target) == "IndexExpr") {
			IndexedAccess access = generateIndexedAccess(*target);
			emitIndexStore(access.base, access.indices, assignedValue);
		}
	}

	void generateIf(const Json& stmt) {
		vector<ConditionalClause> clauses;
		const Json* cond = jsonField(stmt, "cond");
		const Json* thenBlock = jsonField(stmt, "then");
		if (cond && thenBlock) {
			ConditionalClause clause;
			clause.cond = cond;
			clause.body = thenBlock;
			clauses.push_back(clause);
		}

		const vector<Json>& elifs = jsonArrayField(stmt, "elif");
		for (size_t i = 0; i < elifs.size(); ++i) {
			ConditionalClause clause;
			clause.cond = jsonField(elifs[i], "cond");
			clause.body = jsonField(elifs[i], "body");
			if (clause.cond && clause.body) {
				clauses.push_back(clause);
			}
		}

		const Json* elseBody = jsonField(stmt, "elseBody");
		string endLabel = newLabel("if_end");
		string fallthroughLabel = endLabel;

		for (size_t i = 0; i < clauses.size(); ++i) {
			bool hasNextClause = i + 1 < clauses.size();
			bool hasElse = elseBody != nullptr;
			fallthroughLabel = hasNextClause ? newLabel("if_next")
				: (hasElse ? newLabel("if_else") : endLabel);
			emitIfFalse(generateExpr(*clauses[i].cond), fallthroughLabel);
			generateBlock(*clauses[i].body, true);
			if (hasNextClause || hasElse) {
				emitGoto(endLabel);
			}
			if (fallthroughLabel != endLabel || hasElse) {
				emitLabel(fallthroughLabel);
			}
		}

		if (elseBody) {
			generateBlock(*elseBody, true);
		}
		emitLabel(endLabel);
	}

	void generateWhile(const Json& stmt) {
		const Json* cond = jsonField(stmt, "cond");
		const Json* body = jsonField(stmt, "body");
		string beginLabel = newLabel("while_begin");
		string endLabel = newLabel("while_end");
		emitLabel(beginLabel);
		if (cond) {
			emitIfFalse(generateExpr(*cond), endLabel);
		}
		if (body) {
			generateBlock(*body, true);
		}
		emitGoto(beginLabel);
		emitLabel(endLabel);
	}

	void generateFor(const Json& stmt) {
		const Json* start = jsonField(stmt, "start");
		const Json* end = jsonField(stmt, "end");
		const Json* step = jsonField(stmt, "step");
		string startValue = start ? generateExpr(*start) : "0";
		string endValue = end ? generateExpr(*end) : "0";
		string stepValue = step ? generateExpr(*step) : "1";

		pushScope();
		string iter = declareVariable(jsonStringField(stmt, "var"));
		emitAssign(iter, startValue);

		string checkLabel = newLabel("for_check");
		string negLabel = newLabel("for_neg");
		string bodyLabel = newLabel("for_body");
		string endLabel = newLabel("for_end");
		emitLabel(checkLabel);

		string dirTemp = newTemp();
		emitBinary(dirTemp, stepValue, ">", "0");
		emitIfFalse(dirTemp, negLabel);

		string posCond = newTemp();
		emitBinary(posCond, iter, "<", endValue);
		emitIfFalse(posCond, endLabel);
		emitGoto(bodyLabel);

		emitLabel(negLabel);
		string negCond = newTemp();
		emitBinary(negCond, iter, ">", endValue);
		emitIfFalse(negCond, endLabel);

		emitLabel(bodyLabel);
		const Json* body = jsonField(stmt, "body");
		if (body) {
			generateBlock(*body, false);
		}

		string nextValue = newTemp();
		emitBinary(nextValue, iter, "+", stepValue);
		emitAssign(iter, nextValue);
		emitGoto(checkLabel);
		emitLabel(endLabel);
		popScope();
	}

	struct IndexAstChain {
		const Json* base;
		vector<const Json*> indices;
		IndexAstChain() : base(nullptr) {}
	};

	IndexAstChain flattenIndexAst(const Json& expr) const {
		IndexAstChain chain;
		const Json* current = &expr;
		vector<const Json*> reversed;
		while (current && jsonNodeType(*current) == "IndexExpr") {
			const Json* index = jsonField(*current, "index");
			if (index) {
				reversed.push_back(index);
			}
			current = jsonField(*current, "array");
		}
		chain.base = current;
		for (size_t i = reversed.size(); i > 0; --i) {
			chain.indices.push_back(reversed[i - 1]);
		}
		return chain;
	}

	IndexedAccess generateIndexedAccess(const Json& expr) {
		IndexAstChain chain = flattenIndexAst(expr);
		IndexedAccess access;
		if (chain.base) {
			access.base = generateExpr(*chain.base);
		}
		for (size_t i = 0; i < chain.indices.size(); ++i) {
			access.indices.push_back(generateExpr(*chain.indices[i]));
		}
		return access;
	}

	string generateExpr(const Json& expr) {
		string type = jsonNodeType(expr);
		if (type == "IntLiteral" || type == "FloatLiteral") {
			return jsonStringField(expr, "value");
		}
		if (type == "StringLiteral") {
			return quoteStringLiteral(jsonStringField(expr, "value"));
		}
		if (type == "BoolLiteral") {
			return jsonStringField(expr, "value") == "true" ? "true" : "false";
		}
		if (type == "Identifier") {
			return lookupVariable(jsonStringField(expr, "name"));
		}
		if (type == "UnaryExpr") {
			const Json* inner = jsonField(expr, "expr");
			if (!inner) {
				return "";
			}
			string temp = newTemp();
			emitUnary(temp, jsonStringField(expr, "op"), generateExpr(*inner));
			return temp;
		}
		if (type == "BinaryExpr") {
			const Json* left = jsonField(expr, "left");
			const Json* right = jsonField(expr, "right");
			if (!left || !right) {
				return "";
			}
			string temp = newTemp();
			emitBinary(temp, generateExpr(*left), jsonStringField(expr, "op"), generateExpr(*right));
			return temp;
		}
		if (type == "Call") {
			const Json* callee = jsonField(expr, "callee");
			string calleeName;
			if (callee && jsonNodeType(*callee) == "Identifier") {
				calleeName = jsonStringField(*callee, "name");
			} else if (callee) {
				calleeName = generateExpr(*callee);
			}
			vector<string> args;
			const vector<Json>& callArgs = jsonArrayField(expr, "args");
			for (size_t i = 0; i < callArgs.size(); ++i) {
				args.push_back(generateExpr(callArgs[i]));
			}
			string temp = newTemp();
			emitCall(temp, calleeName, args);
			return temp;
		}
		if (type == "ArrayLiteral") {
			vector<string> elements;
			const vector<Json>& items = jsonArrayField(expr, "elements");
			for (size_t i = 0; i < items.size(); ++i) {
				elements.push_back(generateExpr(items[i]));
			}
			string temp = newTemp();
			emitArrayLiteral(temp, elements);
			return temp;
		}
		if (type == "IndexExpr") {
			IndexedAccess access = generateIndexedAccess(expr);
			string temp = newTemp();
			emitIndexLoad(temp, access.base, access.indices);
			return temp;
		}
		return "";
	}
};

static string renderInstruction(const IRInstruction& instruction) {
	switch (instruction.kind) {
		case IRInstruction::LABEL:
			return instruction.label + ":";
		case IRInstruction::ASSIGN:
			return "  " + instruction.dest + " = " + instruction.arg1;
		case IRInstruction::BINARY:
			return "  " + instruction.dest + " = " + instruction.arg1 + " " + instruction.op + " " + instruction.arg2;
		case IRInstruction::UNARY:
			return "  " + instruction.dest + " = " + instruction.op + " " + instruction.arg1;
		case IRInstruction::GOTO:
			return "  goto " + instruction.target;
		case IRInstruction::IF_FALSE:
			return "  if_false " + instruction.arg1 + " goto " + instruction.target;
		case IRInstruction::RETURN:
			return instruction.arg1.empty() ? "  ret" : "  ret " + instruction.arg1;
		case IRInstruction::CALL: {
			string callText = "call " + instruction.callee + "(" + joinOperands(instruction.args, ", ") + ")";
			if (instruction.dest.empty()) {
				return "  " + callText;
			}
			return "  " + instruction.dest + " = " + callText;
		}
		case IRInstruction::INDEX_LOAD:
			return "  " + instruction.dest + " = " + renderIndexedOperand(instruction.arg1, instruction.args);
		case IRInstruction::INDEX_STORE:
			return "  " + renderIndexedOperand(instruction.dest, instruction.args) + " = " + instruction.arg1;
		case IRInstruction::ARRAY_LITERAL:
			return "  " + instruction.dest + " = [" + joinOperands(instruction.args, ", ") + "]";
		default:
			return "";
	}
}

static void printTacProgram(const IRProgram& program) {
	for (size_t i = 0; i < program.functions.size(); ++i) {
		const IRFunction& function = program.functions[i];
		cout << "func " << function.name << "(";
		for (size_t j = 0; j < function.params.size(); ++j) {
			if (j > 0) {
				cout << ", ";
			}
			cout << function.params[j].first << ":" << function.params[j].second;
		}
		cout << ") -> " << function.returnType << "\n";
		for (size_t j = 0; j < function.instructions.size(); ++j) {
			string line = renderInstruction(function.instructions[j]);
			if (!line.empty()) {
				cout << line << "\n";
			}
		}
		cout << "endfunc\n";
		if (i + 1 < program.functions.size()) {
			cout << "\n";
		}
	}
}

static bool isBlockTerminator(const IRInstruction& instruction) {
	return instruction.kind == IRInstruction::GOTO ||
		instruction.kind == IRInstruction::IF_FALSE ||
		instruction.kind == IRInstruction::RETURN;
}

static vector<pair<size_t, size_t> > buildBasicBlocks(const vector<IRInstruction>& instructions) {
	vector<pair<size_t, size_t> > blocks;
	if (instructions.empty()) {
		return blocks;
	}
	size_t start = 0;
	while (start < instructions.size()) {
		size_t end = start;
		while (end + 1 < instructions.size() &&
			   instructions[end + 1].kind != IRInstruction::LABEL &&
			   !isBlockTerminator(instructions[end])) {
			++end;
		}
		blocks.push_back(make_pair(start, end));
		start = end + 1;
	}
	return blocks;
}

static bool replaceKnownConstant(string& operand, const map<string, ConstValue>& env) {
	if (!isVariableOperand(operand)) {
		return false;
	}
	map<string, ConstValue>::const_iterator it = env.find(operand);
	if (it == env.end()) {
		return false;
	}
	operand = constToOperand(it->second);
	return true;
}

static bool simplifyInstruction(IRInstruction& instruction) {
	bool changed = false;
	if (instruction.kind == IRInstruction::ASSIGN) {
		if (instruction.dest == instruction.arg1) {
			instruction.kind = IRInstruction::NOP;
			return true;
		}
		return false;
	}
	if (instruction.kind == IRInstruction::UNARY) {
		ConstValue operand = parseScalarConstant(instruction.arg1);
		ConstValue result;
		if (evaluateUnaryConstant(instruction.op, operand, result)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = constToOperand(result);
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "+") {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.op.clear();
			return true;
		}
		return false;
	}
	if (instruction.kind == IRInstruction::BINARY) {
		ConstValue left = parseScalarConstant(instruction.arg1);
		ConstValue right = parseScalarConstant(instruction.arg2);
		ConstValue result;
		if (evaluateBinaryConstant(instruction.op, left, right, result)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = constToOperand(result);
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}

		if (instruction.op == "+" && right.known() && isZeroConst(right)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "+" && left.known() && isZeroConst(left)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = instruction.arg2;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "-" && right.known() && isZeroConst(right)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "-" && instruction.arg1 == instruction.arg2) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = "0";
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "*" && ((left.known() && isZeroConst(left)) || (right.known() && isZeroConst(right)))) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = (left.kind == ConstValue::FLOAT || right.kind == ConstValue::FLOAT) ? "0.0" : "0";
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "*" && right.known() && isOneConst(right)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "*" && left.known() && isOneConst(left)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = instruction.arg2;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "/" && right.known() && isOneConst(right)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "%" && right.kind == ConstValue::INT && right.intValue == 1) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = "0";
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "and" && left.kind == ConstValue::BOOL) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = left.boolValue ? instruction.arg2 : "false";
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "and" && right.kind == ConstValue::BOOL) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = right.boolValue ? instruction.arg1 : "false";
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "or" && left.kind == ConstValue::BOOL) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = left.boolValue ? "true" : instruction.arg2;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "or" && right.kind == ConstValue::BOOL) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = right.boolValue ? "true" : instruction.arg1;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if ((instruction.op == "|" || instruction.op == "^") && right.kind == ConstValue::INT && right.intValue == 0) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if ((instruction.op == "|" || instruction.op == "^") && left.kind == ConstValue::INT && left.intValue == 0) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = instruction.arg2;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if (instruction.op == "&" && ((left.kind == ConstValue::INT && left.intValue == 0) ||
			(right.kind == ConstValue::INT && right.intValue == 0))) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = "0";
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if ((instruction.op == "<<" || instruction.op == ">>") && right.kind == ConstValue::INT && right.intValue == 0) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		if ((instruction.op == "==" || instruction.op == "!=" ||
			 instruction.op == "<" || instruction.op == ">" ||
			 instruction.op == "<=" || instruction.op == ">=") &&
			instruction.arg1 == instruction.arg2) {
			instruction.kind = IRInstruction::ASSIGN;
			if (instruction.op == "==" || instruction.op == "<=" || instruction.op == ">=") {
				instruction.arg1 = "true";
			} else if (instruction.op == "!=" || instruction.op == "<" || instruction.op == ">") {
				instruction.arg1 = "false";
			}
			instruction.arg2.clear();
			instruction.op.clear();
			return true;
		}
		return changed;
	}
	if (instruction.kind == IRInstruction::CALL && instruction.args.size() == 1 && isPureBuiltinCall(instruction.callee)) {
		ConstValue value = parseScalarConstant(instruction.args[0]);
		ConstValue result;
		if (evaluateCastConstant(instruction.callee, value, result)) {
			instruction.kind = IRInstruction::ASSIGN;
			instruction.arg1 = constToOperand(result);
			instruction.callee.clear();
			instruction.args.clear();
			return true;
		}
	}
	return false;
}

static bool runConstantPropagation(IRFunction& function) {
	bool changed = false;
	vector<pair<size_t, size_t> > blocks = buildBasicBlocks(function.instructions);
	for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
		map<string, ConstValue> env;
		for (size_t i = blocks[blockIndex].first; i <= blocks[blockIndex].second; ++i) {
			IRInstruction& instruction = function.instructions[i];
			switch (instruction.kind) {
				case IRInstruction::ASSIGN:
				case IRInstruction::UNARY:
				case IRInstruction::IF_FALSE:
				case IRInstruction::RETURN:
					changed = replaceKnownConstant(instruction.arg1, env) || changed;
					break;
				case IRInstruction::BINARY:
					changed = replaceKnownConstant(instruction.arg1, env) || changed;
					changed = replaceKnownConstant(instruction.arg2, env) || changed;
					break;
				case IRInstruction::CALL:
				case IRInstruction::ARRAY_LITERAL:
				case IRInstruction::INDEX_LOAD:
				case IRInstruction::INDEX_STORE:
					for (size_t j = 0; j < instruction.args.size(); ++j) {
						changed = replaceKnownConstant(instruction.args[j], env) || changed;
					}
					if (instruction.kind == IRInstruction::CALL ||
						instruction.kind == IRInstruction::INDEX_LOAD) {
						changed = replaceKnownConstant(instruction.arg1, env) || changed;
					}
					if (instruction.kind == IRInstruction::INDEX_STORE) {
						changed = replaceKnownConstant(instruction.dest, env) || changed;
						changed = replaceKnownConstant(instruction.arg1, env) || changed;
					}
					break;
				default:
					break;
			}

			changed = simplifyInstruction(instruction) || changed;

			string defined;
			bool hasDefinition = false;
			if (instruction.kind == IRInstruction::ASSIGN || instruction.kind == IRInstruction::UNARY ||
				instruction.kind == IRInstruction::BINARY || instruction.kind == IRInstruction::CALL ||
				instruction.kind == IRInstruction::INDEX_LOAD || instruction.kind == IRInstruction::ARRAY_LITERAL) {
				defined = instruction.dest;
				hasDefinition = !defined.empty();
			}

			if (!hasDefinition) {
				continue;
			}

			if (instruction.kind == IRInstruction::ASSIGN) {
				ConstValue constant = parseScalarConstant(instruction.arg1);
				if (constant.known()) {
					env[defined] = constant;
				} else {
					env.erase(defined);
				}
			} else {
				env.erase(defined);
			}
		}
	}
	return changed;
}

static bool isCommutativeOp(const string& op) {
	return op == "+" || op == "*" || op == "==" || op == "!=" ||
		op == "&" || op == "|" || op == "^" || op == "and" || op == "or";
}

static void appendOperandUse(vector<string>& uses, const string& operand) {
	if (isVariableOperand(operand)) {
		uses.push_back(operand);
	}
}

static vector<string> instructionUses(const IRInstruction& instruction) {
	vector<string> uses;
	switch (instruction.kind) {
		case IRInstruction::ASSIGN:
		case IRInstruction::UNARY:
		case IRInstruction::IF_FALSE:
		case IRInstruction::RETURN:
			appendOperandUse(uses, instruction.arg1);
			break;
		case IRInstruction::BINARY:
			appendOperandUse(uses, instruction.arg1);
			appendOperandUse(uses, instruction.arg2);
			break;
		case IRInstruction::CALL:
			for (size_t i = 0; i < instruction.args.size(); ++i) {
				appendOperandUse(uses, instruction.args[i]);
			}
			break;
		case IRInstruction::ARRAY_LITERAL:
			for (size_t i = 0; i < instruction.args.size(); ++i) {
				appendOperandUse(uses, instruction.args[i]);
			}
			break;
		case IRInstruction::INDEX_LOAD:
			appendOperandUse(uses, instruction.arg1);
			for (size_t i = 0; i < instruction.args.size(); ++i) {
				appendOperandUse(uses, instruction.args[i]);
			}
			break;
		case IRInstruction::INDEX_STORE:
			appendOperandUse(uses, instruction.dest);
			appendOperandUse(uses, instruction.arg1);
			for (size_t i = 0; i < instruction.args.size(); ++i) {
				appendOperandUse(uses, instruction.args[i]);
			}
			break;
		default:
			break;
	}
	return uses;
}

static string instructionDefinedValue(const IRInstruction& instruction) {
	switch (instruction.kind) {
		case IRInstruction::ASSIGN:
		case IRInstruction::UNARY:
		case IRInstruction::BINARY:
		case IRInstruction::CALL:
		case IRInstruction::INDEX_LOAD:
		case IRInstruction::ARRAY_LITERAL:
			return instruction.dest;
		default:
			return "";
	}
}

struct AvailableExpr {
	string dest;
	set<string> deps;
};

static string expressionKey(const IRInstruction& instruction) {
	if (instruction.kind == IRInstruction::BINARY) {
		string left = instruction.arg1;
		string right = instruction.arg2;
		if (isCommutativeOp(instruction.op) && right < left) {
			swap(left, right);
		}
		return "bin|" + instruction.op + "|" + left + "|" + right;
	}
	if (instruction.kind == IRInstruction::UNARY) {
		return "un|" + instruction.op + "|" + instruction.arg1;
	}
	if (instruction.kind == IRInstruction::INDEX_LOAD) {
		return "idx|" + instruction.arg1 + "|" + joinOperands(instruction.args, "|");
	}
	return "";
}

static bool isCseCandidate(const IRInstruction& instruction) {
	return instruction.kind == IRInstruction::BINARY ||
		instruction.kind == IRInstruction::UNARY ||
		instruction.kind == IRInstruction::INDEX_LOAD;
}

static void killAvailableExpressions(map<string, AvailableExpr>& available, const string& symbol) {
	if (symbol.empty()) {
		return;
	}
	vector<string> doomed;
	for (map<string, AvailableExpr>::const_iterator it = available.begin(); it != available.end(); ++it) {
		if (it->second.dest == symbol || it->second.deps.count(symbol) > 0) {
			doomed.push_back(it->first);
		}
	}
	for (size_t i = 0; i < doomed.size(); ++i) {
		available.erase(doomed[i]);
	}
}

static bool runCommonSubexpressionElimination(IRFunction& function) {
	bool changed = false;
	vector<pair<size_t, size_t> > blocks = buildBasicBlocks(function.instructions);
	for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
		map<string, AvailableExpr> available;
		for (size_t i = blocks[blockIndex].first; i <= blocks[blockIndex].second; ++i) {
			IRInstruction& instruction = function.instructions[i];
			string key = expressionKey(instruction);
			vector<string> uses = instructionUses(instruction);
			if (isCseCandidate(instruction) && !key.empty()) {
				map<string, AvailableExpr>::iterator found = available.find(key);
				if (found != available.end()) {
					string prior = found->second.dest;
					if (prior == instruction.dest) {
						instruction.kind = IRInstruction::NOP;
					} else {
						instruction.kind = IRInstruction::ASSIGN;
						instruction.arg1 = prior;
						instruction.arg2.clear();
						instruction.op.clear();
						instruction.args.clear();
					}
					changed = true;
				}
			}

			if (instruction.kind == IRInstruction::INDEX_STORE) {
				available.clear();
				continue;
			}

			string defined = instructionDefinedValue(instruction);
			killAvailableExpressions(available, defined);

			if (instruction.kind == IRInstruction::CALL && !isPureBuiltinCall(instruction.callee)) {
				available.clear();
				continue;
			}

			if (isCseCandidate(instruction) && instruction.kind != IRInstruction::NOP) {
				AvailableExpr expr;
				expr.dest = instruction.dest;
				for (size_t j = 0; j < uses.size(); ++j) {
					expr.deps.insert(uses[j]);
				}
				available[expressionKey(instruction)] = expr;
			}
		}
	}
	return changed;
}

static bool stripNops(IRFunction& function) {
	vector<IRInstruction> kept;
	bool changed = false;
	for (size_t i = 0; i < function.instructions.size(); ++i) {
		if (function.instructions[i].kind == IRInstruction::NOP) {
			changed = true;
			continue;
		}
		kept.push_back(function.instructions[i]);
	}
	if (changed) {
		function.instructions.swap(kept);
	}
	return changed;
}

static map<string, size_t> buildLabelIndex(const vector<IRInstruction>& instructions) {
	map<string, size_t> labels;
	for (size_t i = 0; i < instructions.size(); ++i) {
		if (instructions[i].kind == IRInstruction::LABEL) {
			labels[instructions[i].label] = i;
		}
	}
	return labels;
}

static bool simplifyControlFlow(IRFunction& function) {
	bool changed = false;
	map<string, size_t> labelIndex = buildLabelIndex(function.instructions);
	for (size_t i = 0; i < function.instructions.size(); ++i) {
		IRInstruction& instruction = function.instructions[i];
		if (instruction.kind == IRInstruction::IF_FALSE) {
			ConstValue cond = parseScalarConstant(instruction.arg1);
			if (cond.kind == ConstValue::BOOL) {
				if (cond.boolValue) {
					instruction.kind = IRInstruction::NOP;
				} else {
					instruction.kind = IRInstruction::GOTO;
					instruction.arg1.clear();
				}
				changed = true;
				continue;
			}
			if (i + 1 < function.instructions.size() &&
				function.instructions[i + 1].kind == IRInstruction::LABEL &&
				function.instructions[i + 1].label == instruction.target) {
				instruction.kind = IRInstruction::NOP;
				changed = true;
			}
		} else if (instruction.kind == IRInstruction::GOTO) {
			map<string, size_t>::const_iterator it = labelIndex.find(instruction.target);
			if (it != labelIndex.end() && it->second == i + 1) {
				instruction.kind = IRInstruction::NOP;
				changed = true;
			}
		}
	}
	return changed;
}

static vector<size_t> instructionSuccessors(const vector<IRInstruction>& instructions,
											const map<string, size_t>& labels,
											size_t index) {
	vector<size_t> successors;
	const IRInstruction& instruction = instructions[index];
	if (instruction.kind == IRInstruction::RETURN) {
		return successors;
	}
	if (instruction.kind == IRInstruction::GOTO) {
		map<string, size_t>::const_iterator it = labels.find(instruction.target);
		if (it != labels.end()) {
			successors.push_back(it->second);
		}
		return successors;
	}
	if (instruction.kind == IRInstruction::IF_FALSE) {
		map<string, size_t>::const_iterator it = labels.find(instruction.target);
		if (it != labels.end()) {
			successors.push_back(it->second);
		}
		if (index + 1 < instructions.size()) {
			successors.push_back(index + 1);
		}
		return successors;
	}
	if (index + 1 < instructions.size()) {
		successors.push_back(index + 1);
	}
	return successors;
}

static bool eliminateUnreachableCode(IRFunction& function) {
	if (function.instructions.empty()) {
		return false;
	}
	map<string, size_t> labels = buildLabelIndex(function.instructions);
	vector<bool> reachable(function.instructions.size(), false);
	vector<size_t> worklist;
	worklist.push_back(0);
	while (!worklist.empty()) {
		size_t index = worklist.back();
		worklist.pop_back();
		if (index >= function.instructions.size() || reachable[index]) {
			continue;
		}
		reachable[index] = true;
		vector<size_t> successors = instructionSuccessors(function.instructions, labels, index);
		for (size_t i = 0; i < successors.size(); ++i) {
			if (!reachable[successors[i]]) {
				worklist.push_back(successors[i]);
			}
		}
	}

	bool changed = false;
	vector<IRInstruction> kept;
	for (size_t i = 0; i < function.instructions.size(); ++i) {
		if (!reachable[i]) {
			changed = true;
			continue;
		}
		kept.push_back(function.instructions[i]);
	}
	if (changed) {
		function.instructions.swap(kept);
	}
	return changed;
}

static bool instructionHasSideEffects(const IRInstruction& instruction) {
	if (instruction.kind == IRInstruction::CALL) {
		return !isPureBuiltinCall(instruction.callee);
	}
	return instruction.kind == IRInstruction::INDEX_STORE ||
		instruction.kind == IRInstruction::GOTO ||
		instruction.kind == IRInstruction::IF_FALSE ||
		instruction.kind == IRInstruction::RETURN;
}

static bool eliminateDeadCode(IRFunction& function) {
	size_t n = function.instructions.size();
	if (n == 0) {
		return false;
	}

	map<string, size_t> labels = buildLabelIndex(function.instructions);
	vector< set<string> > useSets(n);
	vector< set<string> > defSets(n);
	vector< vector<size_t> > successors(n);
	for (size_t i = 0; i < n; ++i) {
		vector<string> uses = instructionUses(function.instructions[i]);
		for (size_t j = 0; j < uses.size(); ++j) {
			useSets[i].insert(uses[j]);
		}
		string defined = instructionDefinedValue(function.instructions[i]);
		if (!defined.empty()) {
			defSets[i].insert(defined);
		}
		successors[i] = instructionSuccessors(function.instructions, labels, i);
	}

	vector< set<string> > liveIn(n);
	vector< set<string> > liveOut(n);
	bool updated = true;
	while (updated) {
		updated = false;
		for (size_t offset = 0; offset < n; ++offset) {
			size_t i = n - 1 - offset;
			set<string> newOut;
			for (size_t s = 0; s < successors[i].size(); ++s) {
				newOut.insert(liveIn[successors[i][s]].begin(), liveIn[successors[i][s]].end());
			}
			set<string> newIn = useSets[i];
			for (set<string>::const_iterator it = newOut.begin(); it != newOut.end(); ++it) {
				if (defSets[i].count(*it) == 0) {
					newIn.insert(*it);
				}
			}
			if (newOut != liveOut[i] || newIn != liveIn[i]) {
				liveOut[i] = newOut;
				liveIn[i] = newIn;
				updated = true;
			}
		}
	}

	bool changed = false;
	vector<IRInstruction> kept;
	for (size_t i = 0; i < n; ++i) {
		string defined = instructionDefinedValue(function.instructions[i]);
		if (!defined.empty() &&
			!instructionHasSideEffects(function.instructions[i]) &&
			liveOut[i].count(defined) == 0) {
			changed = true;
			continue;
		}
		kept.push_back(function.instructions[i]);
	}
	if (changed) {
		function.instructions.swap(kept);
	}
	return changed;
}

static bool removeUnusedLabels(IRFunction& function) {
	set<string> referenced;
	for (size_t i = 0; i < function.instructions.size(); ++i) {
		if (function.instructions[i].kind == IRInstruction::GOTO ||
			function.instructions[i].kind == IRInstruction::IF_FALSE) {
			referenced.insert(function.instructions[i].target);
		}
	}

	bool changed = false;
	vector<IRInstruction> kept;
	for (size_t i = 0; i < function.instructions.size(); ++i) {
		if (function.instructions[i].kind == IRInstruction::LABEL &&
			i != 0 && referenced.count(function.instructions[i].label) == 0) {
			changed = true;
			continue;
		}
		kept.push_back(function.instructions[i]);
	}
	if (changed) {
		function.instructions.swap(kept);
	}
	return changed;
}

static void optimizeFunction(IRFunction& function) {
	for (int iteration = 0; iteration < 6; ++iteration) {
		bool changed = false;
		changed = runConstantPropagation(function) || changed;
		changed = runCommonSubexpressionElimination(function) || changed;
		changed = runConstantPropagation(function) || changed;
		changed = simplifyControlFlow(function) || changed;
		changed = stripNops(function) || changed;
		changed = eliminateUnreachableCode(function) || changed;
		changed = eliminateDeadCode(function) || changed;
		changed = removeUnusedLabels(function) || changed;
		changed = stripNops(function) || changed;
		if (!changed) {
			break;
		}
	}
}

static void optimizeProgram(IRProgram& program) {
	for (size_t i = 0; i < program.functions.size(); ++i) {
		optimizeFunction(program.functions[i]);
	}
}

struct RuntimeValue {
	enum Kind {
		VOID,
		INT,
		FLOAT,
		BOOL,
		STRING,
		ARRAY
	} kind;

	long long intValue;
	double floatValue;
	bool boolValue;
	string stringValue;
	vector<RuntimeValue> arrayValue;

	RuntimeValue() : kind(VOID), intValue(0), floatValue(0.0), boolValue(false) {}

	static RuntimeValue makeVoid() {
		return RuntimeValue();
	}

	static RuntimeValue makeInt(long long value) {
		RuntimeValue runtime;
		runtime.kind = INT;
		runtime.intValue = value;
		runtime.floatValue = static_cast<double>(value);
		return runtime;
	}

	static RuntimeValue makeFloat(double value) {
		RuntimeValue runtime;
		runtime.kind = FLOAT;
		runtime.floatValue = value;
		return runtime;
	}

	static RuntimeValue makeBool(bool value) {
		RuntimeValue runtime;
		runtime.kind = BOOL;
		runtime.boolValue = value;
		return runtime;
	}

	static RuntimeValue makeString(const string& value) {
		RuntimeValue runtime;
		runtime.kind = STRING;
		runtime.stringValue = value;
		return runtime;
	}

	static RuntimeValue makeArray(const vector<RuntimeValue>& values) {
		RuntimeValue runtime;
		runtime.kind = ARRAY;
		runtime.arrayValue = values;
		return runtime;
	}
};

static RuntimeValue runtimeValueFromConst(const ConstValue& value) {
	switch (value.kind) {
		case ConstValue::INT:
			return RuntimeValue::makeInt(value.intValue);
		case ConstValue::FLOAT:
			return RuntimeValue::makeFloat(value.floatValue);
		case ConstValue::BOOL:
			return RuntimeValue::makeBool(value.boolValue);
		case ConstValue::STRING:
			return RuntimeValue::makeString(value.stringValue);
		default:
			return RuntimeValue::makeVoid();
	}
}

static string renderRuntimeValue(const RuntimeValue& value) {
	switch (value.kind) {
		case RuntimeValue::VOID:
			return "void";
		case RuntimeValue::INT:
			return to_string(value.intValue);
		case RuntimeValue::FLOAT:
			return formatDoubleLiteral(value.floatValue);
		case RuntimeValue::BOOL:
			return value.boolValue ? "true" : "false";
		case RuntimeValue::STRING:
			return value.stringValue;
		case RuntimeValue::ARRAY: {
			vector<string> items;
			for (size_t i = 0; i < value.arrayValue.size(); ++i) {
				items.push_back(renderRuntimeValue(value.arrayValue[i]));
			}
			return "[" + joinOperands(items, ", ") + "]";
		}
	}
	return "";
}

static bool runtimeValuesEqual(const RuntimeValue& left, const RuntimeValue& right) {
	if ((left.kind == RuntimeValue::INT || left.kind == RuntimeValue::FLOAT) &&
		(right.kind == RuntimeValue::INT || right.kind == RuntimeValue::FLOAT)) {
		double leftValue = left.kind == RuntimeValue::INT ? static_cast<double>(left.intValue) : left.floatValue;
		double rightValue = right.kind == RuntimeValue::INT ? static_cast<double>(right.intValue) : right.floatValue;
		return fabs(leftValue - rightValue) < 1e-12;
	}
	if (left.kind != right.kind) {
		return false;
	}
	switch (left.kind) {
		case RuntimeValue::VOID:
			return true;
		case RuntimeValue::INT:
			return left.intValue == right.intValue;
		case RuntimeValue::FLOAT:
			return fabs(left.floatValue - right.floatValue) < 1e-12;
		case RuntimeValue::BOOL:
			return left.boolValue == right.boolValue;
		case RuntimeValue::STRING:
			return left.stringValue == right.stringValue;
		case RuntimeValue::ARRAY:
			if (left.arrayValue.size() != right.arrayValue.size()) {
				return false;
			}
			for (size_t i = 0; i < left.arrayValue.size(); ++i) {
				if (!runtimeValuesEqual(left.arrayValue[i], right.arrayValue[i])) {
					return false;
				}
			}
			return true;
	}
	return false;
}

struct IRFrame {
	const IRFunction* function;
	map<string, RuntimeValue> locals;
	size_t pc;

	IRFrame() : function(nullptr), pc(0) {}
};

class IRInterpreter {
public:
	explicit IRInterpreter(const IRProgram& irProgram) : program(irProgram) {
		for (size_t i = 0; i < program.functions.size(); ++i) {
			functions[program.functions[i].name] = &program.functions[i];
			map<string, size_t> labels;
			for (size_t j = 0; j < program.functions[i].instructions.size(); ++j) {
				if (program.functions[i].instructions[j].kind == IRInstruction::LABEL) {
					labels[program.functions[i].instructions[j].label] = j;
				}
			}
			labelTables[program.functions[i].name] = labels;
		}
	}

	RuntimeValue execute() {
		bool hasTopLevel = functions.find("__top_level") != functions.end();
		bool hasMain = functions.find("main") != functions.end();
		RuntimeValue topLevelResult = RuntimeValue::makeVoid();
		if (hasTopLevel) {
			topLevelResult = callFunction("__top_level", vector<RuntimeValue>());
		}
		if (hasMain) {
			return callFunction("main", vector<RuntimeValue>());
		}
		if (hasTopLevel) {
			return topLevelResult;
		}
		throw string("no runnable entry point found");
	}

private:
	const IRProgram& program;
	map<string, const IRFunction*> functions;
	map<string, map<string, size_t> > labelTables;

	const IRFunction& requireFunction(const string& name) const {
		map<string, const IRFunction*>::const_iterator it = functions.find(name);
		if (it == functions.end()) {
			throw string("unknown function: " + name);
		}
		return *it->second;
	}

	const map<string, size_t>& requireLabels(const string& functionName) const {
		map<string, map<string, size_t> >::const_iterator it = labelTables.find(functionName);
		if (it == labelTables.end()) {
			throw string("missing label table for function: " + functionName);
		}
		return it->second;
	}

	RuntimeValue resolveOperand(const string& operand, const IRFrame& frame) const {
		if (operand.empty()) {
			return RuntimeValue::makeVoid();
		}
		ConstValue constant = parseScalarConstant(operand);
		if (constant.known()) {
			return runtimeValueFromConst(constant);
		}
		map<string, RuntimeValue>::const_iterator it = frame.locals.find(operand);
		if (it == frame.locals.end()) {
			throw string("use of uninitialized value: " + operand + " in function " + frame.function->name);
		}
		return it->second;
	}

	RuntimeValue& resolveStoredOperand(const string& operand, IRFrame& frame) const {
		map<string, RuntimeValue>::iterator it = frame.locals.find(operand);
		if (it == frame.locals.end()) {
			throw string("use of uninitialized storage: " + operand + " in function " + frame.function->name);
		}
		return it->second;
	}

	static bool isNumericRuntime(const RuntimeValue& value) {
		return value.kind == RuntimeValue::INT || value.kind == RuntimeValue::FLOAT;
	}

	static double numericAsDouble(const RuntimeValue& value) {
		return value.kind == RuntimeValue::FLOAT ? value.floatValue : static_cast<double>(value.intValue);
	}

	static long long requireIntValue(const RuntimeValue& value, const string& context) {
		if (value.kind != RuntimeValue::INT) {
			throw string(context + " expects int, found " + renderRuntimeValue(value));
		}
		return value.intValue;
	}

	static bool requireBoolValue(const RuntimeValue& value, const string& context) {
		if (value.kind != RuntimeValue::BOOL) {
			throw string(context + " expects bool, found " + renderRuntimeValue(value));
		}
		return value.boolValue;
	}

	static bool runtimeTruthy(const RuntimeValue& value) {
		if (value.kind == RuntimeValue::BOOL) {
			return value.boolValue;
		}
		if (value.kind == RuntimeValue::INT) {
			return value.intValue != 0;
		}
		if (value.kind == RuntimeValue::FLOAT) {
			return fabs(value.floatValue) > 1e-12;
		}
		if (value.kind == RuntimeValue::STRING) {
			return !value.stringValue.empty() && value.stringValue != "0" && value.stringValue != "false";
		}
		if (value.kind == RuntimeValue::ARRAY) {
			return !value.arrayValue.empty();
		}
		return false;
	}

	static RuntimeValue performUnary(const string& op, const RuntimeValue& value) {
		if (op == "+") {
			if (!isNumericRuntime(value)) {
				throw string("unary + expects numeric operand");
			}
			return value;
		}
		if (op == "-") {
			if (value.kind == RuntimeValue::INT) {
				return RuntimeValue::makeInt(-value.intValue);
			}
			if (value.kind == RuntimeValue::FLOAT) {
				return RuntimeValue::makeFloat(-value.floatValue);
			}
			throw string("unary - expects numeric operand");
		}
		if (op == "~") {
			return RuntimeValue::makeInt(~requireIntValue(value, "bitwise not"));
		}
		if (op == "not") {
			return RuntimeValue::makeBool(!requireBoolValue(value, "logical not"));
		}
		throw string("unsupported unary operator: " + op);
	}

	static RuntimeValue performBinary(const string& op, const RuntimeValue& left, const RuntimeValue& right) {
		if (op == "and") {
			return RuntimeValue::makeBool(requireBoolValue(left, "and") && requireBoolValue(right, "and"));
		}
		if (op == "or") {
			return RuntimeValue::makeBool(requireBoolValue(left, "or") || requireBoolValue(right, "or"));
		}
		if (op == "|" || op == "&" || op == "^" || op == "<<" || op == ">>" || op == "%") {
			long long leftValue = requireIntValue(left, op);
			long long rightValue = requireIntValue(right, op);
			if (op == "|") {
				return RuntimeValue::makeInt(leftValue | rightValue);
			}
			if (op == "&") {
				return RuntimeValue::makeInt(leftValue & rightValue);
			}
			if (op == "^") {
				return RuntimeValue::makeInt(leftValue ^ rightValue);
			}
			if (op == "<<") {
				return RuntimeValue::makeInt(leftValue << rightValue);
			}
			if (op == ">>") {
				return RuntimeValue::makeInt(leftValue >> rightValue);
			}
			if (rightValue == 0) {
				throw string("modulo by zero");
			}
			return RuntimeValue::makeInt(leftValue % rightValue);
		}
		if (op == "+" || op == "-" || op == "*" || op == "/") {
			if (!isNumericRuntime(left) || !isNumericRuntime(right)) {
				throw string("arithmetic operator " + op + " expects numeric operands");
			}
			bool useFloat = left.kind == RuntimeValue::FLOAT || right.kind == RuntimeValue::FLOAT;
			double leftFloat = numericAsDouble(left);
			double rightFloat = numericAsDouble(right);
			if (op == "+") {
				return useFloat ? RuntimeValue::makeFloat(leftFloat + rightFloat)
					: RuntimeValue::makeInt(left.intValue + right.intValue);
			}
			if (op == "-") {
				return useFloat ? RuntimeValue::makeFloat(leftFloat - rightFloat)
					: RuntimeValue::makeInt(left.intValue - right.intValue);
			}
			if (op == "*") {
				return useFloat ? RuntimeValue::makeFloat(leftFloat * rightFloat)
					: RuntimeValue::makeInt(left.intValue * right.intValue);
			}
			if (fabs(rightFloat) < 1e-12) {
				throw string("division by zero");
			}
			return useFloat ? RuntimeValue::makeFloat(leftFloat / rightFloat)
				: RuntimeValue::makeInt(left.intValue / right.intValue);
		}
		if (op == "==" || op == "!=") {
			bool equal = runtimeValuesEqual(left, right);
			return RuntimeValue::makeBool(op == "==" ? equal : !equal);
		}
		if (op == "<" || op == "<=" || op == ">" || op == ">=") {
			if (!isNumericRuntime(left) || !isNumericRuntime(right)) {
				throw string("comparison operator " + op + " expects numeric operands");
			}
			double leftValue = numericAsDouble(left);
			double rightValue = numericAsDouble(right);
			if (op == "<") {
				return RuntimeValue::makeBool(leftValue < rightValue);
			}
			if (op == "<=") {
				return RuntimeValue::makeBool(leftValue <= rightValue);
			}
			if (op == ">") {
				return RuntimeValue::makeBool(leftValue > rightValue);
			}
			return RuntimeValue::makeBool(leftValue >= rightValue);
		}
		throw string("unsupported binary operator: " + op);
	}

	static RuntimeValue performBuiltinCall(const string& callee, const vector<RuntimeValue>& args) {
		if (args.size() != 1) {
			throw string("builtin call " + callee + " expects one argument");
		}
		const RuntimeValue& value = args[0];
		if (callee == "int") {
			if (value.kind == RuntimeValue::INT) {
				return value;
			}
			if (value.kind == RuntimeValue::FLOAT) {
				return RuntimeValue::makeInt(static_cast<long long>(value.floatValue));
			}
			if (value.kind == RuntimeValue::BOOL) {
				return RuntimeValue::makeInt(value.boolValue ? 1 : 0);
			}
			if (value.kind == RuntimeValue::STRING) {
				return RuntimeValue::makeInt(atoll(value.stringValue.c_str()));
			}
		} else if (callee == "float") {
			if (value.kind == RuntimeValue::INT) {
				return RuntimeValue::makeFloat(static_cast<double>(value.intValue));
			}
			if (value.kind == RuntimeValue::FLOAT) {
				return value;
			}
			if (value.kind == RuntimeValue::BOOL) {
				return RuntimeValue::makeFloat(value.boolValue ? 1.0 : 0.0);
			}
			if (value.kind == RuntimeValue::STRING) {
				return RuntimeValue::makeFloat(strtod(value.stringValue.c_str(), nullptr));
			}
		} else if (callee == "bool") {
			return RuntimeValue::makeBool(runtimeTruthy(value));
		} else if (callee == "str") {
			if (value.kind == RuntimeValue::STRING) {
				return value;
			}
			return RuntimeValue::makeString(renderRuntimeValue(value));
		}
		throw string("invalid builtin conversion: " + callee);
	}

	static vector<size_t> resolveIndices(const vector<string>& rawIndices, const IRFrame& frame, const IRInterpreter& interpreter) {
		vector<size_t> indices;
		for (size_t i = 0; i < rawIndices.size(); ++i) {
			RuntimeValue indexValue = interpreter.resolveOperand(rawIndices[i], frame);
			long long numeric = requireIntValue(indexValue, "array index");
			if (numeric < 0) {
				throw string("array index cannot be negative");
			}
			indices.push_back(static_cast<size_t>(numeric));
		}
		return indices;
	}

	static const RuntimeValue& resolveIndexedConst(const RuntimeValue& base, const vector<size_t>& indices) {
		const RuntimeValue* current = &base;
		for (size_t i = 0; i < indices.size(); ++i) {
			if (current->kind != RuntimeValue::ARRAY) {
				throw string("indexing non-array value");
			}
			if (indices[i] >= current->arrayValue.size()) {
				throw string("array index out of range");
			}
			current = &current->arrayValue[indices[i]];
		}
		return *current;
	}

	static RuntimeValue& resolveIndexedMutable(RuntimeValue& base, const vector<size_t>& indices) {
		RuntimeValue* current = &base;
		for (size_t i = 0; i < indices.size(); ++i) {
			if (current->kind != RuntimeValue::ARRAY) {
				throw string("indexing non-array value");
			}
			if (indices[i] >= current->arrayValue.size()) {
				throw string("array index out of range");
			}
			current = &current->arrayValue[indices[i]];
		}
		return *current;
	}

	RuntimeValue callFunction(const string& name, const vector<RuntimeValue>& args) const {
		if (isPureBuiltinCall(name)) {
			return performBuiltinCall(name, args);
		}

		const IRFunction& function = requireFunction(name);
		if (args.size() != function.params.size()) {
			throw string("argument count mismatch when calling " + name);
		}

		IRFrame frame;
		frame.function = &function;
		frame.pc = 0;
		for (size_t i = 0; i < function.params.size(); ++i) {
			frame.locals[function.params[i].first] = args[i];
		}

		const map<string, size_t>& labels = requireLabels(function.name);
		while (frame.pc < function.instructions.size()) {
			const IRInstruction& instruction = function.instructions[frame.pc];
			switch (instruction.kind) {
				case IRInstruction::NOP:
				case IRInstruction::LABEL:
					++frame.pc;
					break;
				case IRInstruction::ASSIGN:
					frame.locals[instruction.dest] = resolveOperand(instruction.arg1, frame);
					++frame.pc;
					break;
				case IRInstruction::UNARY:
					frame.locals[instruction.dest] = performUnary(instruction.op, resolveOperand(instruction.arg1, frame));
					++frame.pc;
					break;
				case IRInstruction::BINARY:
					frame.locals[instruction.dest] = performBinary(
						instruction.op,
						resolveOperand(instruction.arg1, frame),
						resolveOperand(instruction.arg2, frame));
					++frame.pc;
					break;
				case IRInstruction::GOTO: {
					map<string, size_t>::const_iterator target = labels.find(instruction.target);
					if (target == labels.end()) {
						throw string("unknown label: " + instruction.target);
					}
					frame.pc = target->second;
					break;
				}
				case IRInstruction::IF_FALSE: {
					RuntimeValue cond = resolveOperand(instruction.arg1, frame);
					if (!requireBoolValue(cond, "if_false")) {
						map<string, size_t>::const_iterator target = labels.find(instruction.target);
						if (target == labels.end()) {
							throw string("unknown label: " + instruction.target);
						}
						frame.pc = target->second;
					} else {
						++frame.pc;
					}
					break;
				}
				case IRInstruction::RETURN:
					return instruction.arg1.empty() ? RuntimeValue::makeVoid() : resolveOperand(instruction.arg1, frame);
				case IRInstruction::CALL: {
					vector<RuntimeValue> callArgs;
					for (size_t i = 0; i < instruction.args.size(); ++i) {
						callArgs.push_back(resolveOperand(instruction.args[i], frame));
					}
					RuntimeValue result = callFunction(instruction.callee, callArgs);
					if (!instruction.dest.empty()) {
						frame.locals[instruction.dest] = result;
					}
					++frame.pc;
					break;
				}
				case IRInstruction::ARRAY_LITERAL: {
					vector<RuntimeValue> elements;
					for (size_t i = 0; i < instruction.args.size(); ++i) {
						elements.push_back(resolveOperand(instruction.args[i], frame));
					}
					frame.locals[instruction.dest] = RuntimeValue::makeArray(elements);
					++frame.pc;
					break;
				}
				case IRInstruction::INDEX_LOAD: {
					const RuntimeValue& base = resolveStoredOperand(instruction.arg1, frame);
					vector<size_t> indices = resolveIndices(instruction.args, frame, *this);
					frame.locals[instruction.dest] = resolveIndexedConst(base, indices);
					++frame.pc;
					break;
				}
				case IRInstruction::INDEX_STORE: {
					RuntimeValue& base = resolveStoredOperand(instruction.dest, frame);
					vector<size_t> indices = resolveIndices(instruction.args, frame, *this);
					resolveIndexedMutable(base, indices) = resolveOperand(instruction.arg1, frame);
					++frame.pc;
					break;
				}
			}
		}

		return RuntimeValue::makeVoid();
	}
};

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

static Json symbolJson(const SymbolRecord& symbol) {
	return Json::object()
		.add("name", Json::str(symbol.name))
		.add("type", Json::str(symbol.type))
		.add("scope", Json::str(symbol.scope))
		.add("line", Json::num(to_string(symbol.line)));
}

static void printInlineJsonObject(const Json& object) {
	cout << "{";
	for (size_t i = 0; i < object.objectItems.size(); ++i) {
		if (i > 0) {
			cout << ", ";
		}
		cout << "\"" << escapeJson(object.objectItems[i].first) << "\": ";
		printJsonValue(object.objectItems[i].second, 0);
	}
	cout << "}";
}

static void printCheckJson(const vector<LexError>& lexErrors,
						   const vector<ParseError>& parseErrors,
						   const vector<SemanticError>& semanticErrors,
						   const vector<SymbolRecord>& symbols) {
	bool hasErrors = !lexErrors.empty() || !parseErrors.empty() || !semanticErrors.empty();
	cout << "{\n";
	cout << "  \"errors\": ";
	vector<Json> errorItems;
	for (size_t i = 0; i < lexErrors.size(); ++i) {
		Json item = Json::object()
			.add("type", Json::str("LEX_ERROR"))
			.add("kind", Json::str(lexErrors[i].type))
			.add("found", Json::str(lexErrors[i].value))
			.add("line", Json::num(to_string(lexErrors[i].line)))
			.add("col", Json::num(to_string(lexErrors[i].col)));
		errorItems.push_back(item);
	}
	for (size_t i = 0; i < parseErrors.size(); ++i) {
		Json item = Json::object()
			.add("type", Json::str("SYNTAX_ERROR"))
			.add("expected", Json::str(parseErrors[i].expected))
			.add("found", Json::str(parseErrors[i].found))
			.add("line", Json::num(to_string(parseErrors[i].line)))
			.add("col", Json::num(to_string(parseErrors[i].col)))
			.add("message", Json::str(parseErrors[i].message));
		errorItems.push_back(item);
	}
	for (size_t i = 0; i < semanticErrors.size(); ++i) {
		errorItems.push_back(semanticErrorJson(semanticErrors[i]));
	}
	if (errorItems.empty()) {
		cout << "[]";
	} else {
		cout << "[\n";
		for (size_t i = 0; i < errorItems.size(); ++i) {
			cout << "    ";
			printInlineJsonObject(errorItems[i]);
			if (i + 1 < errorItems.size()) {
				cout << ",";
			}
			cout << "\n";
		}
		cout << "  ]";
	}
	if (hasErrors) {
		cout << "\n";
		cout << "}\n";
		return;
	}
	cout << ",\n";

	cout << "  \"symbols\": ";
	if (symbols.empty()) {
		cout << "[]\n";
	} else {
		cout << "[\n";
		for (size_t i = 0; i < symbols.size(); ++i) {
			cout << "    ";
			printInlineJsonObject(symbolJson(symbols[i]));
			if (i + 1 < symbols.size()) {
				cout << ",";
			}
			cout << "\n";
		}
		cout << "  ]\n";
	}
	cout << "}\n";
}

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

	if (argc < 3 || argc > 4) {
		cerr << "Usage: ./compiler <tokenize|parse|check|codegen|run> <input_file> [--optimize]\n";
		return 1;
	}

	string mode = argv[1];
	bool validMode = mode == "tokenize" || mode == "parse" || mode == "check" || mode == "codegen" || mode == "run";
	if (!validMode) {
		cerr << "Usage: ./compiler <tokenize|parse|check|codegen|run> <input_file> [--optimize]\n";
		return 1;
	}

	bool optimize = false;
	if (argc == 4) {
		if ((mode != "codegen" && mode != "run") || string(argv[3]) != "--optimize") {
			cerr << "Usage: ./compiler <tokenize|parse|check|codegen|run> <input_file> [--optimize]\n";
			return 1;
		}
		optimize = true;
	}

	ifstream input(argv[2], ios::in | ios::binary);
	if (!input) {
		cerr << "Failed to open input file: " << argv[2] << '\n';
		return 1;
	}

	string source((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
	Lexer lexer(move(source));
	lexer.run();
	if (mode == "tokenize") {
		printJson(lexer.getTokens(), lexer.getErrors());
	} else if (mode == "parse") {
		Parser parser(lexer.getTokens());
		Json ast = parser.parseProgram();
		printParseJson(ast, lexer.getErrors(), parser.getErrors());
	} else {
		Parser parser(lexer.getTokens());
		Json ast = parser.parseProgram();
		if (!lexer.getErrors().empty() || !parser.getErrors().empty()) {
			if (mode == "check") {
				vector<SemanticError> semanticErrors;
				vector<SymbolRecord> symbols;
				printCheckJson(lexer.getErrors(), parser.getErrors(), semanticErrors, symbols);
			} else {
				cerr << "Code generation aborted due to lexical or syntax errors.\n";
				return 1;
			}
		} else {
			SemanticAnalyzer analyzer;
			analyzer.analyze(ast);
			if (mode == "check") {
				printCheckJson(lexer.getErrors(), parser.getErrors(), analyzer.getErrors(), analyzer.getSymbols());
			} else {
				if (!analyzer.getErrors().empty()) {
					cerr << (mode == "run"
						? "Execution aborted due to semantic errors.\n"
						: "Code generation aborted due to semantic errors.\n");
					return 1;
				}
				TacGenerator generator;
				IRProgram program = generator.generate(ast);
				if (optimize) {
					optimizeProgram(program);
				}
				if (mode == "codegen") {
					printTacProgram(program);
				} else {
					try {
						IRInterpreter interpreter(program);
						RuntimeValue result = interpreter.execute();
						cout << renderRuntimeValue(result) << "\n";
					} catch (const string& error) {
						cerr << "Runtime error: " << error << "\n";
						return 1;
					}
				}
			}
		}
	}
	return 0;
}
