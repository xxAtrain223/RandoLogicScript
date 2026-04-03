#include "builder.h"

#include "grammar.h"

#include <charconv>
#include <stdexcept>
#include <string>

namespace rls::parser {

namespace {

using Node = tao::pegtl::parse_tree::node;

// =============================================================================
// Span helper
// =============================================================================

ast::Span makeSpan(const Node& n) {
	ast::Span span;
	span.file = std::string(n.source);
	span.start = {
		static_cast<uint32_t>(n.m_begin.line),
		static_cast<uint32_t>(n.m_begin.column)
	};
	if (n.has_content()) {
		span.end = {
			static_cast<uint32_t>(n.m_end.line),
			static_cast<uint32_t>(n.m_end.column)
		};
	}
	return span;
}

// =============================================================================
// Forward declarations
// =============================================================================

ast::ExprPtr buildExpr(const Node& n);

// =============================================================================
// Operator mapping
// =============================================================================

ast::BinaryOp mapCompOp(std::string_view s) {
	if (s == "==") return ast::BinaryOp::Eq;
	if (s == "!=") return ast::BinaryOp::NotEq;
	if (s == ">=") return ast::BinaryOp::GtEq;
	if (s == "<=") return ast::BinaryOp::LtEq;
	if (s == ">")  return ast::BinaryOp::Gt;
	if (s == "<")  return ast::BinaryOp::Lt;
	// "is not" (may contain whitespace / comments between the two words)
	if (s.find("not") != std::string_view::npos) return ast::BinaryOp::NotEq;
	// bare "is"
	return ast::BinaryOp::Eq;
}

ast::BinaryOp mapAddSubOp(std::string_view s) {
	if (s == "+") return ast::BinaryOp::Add;
	return ast::BinaryOp::Sub;
}

ast::BinaryOp mapMulDivOp(std::string_view s) {
	if (s == "*") return ast::BinaryOp::Mul;
	return ast::BinaryOp::Div;
}

// =============================================================================
// Expression builders
// =============================================================================

/// Left-fold binary expression whose children alternate:
///   [operand, op_token, operand, op_token, operand, ...]
template <typename OpMapper>
ast::ExprPtr buildBinaryChain(const Node& n, OpMapper mapOp) {
	auto result = buildExpr(*n.children[0]);
	for (size_t i = 1; i + 1 < n.children.size(); i += 2) {
		auto op = mapOp(n.children[i]->string_view());
		auto right = buildExpr(*n.children[i + 1]);
		result = ast::makeExpr(ast::BinaryExpr(
			op, std::move(result), std::move(right)));
	}
	return result;
}

/// Left-fold for and/or chains whose children are just operands
/// (no explicit operator token nodes).
ast::ExprPtr buildLogicalChain(const Node& n, ast::BinaryOp op) {
	auto result = buildExpr(*n.children[0]);
	for (size_t i = 1; i < n.children.size(); ++i) {
		auto right = buildExpr(*n.children[i]);
		result = ast::makeExpr(ast::BinaryExpr(
			op, std::move(result), std::move(right)));
	}
	return result;
}

/// Main expression dispatcher — pattern-matches on the CST node type.
ast::ExprPtr buildExpr(const Node& n) {

	// -- Leaf nodes -----------------------------------------------------------

	if (n.is_type<grammar::atom_keyword>()) {
		auto s = n.string_view();
		if (s == "true" || s == "always")
			return ast::makeExpr(ast::BoolLiteral{true}, makeSpan(n));
		if (s == "false" || s == "never")
			return ast::makeExpr(ast::BoolLiteral{false}, makeSpan(n));
		if (s == "is_child")
			return ast::makeExpr(ast::KeywordExpr{ast::Keyword::IsChild}, makeSpan(n));
		if (s == "is_adult")
			return ast::makeExpr(ast::KeywordExpr{ast::Keyword::IsAdult}, makeSpan(n));
		if (s == "at_day")
			return ast::makeExpr(ast::KeywordExpr{ast::Keyword::AtDay}, makeSpan(n));
		if (s == "at_night")
			return ast::makeExpr(ast::KeywordExpr{ast::Keyword::AtNight}, makeSpan(n));
		if (s == "is_vanilla")
			return ast::makeExpr(ast::KeywordExpr{ast::Keyword::IsVanilla}, makeSpan(n));
		if (s == "is_mq")
			return ast::makeExpr(ast::KeywordExpr{ast::Keyword::IsMq}, makeSpan(n));
		throw std::runtime_error("Unknown atom keyword: " + std::string(s));
	}

	if (n.is_type<grammar::ident>()) {
		return ast::makeExpr(
			ast::Identifier{std::string(n.string_view())}, makeSpan(n));
	}

	if (n.is_type<grammar::integer>()) {
		int value = 0;
		auto text = n.string_view();
		auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (ec != std::errc()) {
			throw std::runtime_error("Invalid integer literal: " + std::string(text));
		}
		return ast::makeExpr(ast::IntLiteral{value}, makeSpan(n));
	}

	// -- Unary ----------------------------------------------------------------

	if (n.is_type<grammar::unary>()) {
		// children: [kw_not, operand]
		return ast::makeExpr(
			ast::UnaryExpr(ast::UnaryOp::Not, buildExpr(*n.children[1])),
			makeSpan(n));
	}

	// -- Binary chains with explicit operator tokens --------------------------

	if (n.is_type<grammar::mul_div>()) {
		return buildBinaryChain(n, mapMulDivOp);
	}

	if (n.is_type<grammar::add_sub>()) {
		return buildBinaryChain(n, mapAddSubOp);
	}

	// -- Comparison -----------------------------------------------------------

	if (n.is_type<grammar::comparison>()) {
		// children: [left, comp_op, right]
		auto op = mapCompOp(n.children[1]->string_view());
		return ast::makeExpr(
			ast::BinaryExpr(op,
				buildExpr(*n.children[0]),
				buildExpr(*n.children[2])),
			makeSpan(n));
	}

	// -- Logical chains (no explicit operator nodes) --------------------------

	if (n.is_type<grammar::and_expr>()) {
		return buildLogicalChain(n, ast::BinaryOp::And);
	}

	if (n.is_type<grammar::or_expr>() ||
	    n.is_type<grammar::match_or_expr>()) {
		return buildLogicalChain(n, ast::BinaryOp::Or);
	}

	// -- Ternary --------------------------------------------------------------

	if (n.is_type<grammar::ternary>() ||
	    n.is_type<grammar::match_ternary>() ||
	    n.is_type<grammar::expr>()) {
		// children: [condition, thenBranch, elseBranch]
		return ast::makeExpr(
			ast::TernaryExpr(
				buildExpr(*n.children[0]),
				buildExpr(*n.children[1]),
				buildExpr(*n.children[2])),
			makeSpan(n));
	}

	// -- Call -----------------------------------------------------------------

	if (n.is_type<grammar::call>()) {
		// children: [ident(funcName), arg, arg, ...]
		// Each arg is either a named_arg node or a bare expression node.
		std::string funcName(n.children[0]->string_view());
		std::vector<ast::Arg> args;
		for (size_t i = 1; i < n.children.size(); ++i) {
			const auto& child = *n.children[i];
			if (child.is_type<grammar::named_arg>()) {
				// named_arg children: [ident(name), expr]
				std::string argName(child.children[0]->string_view());
				args.emplace_back(std::move(argName), buildExpr(*child.children[1]));
			} else {
				// Positional argument — the child IS the expression.
				args.emplace_back(std::nullopt, buildExpr(child));
			}
		}
		return ast::makeExpr(
			ast::CallExpr(std::move(funcName), std::move(args)), makeSpan(n));
	}

	// -- Shared block ---------------------------------------------------------

	if (n.is_type<grammar::shared_block>()) {
		// children: [optional kw_any_age, shared_branch, shared_branch, ...]
		bool anyAge = false;
		std::vector<ast::SharedBranch> branches;
		for (const auto& child : n.children) {
			if (child->is_type<grammar::kw_any_age>()) {
				anyAge = true;
			} else if (child->is_type<grammar::shared_branch>()) {
				// shared_branch children: [kw_here|ident, expr]
				std::optional<std::string> region;
				if (!child->children[0]->is_type<grammar::kw_here>()) {
					region = std::string(child->children[0]->string_view());
				}
				branches.emplace_back(std::move(region),
					buildExpr(*child->children[1]));
			}
		}
		return ast::makeExpr(
			ast::SharedBlock(anyAge, std::move(branches)), makeSpan(n));
	}

	// -- Any-age block --------------------------------------------------------

	if (n.is_type<grammar::any_age_block>()) {
		// children: [kw_any_age, body_expr]
		return ast::makeExpr(
			ast::AnyAgeBlock(buildExpr(*n.children.back())), makeSpan(n));
	}

	// -- Match expression -----------------------------------------------------

	if (n.is_type<grammar::match_expr>()) {
		// children: [ident(discriminant), match_arm, match_arm, ...]
		std::string discriminant(n.children[0]->string_view());
		std::vector<ast::MatchArm> arms;

		for (size_t i = 1; i < n.children.size(); ++i) {
			const auto& armNode = *n.children[i];
			// match_arm children: [match_pattern, body_expr, optional trailing_or]

			// Patterns
			std::vector<std::string> patterns;
			const auto& patNode = *armNode.children[0];
			for (const auto& p : patNode.children) {
				patterns.emplace_back(std::string(p->string_view()));
			}

			// Body
			ast::ExprPtr body = buildExpr(*armNode.children[1]);

			// Fallthrough
			bool fallthrough = armNode.children.size() > 2 &&
				armNode.children.back()->is_type<grammar::trailing_or>();

			arms.emplace_back(
				std::move(patterns), std::move(body), fallthrough);
		}

		return ast::makeExpr(
			ast::MatchExpr(std::move(discriminant), std::move(arms)),
			makeSpan(n));
	}

	throw std::runtime_error(
		std::string("buildExpr: unhandled node type: ") + std::string(n.type));
}

// =============================================================================
// Param builder
// =============================================================================

ast::Param buildParam(const Node& n) {
	// param children: [ident(name), optional type, optional default_expr]
	std::string name(n.children[0]->string_view());
	std::optional<std::string> type;
	ast::ExprPtr defaultValue;

	for (size_t i = 1; i < n.children.size(); ++i) {
		if (n.children[i]->is_type<grammar::type>()) {
			type = std::string(n.children[i]->string_view());
		} else {
			defaultValue = buildExpr(*n.children[i]);
		}
	}

	return ast::Param(std::move(name), std::move(type), std::move(defaultValue));
}

// =============================================================================
// Entry builder
// =============================================================================

ast::Entry buildEntry(const Node& n) {
	// entry children: [ident(name), expr]
	std::string name(n.children[0]->string_view());
	return ast::Entry(std::move(name), buildExpr(*n.children[1]), makeSpan(n));
}

// =============================================================================
// Section builder
// =============================================================================

ast::Section buildSection(const Node& n) {
	// section children: [section_kind, entry, entry, ...]
	ast::SectionKind kind;
	auto s = n.children[0]->string_view();
	if (s == "events")        kind = ast::SectionKind::Events;
	else if (s == "locations") kind = ast::SectionKind::Locations;
	else                       kind = ast::SectionKind::Exits;

	std::vector<ast::Entry> entries;
	for (size_t i = 1; i < n.children.size(); ++i) {
		entries.push_back(buildEntry(*n.children[i]));
	}

	return ast::Section(kind, std::move(entries));
}

// =============================================================================
// Declaration builders
// =============================================================================

ast::RegionDecl buildRegionDecl(const Node& n) {
	// children: [ident(name), scene_prop, optional time_prop,
	//            optional areas_prop, section, section, ...]
	std::string name(n.children[0]->string_view());

	std::optional<std::string> scene;
	ast::TimePasses timePasses = ast::TimePasses::Auto;
	std::vector<std::string> areas;
	std::vector<ast::Section> sections;

	for (size_t i = 1; i < n.children.size(); ++i) {
		const auto& child = *n.children[i];
		if (child.is_type<grammar::scene_prop>()) {
			// scene_prop children: [ident(scene_name)]
			scene = std::string(child.children[0]->string_view());
		} else if (child.is_type<grammar::time_prop>()) {
			timePasses = (child.string_view() == "time_passes")
				? ast::TimePasses::Yes
				: ast::TimePasses::No;
		} else if (child.is_type<grammar::areas_prop>()) {
			// areas_prop children: [ident, ident, ...]
			for (const auto& area : child.children) {
				areas.emplace_back(std::string(area->string_view()));
			}
		} else if (child.is_type<grammar::section>()) {
			sections.push_back(buildSection(child));
		}
	}

	return ast::RegionDecl(
		std::move(name),
		ast::RegionBody(std::move(scene), timePasses,
			std::move(areas), std::move(sections)),
		makeSpan(n));
}

ast::ExtendRegionDecl buildExtendDecl(const Node& n) {
	// children: [ident(name), section, section, ...]
	std::string name(n.children[0]->string_view());
	std::vector<ast::Section> sections;
	for (size_t i = 1; i < n.children.size(); ++i) {
		sections.push_back(buildSection(*n.children[i]));
	}
	return ast::ExtendRegionDecl(
		std::move(name), std::move(sections), makeSpan(n));
}

ast::DefineDecl buildDefineDecl(const Node& n) {
	// children: [ident(name), param, param, ..., body_expr]
	std::string name(n.children[0]->string_view());
	std::vector<ast::Param> params;
	ast::ExprPtr body;

	for (size_t i = 1; i < n.children.size(); ++i) {
		if (n.children[i]->is_type<grammar::param>()) {
			params.push_back(buildParam(*n.children[i]));
		} else {
			body = buildExpr(*n.children[i]);
		}
	}

	return ast::DefineDecl(
		std::move(name), std::move(params), std::move(body), makeSpan(n));
}

ast::EnemyDecl buildEnemyDecl(const Node& n) {
	// children: [ident(name), enemy_field, enemy_field, ...]
	std::string name(n.children[0]->string_view());
	std::vector<ast::EnemyField> fields;

	for (size_t i = 1; i < n.children.size(); ++i) {
		const auto& fieldNode = *n.children[i];
		// enemy_field children: [enemy_field_kind, param, ..., body_expr]

		ast::EnemyFieldKind kind;
		auto s = fieldNode.children[0]->string_view();
		if (s == "kill")       kind = ast::EnemyFieldKind::Kill;
		else if (s == "pass")  kind = ast::EnemyFieldKind::Pass;
		else if (s == "drop")  kind = ast::EnemyFieldKind::Drop;
		else                   kind = ast::EnemyFieldKind::Avoid;

		std::vector<ast::Param> params;
		ast::ExprPtr body;

		for (size_t j = 1; j < fieldNode.children.size(); ++j) {
			if (fieldNode.children[j]->is_type<grammar::param>()) {
				params.push_back(buildParam(*fieldNode.children[j]));
			} else {
				body = buildExpr(*fieldNode.children[j]);
			}
		}

		fields.emplace_back(kind, std::move(params), std::move(body),
			makeSpan(fieldNode));
	}

	return ast::EnemyDecl(std::move(name), std::move(fields), makeSpan(n));
}

ast::Decl buildDecl(const Node& n) {
	if (n.is_type<grammar::region_decl>()) return buildRegionDecl(n);
	if (n.is_type<grammar::extend_decl>()) return buildExtendDecl(n);
	if (n.is_type<grammar::define_decl>()) return buildDefineDecl(n);
	if (n.is_type<grammar::enemy_decl>())  return buildEnemyDecl(n);
	throw std::runtime_error(
		std::string("buildDecl: unhandled node type: ") + std::string(n.type));
}

} // anonymous namespace

// =============================================================================
// Public entry point
// =============================================================================

ast::File buildFile(const Node& root) {
	ast::File file;
	// The parse tree's synthetic root contains the rls_file node as its
	// sole child.  Declarations live one level deeper.
	if (!root.children.empty()) {
		const auto& fileNode = *root.children[0];
		for (const auto& child : fileNode.children) {
			file.declarations.push_back(buildDecl(*child));
		}
	}
	return file;
}

} // namespace rls::parser
