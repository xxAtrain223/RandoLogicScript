#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <map>
#include <utility>
#include <variant>
#include <vector>

namespace rls::ast {

// == Source tracking ==========================================================

/// A position within a source file (1-based line and column).
struct Position {
	uint32_t line = 0;
	uint32_t column = 0;
};

/// A half-open range in a source file.
struct SourceRange {
	Position start;
	Position end;
};

/// A 1-based UTF-16 position for external editor consumers.
struct Utf16Position {
	uint32_t line = 0;
	uint32_t column = 0;
};

/// Immutable, validated UTF-8 source text.
///
/// Source bytes are preserved exactly, including CRLF. `Position` line and
/// column values are 1-based; columns count UTF-8 bytes. Invalid UTF-8 is
/// rejected by factories and edits.
class SourceText {
public:
	SourceText() : lineStarts_{0} {}

	static std::optional<SourceText> FromUtf8(std::string content) {
		if (!isValidUtf8(content)) return std::nullopt;
		return SourceText(std::move(content));
	}

	const std::string& content() const { return content_; }
	const std::vector<size_t>& lineStarts() const { return lineStarts_; }

	std::optional<size_t> byteOffsetFromUtf8Position(Position position) const {
		if (position.line == 0 || position.column == 0 || position.line > lineStarts_.size()) {
			return std::nullopt;
		}

		const size_t lineStart = lineStarts_[position.line - 1];
		const size_t offset = lineStart + position.column - 1;
		const size_t lineLimit = position.line < lineStarts_.size()
			? lineStarts_[position.line]
			: content_.size();
		if (offset > lineLimit || (position.line < lineStarts_.size() && offset == lineLimit)) {
			return std::nullopt;
		}
		return offset;
	}

	std::optional<Position> utf8PositionAtByteOffset(size_t offset) const {
		if (offset > content_.size()) return std::nullopt;

		size_t lineIndex = 0;
		while (lineIndex + 1 < lineStarts_.size() && lineStarts_[lineIndex + 1] <= offset) {
			++lineIndex;
		}
		return Position{
			static_cast<uint32_t>(lineIndex + 1),
			static_cast<uint32_t>(offset - lineStarts_[lineIndex] + 1),
		};
	}

	std::optional<Utf16Position> utf16PositionAtByteOffset(size_t offset) const {
		const auto utf8Position = utf8PositionAtByteOffset(offset);
		if (!utf8Position) return std::nullopt;

		const size_t lineStart = lineStarts_[utf8Position->line - 1];
		size_t cursor = lineStart;
		uint32_t utf16Column = 1;
		while (cursor < offset) {
			const size_t width = utf8CodePointWidth(static_cast<unsigned char>(content_[cursor]));
			if (cursor + width > offset) return std::nullopt;
			utf16Column += width == 4 ? 2 : 1;
			cursor += width;
		}
		return Utf16Position{utf8Position->line, utf16Column};
	}

	std::optional<size_t> byteOffsetFromUtf16Position(Utf16Position position) const {
		if (position.line == 0 || position.column == 0 || position.line > lineStarts_.size()) {
			return std::nullopt;
		}

		const size_t lineStart = lineStarts_[position.line - 1];
		const size_t lineLimit = position.line < lineStarts_.size()
			? lineStarts_[position.line]
			: content_.size();
		size_t cursor = lineStart;
		uint32_t utf16Column = 1;
		while (cursor < lineLimit && utf16Column < position.column) {
			const size_t width = utf8CodePointWidth(static_cast<unsigned char>(content_[cursor]));
			const uint32_t units = width == 4 ? 2 : 1;
			if (utf16Column + units > position.column || cursor + width > lineLimit) {
				return std::nullopt;
			}
			utf16Column += units;
			cursor += width;
		}
		return utf16Column == position.column ? std::optional<size_t>(cursor) : std::nullopt;
	}

	std::optional<SourceText> replaceAll(std::string replacement) const {
		return FromUtf8(std::move(replacement));
	}

