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
