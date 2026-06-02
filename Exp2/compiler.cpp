#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>

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

	if (argc != 3 || string(argv[1]) != "tokenize") {
		cerr << "Usage: ./compiler tokenize <input_file>\n";
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
	printJson(lexer.getTokens(), lexer.getErrors());
	return 0;
}