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

#include <vector>

namespace rls::parser {

// == Selector =================================================================

/// Selector template: maps each grammar Rule to a parse-tree node policy.
template <typename Rule>
using selector = tao::pegtl::parse_tree::selector<
	Rule,

	// -- Leaf / token nodes (store matched text) ------------------------------
	tao::pegtl::parse_tree::store_content::on<
		grammar::ident,
		grammar::match_default,
		grammar::integer,
		grammar::glob_pattern,
		grammar::string_literal,
		grammar::atom_keyword,
		grammar::invoke_suffix,
		grammar::parameter_type_name,
		grammar::return_type_name,
		grammar::enum_name,
		grammar::member_object,
		grammar::member_name,
		grammar::call_callee,
		grammar::named_argument_label,
		grammar::entry_label,
		grammar::region_data_key,
		grammar::region_name,
		grammar::comp_op,
		grammar::mul_div_op,
		grammar::add_sub_op,
		grammar::section_kind,
		grammar::section,
		grammar::region_decl,
		grammar::extend_decl,
		grammar::define_decl,
		grammar::extern_define_decl,
		grammar::enum_decl,
		grammar::extern_enum_decl,
		grammar::enum_member,
		grammar::extern_enum_entry,
		grammar::region_data_entry,
		grammar::entry,
		grammar::invoke_call,
		grammar::call,
		grammar::member_access,
		grammar::named_arg,
		grammar::match_expr,
		grammar::match_arm,
		grammar::match_pattern,
		grammar::list_expr,
		grammar::kw_not,       // marker: unary "not"
		grammar::kw_here,      // `here` keyword atom (resolves to current region)
		grammar::trailing_or   // marker: fallthrough in match arms
	>,

	// -- Structural nodes (children matter, text doesn't) ---------------------
	tao::pegtl::parse_tree::remove_content::on<
		// File root
		grammar::rls_file,
		// Parameters
		grammar::param
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
		grammar::match_single_pattern,
		grammar::arg,
		grammar::declaration
	>
>;

// == Builder ==================================================================

/// Walk a PEGTL parse tree (CST) and produce an AST File.
/// Diagnostics encountered during CST-to-AST conversion are appended to diags.
ast::File buildFile(const tao::pegtl::parse_tree::node& root,
                    std::vector<ast::Diagnostic>& diags);

} // namespace rls::parser
