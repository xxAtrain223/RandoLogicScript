#pragma once

// =============================================================================
// Parse-tree selector & CST-to-AST builder
// =============================================================================
//
// Tells PEGTL's parse_tree::parse() which grammar rules become CST nodes.
// Three categories:
//
//   store_content   Leaf / token nodes — keep matched text.
//   remove_content  Structural nodes — keep children, discard text.
//   fold_one        Transparent wrappers — collapse when a single child
//                   remains (precedence layers, grouping parens, etc.).
//
// The resulting CST is then walked procedurally to produce ast:: types.
// =============================================================================

#include "ast.h"
#include "grammar.h"

#include <tao/pegtl/contrib/parse_tree.hpp>

namespace rls::parser {

// == Selector =================================================================

/// Selector template: maps each grammar Rule to a parse-tree node policy.
template <typename Rule>
using selector = tao::pegtl::parse_tree::selector<
	Rule,

	// -- Leaf / token nodes (store matched text) ------------------------------
	tao::pegtl::parse_tree::store_content::on<
		grammar::ident,
		grammar::integer,
		grammar::atom_keyword,
		grammar::type,
		grammar::comp_op,
		grammar::mul_div_op,
		grammar::add_sub_op,
		grammar::section_kind,
		grammar::enemy_field_kind,
		grammar::time_prop,
		grammar::kw_not,       // marker: unary "not"
		grammar::kw_any_age,   // marker: any_age in shared blocks
		grammar::kw_here,      // marker: "from here" in shared branches
		grammar::trailing_or   // marker: fallthrough in match arms
	>,

	// -- Structural nodes (children matter, text doesn't) ---------------------
	tao::pegtl::parse_tree::remove_content::on<
		// File root
		grammar::rls_file,
		// Top-level declarations
		grammar::region_decl,
		grammar::extend_decl,
		grammar::define_decl,
		grammar::enemy_decl,
		grammar::enemy_field,
		// Region properties
		grammar::scene_prop,
		grammar::areas_prop,
		// Sections & entries
		grammar::section,
		grammar::entry,
		// Parameters
		grammar::param,
		// Expressions
		grammar::call,
		grammar::named_arg,
		grammar::shared_block,
		grammar::shared_branch,
		grammar::any_age_block,
		grammar::match_expr,
		grammar::match_arm,
		grammar::match_pattern
	>,

	// -- Transparent wrappers (fold when single child) ------------------------
	//
	// Precedence layers: each one collapses into its operand when there
	// is no actual operator at that level.  E.g. `and_expr` wrapping a
	// single `comparison` folds away, but `a and b` keeps the node.
	tao::pegtl::parse_tree::fold_one::on<
		grammar::expr,
		grammar::ternary,
		grammar::or_expr,
		grammar::and_expr,
		grammar::comparison,
		grammar::add_sub,
		grammar::mul_div,
		grammar::unary,
		grammar::primary,
		grammar::atom,
		grammar::paren_expr,
		grammar::match_ternary,
		grammar::match_or_expr,
		grammar::arg,
		grammar::declaration
	>
>;

// == Builder ==================================================================

/// Walk a PEGTL parse tree (CST) and produce an AST File.
ast::File buildFile(const tao::pegtl::parse_tree::node& root);

} // namespace rls::parser