	std::optional<SourceText> replace(SourceRange range, std::string replacement) const {
		const auto start = byteOffsetFromUtf8Position(range.start);
		const auto end = byteOffsetFromUtf8Position(range.end);
		if (!start || !end || *start > *end || !isValidUtf8(replacement)) return std::nullopt;

		std::string updated;
		updated.reserve(*start + replacement.size() + content_.size() - *end);
		updated.append(content_, 0, *start);
		updated += replacement;
		updated.append(content_, *end, std::string::npos);
		return FromUtf8(std::move(updated));
	}

	std::optional<SourceRange> incompleteTokenRangeAt(Position position) const {
		const auto offset = byteOffsetFromUtf8Position(position);
		if (!offset) return std::nullopt;

		size_t start = *offset;
		while (start > 0 && isTokenByte(static_cast<unsigned char>(content_[start - 1]))) --start;
		size_t end = *offset;
		while (end < content_.size() && isTokenByte(static_cast<unsigned char>(content_[end]))) ++end;
		if (start == end) return std::nullopt;

		const auto startPosition = utf8PositionAtByteOffset(start);
		const auto endPosition = utf8PositionAtByteOffset(end);
		return SourceRange{*startPosition, *endPosition};
	}

private:
	std::string content_;
	std::vector<size_t> lineStarts_;

	explicit SourceText(std::string content) : content_(std::move(content)), lineStarts_{0} {
		for (size_t offset = 0; offset < content_.size(); ++offset) {
			if (content_[offset] == '\n') lineStarts_.push_back(offset + 1);
		}
	}

	static bool isTokenByte(unsigned char byte) {
		return byte == '_' || (byte >= '0' && byte <= '9') ||
			(byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z');
	}

	static size_t utf8CodePointWidth(unsigned char leadByte) {
		if (leadByte < 0x80) return 1;
		if (leadByte < 0xE0) return 2;
		if (leadByte < 0xF0) return 3;
		return 4;
	}

	static bool isValidUtf8(std::string_view text) {
		for (size_t offset = 0; offset < text.size();) {
			const unsigned char leadByte = static_cast<unsigned char>(text[offset]);
			if (leadByte < 0x80) {
				++offset;
				continue;
			}

			const size_t width = utf8CodePointWidth(leadByte);
			if ((leadByte < 0xC2) || (leadByte > 0xF4) || offset + width > text.size()) {
				return false;
			}
			for (size_t index = 1; index < width; ++index) {
				if ((static_cast<unsigned char>(text[offset + index]) & 0xC0) != 0x80) return false;
			}
			const unsigned char secondByte = static_cast<unsigned char>(text[offset + 1]);
			if ((leadByte == 0xE0 && secondByte < 0xA0) ||
				(leadByte == 0xED && secondByte >= 0xA0) ||
				(leadByte == 0xF0 && secondByte < 0x90) ||
				(leadByte == 0xF4 && secondByte > 0x8F)) {
				return false;
			}
			offset += width;
		}
		return true;
	}
};

/// A span of source text: the file it came from plus start/end positions.
struct Span {
	std::string file;
	Position start;
	Position end;
};

// == Forward declarations =====================================================

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

// == Enumerations =============================================================

enum class UnaryOp { Not };

enum class BinaryOp {
	// Logical
	And, Or,
	// Comparison
	Eq, NotEq, Lt, LtEq, Gt, GtEq,
	// Arithmetic
	Add, Sub, Mul, Div,
};

enum class SectionKind { Events, Locations, Exits };

enum class IdentifierKind {
	Unresolved,
	Parameter,
	EnumValue,
	DeclaredValue,
	FunctionRef,
};

// == Name-like syntax nodes ==================================================

/// A syntactic name token used for declarations, labels, and symbolic refs.
struct Name {
	std::string text;
	Span span;

	Name() = default;
	explicit Name(std::string text, Span span = {})
		: text(std::move(text)), span(std::move(span)) {}

	std::string_view view() const { return text; }
};

inline bool operator==(const Name& lhs, std::string_view rhs) {
	return lhs.text == rhs;
}

inline bool operator==(std::string_view lhs, const Name& rhs) {
	return lhs == rhs.text;
}

inline std::ostream& operator<<(std::ostream& out, const Name& name) {
	return out << name.text;
}

/// A type annotation name such as `Bool`, `Distance`, or `Item`.
struct TypeRef {
	Name name;

