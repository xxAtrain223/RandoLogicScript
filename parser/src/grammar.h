#pragma once

#include <tao/pegtl.hpp>

namespace rls::parser::grammar {

using namespace tao::pegtl;

// == Keywords =================================================================

// Top-level declarations
struct kw_region : TAO_PEGTL_STRING("region") {};
struct kw_extend : TAO_PEGTL_STRING("extend") {};
struct kw_define : TAO_PEGTL_STRING("define") {};
struct kw_enemy : TAO_PEGTL_STRING("enemy") {};

// Region sections
struct kw_events : TAO_PEGTL_STRING("events") {};
struct kw_locations : TAO_PEGTL_STRING("locations") {};
struct kw_exits : TAO_PEGTL_STRING("exits") {};

// Region properties
struct kw_scene : TAO_PEGTL_STRING("scene") {};
struct kw_time_passes : TAO_PEGTL_STRING("time_passes") {};
struct kw_no_time_passes : TAO_PEGTL_STRING("no_time_passes") {};
struct kw_areas : TAO_PEGTL_STRING("areas") {};

// Boolean literals and aliases
struct kw_true : TAO_PEGTL_STRING("true") {};
struct kw_false : TAO_PEGTL_STRING("false") {};
struct kw_always : TAO_PEGTL_STRING("always") {};
struct kw_never : TAO_PEGTL_STRING("never") {};

// Logical operators
struct kw_and : TAO_PEGTL_STRING("and") {};
struct kw_or : TAO_PEGTL_STRING("or") {};
struct kw_not : TAO_PEGTL_STRING("not") {};

// Comparison operator alias
struct kw_is : TAO_PEGTL_STRING("is") {};

// Age and time
struct kw_is_child : TAO_PEGTL_STRING("is_child") {};
struct kw_is_adult : TAO_PEGTL_STRING("is_adult") {};
struct kw_at_day : TAO_PEGTL_STRING("at_day") {};
struct kw_at_night : TAO_PEGTL_STRING("at_night") {};
struct kw_any_age : TAO_PEGTL_STRING("any_age") {};

// Dungeon variant selection
struct kw_is_vanilla : TAO_PEGTL_STRING("is_vanilla") {};
struct kw_is_mq : TAO_PEGTL_STRING("is_mq") {};

// Shared / multi-region blocks
struct kw_shared : TAO_PEGTL_STRING("shared") {};
struct kw_from : TAO_PEGTL_STRING("from") {};
struct kw_here : TAO_PEGTL_STRING("here") {};

// Match expressions
struct kw_match : TAO_PEGTL_STRING("match") {};

// Enemy fields
struct kw_kill : TAO_PEGTL_STRING("kill") {};
struct kw_pass : TAO_PEGTL_STRING("pass") {};
struct kw_drop : TAO_PEGTL_STRING("drop") {};
struct kw_avoid : TAO_PEGTL_STRING("avoid") {};

// == Character classes ========================================================

/// Matches [a-zA-Z_] — valid first character of an identifier.
/// Named `ident_first` to avoid clashing with PEGTL's inline `ascii::identifier_first`.
struct ident_first : sor<alpha, one<'_'>> {};

/// Matches [a-zA-Z0-9_] — valid continuation character of an identifier.
/// Named `ident_other` to avoid clashing with PEGTL's inline `ascii::identifier_other`.
struct ident_other : sor<alnum, one<'_'>> {};

// == Whitespace & comments ====================================================

/// Line comment: `# ... <eol>` — from `#` to end of line (or end of input).
struct line_comment : seq<one<'#'>, until<eolf>> {};

/// A single whitespace element: space/tab/newline or a line comment.
struct ws : sor<space, line_comment> {};

/// Optional whitespace (skipper) — used between tokens.
struct _ : star<ws> {};

// == Keyword boundary =========================================================

/// Wraps a keyword string rule so it only matches when NOT followed by an
/// identifier continuation character. Prevents `region` from matching inside
/// `regionFoo` or `always` inside `always_something`.
template <typename str>
struct kw : seq<str, not_at<ident_other>> {};

// == Identifiers & literals ===================================================

/// All reserved words that must not be parsed as plain identifiers.
/// Listed longest-first where prefixes overlap (e.g. `no_time_passes`
/// before `not`, `is_vanilla`/`is_mq`/`is_child`/`is_adult` before `is`).
struct reserved : sor<
	// Multi-word / long keywords first (prefix-safe ordering)
	kw<kw_no_time_passes>,
	kw<kw_time_passes>,
	kw<kw_is_vanilla>,
	kw<kw_is_child>,
	kw<kw_is_adult>,
	kw<kw_is_mq>,
	// Top-level declarations
	kw<kw_region>,
	kw<kw_extend>,
	kw<kw_define>,
	kw<kw_enemy>,
	// Region sections
	kw<kw_events>,
	kw<kw_locations>,
	kw<kw_exits>,
	// Region properties
	kw<kw_scene>,
	kw<kw_areas>,
	// Boolean literals and aliases
	kw<kw_true>,
	kw<kw_false>,
	kw<kw_always>,
	kw<kw_never>,
	// Logical operators
	kw<kw_and>,
	kw<kw_or>,
	kw<kw_not>,
	// Comparison alias
	kw<kw_is>,
	// Age and time
	kw<kw_at_day>,
	kw<kw_at_night>,
	kw<kw_any_age>,
	// Shared blocks
	kw<kw_shared>,
	kw<kw_from>,
	kw<kw_here>,
	// Match
	kw<kw_match>,
	// Enemy fields
	kw<kw_kill>,
	kw<kw_pass>,
	kw<kw_drop>,
	kw<kw_avoid>
> {};

/// Named identifier: enum values (`RG_HOOKSHOT`), parameters, region names, etc.
/// An identifier is any `[a-zA-Z_][a-zA-Z0-9_]*` sequence that is NOT a reserved word.
/// Named `ident` to avoid clashing with PEGTL's inline `ascii::identifier`.
struct ident : seq<not_at<reserved>, ident_first, star<ident_other>> {};

/// Integer literal: one or more digits.
struct integer : seq<opt<one<'-'>>, plus<digit>> {};

// == Punctuation ==============================================================

struct open_brace    : one<'{'> {};
struct close_brace   : one<'}'> {};
struct open_paren    : one<'('> {};
struct close_paren   : one<')'> {};
struct colon         : one<':'> {};
struct comma         : one<','> {};
struct question_mark : one<'?'> {};

// == Operators ================================================================

// Comparison (ordered longest-first to avoid prefix ambiguity)
struct op_eq  : string<'=', '='> {};
struct op_neq : string<'!', '='> {};
struct op_gte : string<'>', '='> {};
struct op_lte : string<'<', '='> {};
struct op_gt  : one<'>'> {};
struct op_lt  : one<'<'> {};

// Arithmetic
struct op_plus  : one<'+'> {};
struct op_minus : one<'-'> {};
struct op_star  : one<'*'> {};
struct op_slash : one<'/'> {};

} // namespace rls::parser::grammar
