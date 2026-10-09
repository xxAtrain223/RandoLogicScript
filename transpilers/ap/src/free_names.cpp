#include "ap_transpiler.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace rls::transpilers::ap {

namespace {

enum class TokenKind { Identifier, Number, String, Punct };

struct Token {
	TokenKind kind;
	std::string_view text;

	bool isPunct(std::string_view p) const { return kind == TokenKind::Punct && text == p; }
	bool isIdentifier(std::string_view word) const { return kind == TokenKind::Identifier && text == word; }
};

bool isIdentifierStart(char c) {
	return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isIdentifierChar(char c) {
	return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Splits Python source into the handful of token kinds the scan needs. Comments and whitespace are
// dropped, and a string literal is a single token, so identifiers inside one are never seen.
std::vector<Token> tokenize(std::string_view src) {
	std::vector<Token> tokens;
	size_t i = 0;
	while (i < src.size()) {
		const char c = src[i];
		if (std::isspace(static_cast<unsigned char>(c)) != 0) {
			++i;
			continue;
		}
		if (c == '#') {
			while (i < src.size() && src[i] != '\n') {
				++i;
			}
			continue;
		}
		const size_t start = i;
		if (c == '"' || c == '\'') {
			++i;
			while (i < src.size() && src[i] != c) {
				i += (src[i] == '\\') ? 2 : 1;
			}
			i = std::min(i + 1, src.size());
			tokens.push_back({TokenKind::String, src.substr(start, i - start)});
		} else if (isIdentifierStart(c)) {
			while (i < src.size() && isIdentifierChar(src[i])) {
				++i;
			}
			tokens.push_back({TokenKind::Identifier, src.substr(start, i - start)});
		} else if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
			while (i < src.size() && (isIdentifierChar(src[i]) || src[i] == '.')) {
				++i;
			}
			tokens.push_back({TokenKind::Number, src.substr(start, i - start)});
		} else {
			// `=` has to be told apart from `==` (keyword argument vs comparison), so the
			// two-character operators are kept whole.
			static constexpr std::string_view twoChar[] = {"==", "!=", "<=", ">=", "->"};
			i += 1;
			if (i < src.size()) {
				const std::string_view pair = src.substr(start, 2);
				if (std::find(std::begin(twoChar), std::end(twoChar), pair) != std::end(twoChar)) {
					i += 1;
				}
			}
			tokens.push_back({TokenKind::Punct, src.substr(start, i - start)});
		}
	}
	return tokens;
}

bool isOpening(const Token& t) {
	return t.isPunct("(") || t.isPunct("[") || t.isPunct("{");
}

bool isClosing(const Token& t) {
	return t.isPunct(")") || t.isPunct("]") || t.isPunct("}");
}

// Python words that are never importable names.
bool isKeywordOrBuiltin(std::string_view word) {
	static constexpr std::string_view words[] = {
		"and", "or", "not", "if", "else", "elif", "lambda", "def", "return", "in", "is", "for", "while",
		"from", "import", "as", "class", "pass", "yield", "await", "async", "with", "try", "except",
		"finally", "raise", "assert", "del", "global", "nonlocal", "break", "continue",
		"True", "False", "None",
		"bool", "int", "str", "float", "tuple", "list", "dict", "set", "frozenset", "len", "min", "max",
		"any", "all", "range", "isinstance", "getattr",
	};
	return std::find(std::begin(words), std::end(words), word) != std::end(words);
}

// `def name(a, b: T = d) -> R:` binds `name`, `a` and `b`; the annotations and the default are
// ordinary references. `i` is the index of `def`.
void bindDefinition(const std::vector<Token>& tokens, size_t i, std::set<std::string>& bound) {
	if (i + 1 >= tokens.size() || tokens[i + 1].kind != TokenKind::Identifier) {
		return;
	}
	bound.insert(std::string(tokens[i + 1].text));
	size_t j = i + 2;
	if (j >= tokens.size() || !tokens[j].isPunct("(")) {
		return;
	}
	int depth = 0;
	bool expectParam = false;
	for (; j < tokens.size(); ++j) {
		const Token& t = tokens[j];
		if (isOpening(t)) {
			++depth;
			expectParam = (depth == 1);
		} else if (isClosing(t)) {
			if (--depth == 0) {
				return;
			}
		} else if (t.isPunct(",") && depth == 1) {
			expectParam = true;
		} else if (t.kind == TokenKind::Identifier && depth == 1 && expectParam) {
			bound.insert(std::string(t.text));
			expectParam = false;
		}
	}
}

// `lambda a, b=b: ...` binds `a` and `b`. `i` is the index of `lambda`.
void bindLambda(const std::vector<Token>& tokens, size_t i, std::set<std::string>& bound) {
	int depth = 0;
	bool expectParam = true;
	for (size_t j = i + 1; j < tokens.size(); ++j) {
		const Token& t = tokens[j];
		if (t.isPunct(":") && depth == 0) {
			return;
		}
		if (isOpening(t)) {
			++depth;
		} else if (isClosing(t)) {
			--depth;
		} else if (t.isPunct(",") && depth == 0) {
			expectParam = true;
		} else if (t.kind == TokenKind::Identifier && depth == 0 && expectParam) {
			bound.insert(std::string(t.text));
			expectParam = false;
		}
	}
}

} // namespace

std::set<std::string> FreePythonNames(std::string_view source, const std::set<std::string>& alsoBound) {
	const std::vector<Token> tokens = tokenize(source);

	std::set<std::string> bound = alsoBound;
	for (size_t i = 0; i < tokens.size(); ++i) {
		if (tokens[i].isIdentifier("def")) {
			bindDefinition(tokens, i, bound);
		} else if (tokens[i].isIdentifier("lambda")) {
			bindLambda(tokens, i, bound);
		}
	}

	std::set<std::string> free;
	for (size_t i = 0; i < tokens.size(); ++i) {
		const Token& t = tokens[i];
		if (t.kind != TokenKind::Identifier || isKeywordOrBuiltin(t.text)) {
			continue;
		}
		const std::string name(t.text);
		if (bound.count(name) != 0) {
			continue;
		}
		const Token* prev = i > 0 ? &tokens[i - 1] : nullptr;
		const Token* next = i + 1 < tokens.size() ? &tokens[i + 1] : nullptr;
		// `x.name` is an attribute, not a name in scope.
		if (prev != nullptr && prev->isPunct(".")) {
			continue;
		}
		// `f(name=value)` is a keyword argument.
		if (next != nullptr && next->isPunct("=") && prev != nullptr && (prev->isPunct("(") || prev->isPunct(","))) {
			continue;
		}
		free.insert(name);
	}
	return free;
}

} // namespace rls::transpilers::ap