	TypeRef() = default;
	explicit TypeRef(Name name) : name(std::move(name)) {}
};

inline bool operator==(const TypeRef& lhs, std::string_view rhs) {
	return lhs.name.text == rhs;
}

inline bool operator==(std::string_view lhs, const TypeRef& rhs) {
	return lhs == rhs.name.text;
}

inline std::ostream& operator<<(std::ostream& out, const TypeRef& typeRef) {
	return out << typeRef.name.text;
}

// == Expression leaf nodes ====================================================

/// Boolean literal: `true`, `false`, `always`, `never`.
struct BoolLiteral {
	bool value;
};

/// Integer literal: `0`, `1`, `48`, etc.
struct IntLiteral {
	int value;
};

/// String literal: `"text"`.
struct StringLiteral {
	std::string value;
};

/// Named identifier: enum values (`RG_HOOKSHOT`), parameters (`distance`), etc.
struct Identifier {
	Name name;
	IdentifierKind kind = IdentifierKind::Unresolved;

	Identifier() = default;
	explicit Identifier(Name name, IdentifierKind kind = IdentifierKind::Unresolved)
		: name(std::move(name)), kind(kind) {}
};

// == Expression compound nodes ================================================

/// Unary expression: `not <operand>`.
struct UnaryExpr {
	UnaryOp op;
	ExprPtr operand;

	UnaryExpr(UnaryOp op, ExprPtr operand)
		: op(op), operand(std::move(operand)) {}
};

/// Binary expression: `<left> <op> <right>`.
struct BinaryExpr {
	BinaryOp op;
	ExprPtr left;
	ExprPtr right;

	BinaryExpr(BinaryOp op, ExprPtr left, ExprPtr right)
		: op(op), left(std::move(left)), right(std::move(right)) {}
};

/// Ternary expression: `<condition> ? <thenBranch> : <elseBranch>`.
struct TernaryExpr {
	ExprPtr condition;
	ExprPtr thenBranch;
	ExprPtr elseBranch;

	TernaryExpr(ExprPtr condition, ExprPtr thenBranch, ExprPtr elseBranch)
		: condition(std::move(condition)),
		  thenBranch(std::move(thenBranch)),
		  elseBranch(std::move(elseBranch)) {}
};

/// A function call argument, either positional or named (`param: value`).
struct Arg {
	std::optional<Name> name;
	ExprPtr value;

	Arg(std::optional<Name> name, ExprPtr value)
		: name(std::move(name)), value(std::move(value)) {}
};

/// Function call: `name(arg1, arg2, param: arg3)`.
struct CallExpr {
	Name callee;
	std::vector<Arg> args;

	CallExpr(Name callee, std::vector<Arg> args)
		: callee(std::move(callee)), args(std::move(args)) {}
};

/// Invoke a callable expression result: `<callee>()`.
struct InvokeExpr {
	ExprPtr callee;

	InvokeExpr(ExprPtr callee)
		: callee(std::move(callee)) {}
};

/// The `here` keyword: resolves to the current region's name during sema.
/// Only valid inside region data and entry conditions.
struct HereRef {
	Name resolvedRegion; ///< Filled in by sema; empty until resolved.
};

/// Member access: `EnumName.ValueName` — dotted enum value disambiguation.
/// `object` is the enum type name; `member` is the value name.
struct MemberExpr {
	Name object;
	Name member;

	MemberExpr(Name object, Name member)
		: object(std::move(object)), member(std::move(member)) {}
};

/// One arm of a `match` expression.
struct MatchArm {
	std::vector<ExprPtr> patterns;      // one or more match expressions
	bool isDefault;                     // `_` catch-all arm
	ExprPtr body;
	bool fallthrough;                   // trailing `or` for OR-accumulation

	MatchArm(std::vector<ExprPtr> patterns, bool isDefault, ExprPtr body, bool fallthrough)
		: patterns(std::move(patterns)),
		  isDefault(isDefault),
		  body(std::move(body)),
		  fallthrough(fallthrough) {}
};

/// Match expression: `match <discriminant> { <arms> }`.
struct MatchExpr {
	ExprPtr discriminant;
	std::vector<MatchArm> arms;

	MatchExpr(ExprPtr discriminant, std::vector<MatchArm> arms)
		: discriminant(std::move(discriminant)), arms(std::move(arms)) {}
};

/// List literal: `[first, second, ...]`.
struct ListExpr {
	std::vector<ExprPtr> elements;

	explicit ListExpr(std::vector<ExprPtr> elements)
		: elements(std::move(elements)) {}
};

// == Expr wrapper =============================================================

/// The central expression node. Wraps a variant of all expression types plus
/// a source location for error reporting.
struct Expr {
	using Variant = std::variant<
		BoolLiteral,
		IntLiteral,
		StringLiteral,
		Identifier,
		MemberExpr,
		UnaryExpr,
		BinaryExpr,
		TernaryExpr,
		CallExpr,
		InvokeExpr,
		HereRef,
		MatchExpr,
		ListExpr
	>;

	Variant node;
	Span span;

	Expr(Variant node, Span span = {})
		: node(std::move(node)), span(span) {}
};

/// Helper to construct an ExprPtr from any expression node type.
template <typename T>
ExprPtr makeExpr(T&& node, Span span = {}) {
	return std::make_unique<Expr>(std::forward<T>(node), span);
}

// == Declaration support types ================================================

/// A parameter in a `define` or enemy field: `name`, optional `: type`,
/// optional `= default`.
struct Param {
	Name name;
	std::optional<TypeRef> type;
	ExprPtr defaultValue; // nullptr if no default
	Span span;

	Param(Name name, std::optional<TypeRef> type, ExprPtr defaultValue, Span span = {})
		: name(std::move(name)),
		  type(std::move(type)),
		  defaultValue(std::move(defaultValue)),
		  span(std::move(span)) {}
};

/// A single entry in a region section: `NAME: condition`.
struct Entry {
	Name name;
	ExprPtr condition;
	Span span;

	Entry(Name name, ExprPtr condition, Span span = {})
		: name(std::move(name)),
		  condition(std::move(condition)),
		  span(span) {}
};

/// A region section: `events { ... }`, `locations { ... }`, or `exits { ... }`.
struct Section {
	SectionKind kind;
	std::vector<Entry> entries;
	Span span;

	Section(SectionKind kind, std::vector<Entry> entries, Span span = {})
		: kind(kind), entries(std::move(entries)), span(std::move(span)) {}
};

/// One arbitrary data entry in a region body: `key: value`.
struct RegionDataEntry {
	Name key;
	ExprPtr value;
	Span span;

	RegionDataEntry(Name key, ExprPtr value, Span span = {})
		: key(std::move(key)), value(std::move(value)), span(std::move(span)) {}
};

/// Region body: arbitrary data and sections shared by `region` declarations.
struct RegionBody {
	std::vector<RegionDataEntry> data;
	std::vector<Section> sections;

	RegionBody(
		std::vector<RegionDataEntry> data,
		std::vector<Section> sections)
		: data(std::move(data)),
		  sections(std::move(sections)) {}

	const RegionDataEntry* findData(std::string_view key) const {
		for (const auto& entry : data) {
			if (entry.key == key) {
				return &entry;
			}
		}
		return nullptr;
	}
};

// == Top-level declarations ===================================================

/// `region KEY { <data and sections> }`
struct RegionDecl {
	Name key;
	RegionBody body;
	Span span;

	RegionDecl(Name key, RegionBody body, Span span = {})
		: key(std::move(key)),
		  body(std::move(body)),
		  span(span) {}
};

/// `extend region RR_NAME { ... }`
/// Extensions add sections and cannot add or replace base-region data.
struct ExtendRegionDecl {
	Name name;
	std::vector<Section> sections;
	Span span;

	ExtendRegionDecl(Name name, std::vector<Section> sections,
	                 Span span = {})
		: name(std::move(name)),
		  sections(std::move(sections)),
		  span(span) {}
};

/// `define name(params): body`
struct DefineDecl {
	Name name;
	std::vector<Param> params;
	ExprPtr body;
	Span span;

	DefineDecl(
		Name name,
		std::vector<Param> params,
		ExprPtr body,
		Span span = {})
		: name(std::move(name)),
		  params(std::move(params)),
		  body(std::move(body)),
		  span(span) {}
};

/// `extern define name(params)`
struct ExternDefineDecl {
	Name name;
	std::vector<Param> params;
	std::optional<TypeRef> returnType;
	Span span;

	ExternDefineDecl(
		Name name,
		std::vector<Param> params,
		Span span = {})
		: name(std::move(name)),
		  params(std::move(params)),
		  returnType(std::nullopt),
		  span(span) {}

	ExternDefineDecl(
		Name name,
		std::vector<Param> params,
		std::optional<TypeRef> returnType,
		Span span = {})
		: name(std::move(name)),
		  params(std::move(params)),
		  returnType(std::move(returnType)),
		  span(span) {}
};

/// One explicit enum member: `NAME` or `NAME = 3`.
struct EnumMemberDecl {
	Name name;
	std::optional<int> explicitValue;
	Span span;

	EnumMemberDecl(Name name, std::optional<int> explicitValue = std::nullopt,
	               Span span = {})
		: name(std::move(name)),
		  explicitValue(explicitValue),
		  span(std::move(span)) {}
};

/// One glob pattern entry for extern enums, e.g. `RG_*`.
struct EnumPatternDecl {
	std::string pattern;
	Span span;

	EnumPatternDecl(std::string pattern, Span span = {})
		: pattern(std::move(pattern)), span(std::move(span)) {}
};

using ExternEnumEntryDecl = std::variant<EnumMemberDecl, EnumPatternDecl>;

/// `enum Name { A, B = 2 }`
struct EnumDecl {
	Name name;
	std::vector<EnumMemberDecl> members;
	Span span;

	EnumDecl(Name name, std::vector<EnumMemberDecl> members, Span span = {})
		: name(std::move(name)), members(std::move(members)), span(std::move(span)) {}
};

/// `extern enum Name { A, RG_* }`
struct ExternEnumDecl {
	Name name;
	std::vector<ExternEnumEntryDecl> entries;
	Span span;

	ExternEnumDecl(Name name, std::vector<ExternEnumEntryDecl> entries,
	               Span span = {})
		: name(std::move(name)), entries(std::move(entries)), span(std::move(span)) {}
};

/// A top-level declaration: region, extend region, define, extern define,
/// enum, or extern enum.
using Decl = std::variant<
	RegionDecl,
	ExtendRegionDecl,
	DefineDecl,
	ExternDefineDecl,
	EnumDecl,
	ExternEnumDecl
>;

// == Diagnostics ==============================================================

enum class DiagnosticLevel { Error, Warning, Info };

struct DiagnosticActionData {
	uint32_t version = 1;
	std::string actionKind;
	std::vector<std::string> arguments;
};

inline std::string levelToString(rls::ast::DiagnosticLevel level) {
	switch (level) {
	case rls::ast::DiagnosticLevel::Error:   return "error";
	case rls::ast::DiagnosticLevel::Warning: return "warning";
	case rls::ast::DiagnosticLevel::Info:    return "info";
	}
	return "unknown";
}

/// A diagnostic message produced during parsing or semantic analysis.
struct Diagnostic {
	std::string code;
	Span span; // location of the offending construct
	DiagnosticLevel level = DiagnosticLevel::Error;
	std::string message;
	std::optional<DiagnosticActionData> data;

	Diagnostic() = default;

	Diagnostic(std::string code, Span span, DiagnosticLevel level, std::string message,
	           std::optional<DiagnosticActionData> data = std::nullopt)
		: code(std::move(code)),
		  span(std::move(span)),
		  level(level),
		  message(std::move(message)),
		  data(std::move(data)) {}
};

// == File =====================================================================

/// Root AST node representing an entire `.rls` file.
struct File {
	std::string path;  // owned canonical path of the source file
	std::vector<Decl> declarations;
	std::vector<Diagnostic> diagnostics;
};

// == Project ==================================================================

enum class Type {
    Bool,       // true, false, always, never, and/or/not, conditions
    Int,        // integer literals, arithmetic results, hearts(), keys(), etc.
	String,     // string literals
	List,       // list literals
	// TODO: Implement parameterized callable syntax (e.g., (Item) -> Bool).
	Callable,   // generic callable value
	Condition,  // callable with signature () -> Bool
	Enum,       // user-defined or host-defined enum value, identified by Project metadata
	Region,     // declared region value
	Event,      // declared event entry value
	Location,   // declared location entry value
    Void,       // statements / declarations with no value
    Error,      // poison type — inference failed, suppress cascading errors
};

enum class EnumKind {
	Normal,
	Extern,
};

struct EnumMemberInfo {
	Name name;
	std::optional<int> value;
	Span span;
};

struct EnumPatternInfo {
	std::string pattern;
	Span span;
};

using EnumEntryInfo = std::variant<EnumMemberInfo, EnumPatternInfo>;

inline bool isEnumMemberEntry(const EnumEntryInfo& entry) {
	return std::holds_alternative<EnumMemberInfo>(entry);
}

inline bool isEnumPatternEntry(const EnumEntryInfo& entry) {
	return std::holds_alternative<EnumPatternInfo>(entry);
}

struct EnumInfo {
	Name name;
	EnumKind kind = EnumKind::Normal;
	Type underlyingType = Type::Int;
	std::vector<EnumEntryInfo> entries;
	Span span;

	EnumInfo() = default;

	EnumInfo(Name name, EnumKind kind, Type underlyingType,
	         std::vector<EnumEntryInfo> entries, Span span = {})
		: name(std::move(name)),
		  kind(kind),
		  underlyingType(underlyingType),
		  entries(std::move(entries)),
		  span(std::move(span)) {}
};

/// Aggregated AST for all `.rls` files in a project.
/// All top-level declarations are globally visible (no import mechanism).
struct Project {
	std::vector<File> files;

	std::map<std::string, const RegionDecl*> RegionDecls;
	std::map<std::string, std::vector<const ExtendRegionDecl*>> ExtendRegionDecls;
	std::map<std::string, std::vector<const Entry*>> EventDecls;
	std::map<std::string, std::vector<const Entry*>> LocationDecls;
	std::map<std::string, const DefineDecl*> DefineDecls;
	std::map<std::string, const ExternDefineDecl*> ExternDefineDecls;
	std::map<std::string, EnumInfo> EnumInfos;

	template <typename T>
	void setType(const T* node, Type type) {
		TypeTable[node] = type;
	}

	template <typename T>
	std::optional<Type> getType(const T* node) const {
		auto it = TypeTable.find(node);
		return it != TypeTable.end() ? std::optional(it->second) : std::nullopt;
	}

	template <typename T>
	void setEnumType(const T* node, std::string enumName) {
		EnumTypeTable[node] = std::move(enumName);
	}

	template <typename T>
	std::optional<std::string_view> getEnumType(const T* node) const {
		auto it = EnumTypeTable.find(node);
		if (it == EnumTypeTable.end()) {
			return std::nullopt;
		}
		return it->second;
	}

	void registerEnum(EnumInfo info) {
		EnumInfos[info.name.text] = std::move(info);
	}

	const EnumInfo* getEnumInfo(std::string_view enumName) const {
		auto it = EnumInfos.find(std::string(enumName));
		return it != EnumInfos.end() ? &it->second : nullptr;
	}

	void setResolvedCallArgs(const CallExpr* node, std::vector<const Expr*> args) {
		ResolvedCallArgs[node] = std::move(args);
	}

	const std::vector<const Expr*>* getResolvedCallArgs(const CallExpr* node) const {
		auto it = ResolvedCallArgs.find(node);
		return it != ResolvedCallArgs.end() ? &it->second : nullptr;
	}

private:
	std::unordered_map<const void*, Type> TypeTable;
	std::unordered_map<const void*, std::string> EnumTypeTable;
	std::unordered_map<const CallExpr*, std::vector<const Expr*>> ResolvedCallArgs;
};

} // namespace rls::ast
