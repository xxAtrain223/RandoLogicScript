#pragma once

// =============================================================================
// PEGTL Crash Course — features used in this grammar
// =============================================================================
//
// PEGTL (Parsing Expression Grammar Template Library) defines parsers as C++
// types. Every grammar rule is a struct that inherits from a PEGTL combinator.
// The library then template-instantiates a recursive-descent parser at compile
// time — there is no separate lexer pass.
//
// ── Matching primitives ──────────────────────────────────────────────────────
//
//   one<'c'>              Match a single literal character.
//   string<'a','b','c'>   Match a fixed character sequence ("abc").
//   TAO_PEGTL_STRING("x") Convenience macro — expands a string literal into
//                          a struct inheriting from `string<...>`. Needed
//                          because C++ doesn't allow string literals as
//                          template arguments (pre-C++20 NTTP).
//   alpha                 [a-zA-Z]
//   alnum                 [a-zA-Z0-9]
//   digit                 [0-9]
//   space                 Any single whitespace character (space, tab, \n, \r).
//
// ── Combinators (compose rules into bigger rules) ────────────────────────────
//
//   seq<A, B, C>          Sequence — match A then B then C (all must succeed).
//   sor<A, B, C>          Ordered choice — try A, if it fails try B, then C.
//                          First match wins (PEG semantics, no ambiguity).
//   star<R>               Zero or more — greedy Kleene star: match R repeatedly.
//   plus<R>               One or more — like star but must match at least once.
//   opt<R>                Optional — match R zero or one time.
//   list<R, Sep>          Shorthand for `seq<R, star<seq<Sep, R>>>` — matches
//                          R separated by Sep (e.g. comma-separated arguments).
//   until<R>              Consume characters until R matches (R is consumed too).
//                          Used here for line comments: `seq<one<'#'>, until<eolf>>`.
//
// ── Lookahead (do NOT consume input) ─────────────────────────────────────────
//
//   at<R>                 Positive lookahead — succeed if R would match, but
//                          don't advance the input position.
//   not_at<R>             Negative lookahead — succeed if R would NOT match.
//                          Used for keyword boundaries: `not_at<ident_other>`
//                          ensures "region" doesn't match inside "regionFoo".
//
// ── Sentinels ────────────────────────────────────────────────────────────────
//
//   eof                   End of input.
//   eolf                  End of line (any newline sequence) OR end of input.
//
// ── Error handling ───────────────────────────────────────────────────────────
//
//   must<A, B, C>         Like seq, but if any rule fails AFTER the first one,
//                          it throws a `parse_error` instead of backtracking.
//                          We use `must<Rule, eof>` in tests to assert a rule
//                          matches the ENTIRE input string.
//
// ── Running the parser ───────────────────────────────────────────────────────
//
//   memory_input in(str, src);     Create an input from a string.
//   parse<Rule>(in);               Run the parser. Returns true/false.
//   parse<Rule, Action>(in);       Run with actions — Action<R>::apply() is
//                                   called whenever rule R matches. This is
//                                   how we'll build AST nodes (future step).
//
// ── How to read this file ────────────────────────────────────────────────────
//
// Each grammar rule is a struct that inherits from a combinator:
//
//   struct my_rule : seq<alpha, star<alnum>> {};   // [a-zA-Z][a-zA-Z0-9]*
//
// Rules reference each other by name — PEGTL resolves them at compile time.
// Forward declarations (e.g. `struct expr;`) allow mutually recursive rules
// like `expr → ... → primary → "(" expr ")"`.
//
// The `_` rule (underscore) is our "skipper" — optional whitespace and
// comments. Insert `_` between tokens to allow flexible formatting:
//
//   struct example : seq<kw<kw_if>, _, open_paren, _, expr, _, close_paren> {};
//
// =============================================================================

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

// == Expression grammar =======================================================
//
// Precedence (lowest to highest):
//   ternary  →  or  →  and  →  comparison  →  add/sub  →  mul/div  →  unary  →  primary
//
// PEGTL note: several rules are mutually recursive (expr → ternary → ... →
// primary → "(" expr ")"), so we forward-declare `expr` and define it after
// `ternary`.

// Forward declaration — defined below after ternary.
struct expr;

// -- Atoms & primary expressions ----------------------------------------------

/// Keyword atoms that evaluate to a value by themselves.
struct atom_keyword : sor<
	kw<kw_always>, kw<kw_never>,
	kw<kw_is_child>, kw<kw_is_adult>,
	kw<kw_at_day>, kw<kw_at_night>,
	kw<kw_is_vanilla>, kw<kw_is_mq>,
	kw<kw_true>, kw<kw_false>
> {};

/// atom = atom_keyword | IDENT | NUMBER
/// (IDENT and NUMBER are tried last; call / shared / any_age / match are
/// handled separately by `primary` so they take priority.)
struct atom : sor<atom_keyword, ident, integer> {};

/// Named argument:  IDENT ":" expr
struct named_arg : seq<ident, _, colon, _, expr> {};

/// A single call argument — named (IDENT ":" expr) or positional (expr).
/// We try `named_arg` first because it starts with `ident` which would also
/// match as a plain expression.
struct arg : sor<named_arg, expr> {};

/// Argument list (possibly empty) between parentheses.
struct arg_list : opt<list<arg, seq<_, comma, _>>> {};

/// Function call:  IDENT "(" arg_list ")"
struct call : seq<ident, _, open_paren, _, arg_list, _, close_paren> {};

/// shared_branch = "from" (IDENT | "here") ":" expr
struct shared_branch : seq<kw<kw_from>, _, sor<kw<kw_here>, ident>, _, colon, _, expr> {};

/// shared_block = "shared" "any_age"? "{" shared_branch+ "}"
struct shared_block : seq<
	kw<kw_shared>, _,
	opt<seq<kw<kw_any_age>, _>>,
	open_brace, _,
	plus<seq<shared_branch, _>>,
	close_brace
> {};

/// any_age_block = "any_age" "{" expr "}"
struct any_age_block : seq<kw<kw_any_age>, _, open_brace, _, expr, _, close_brace> {};

// Forward declaration — match_expr is defined after ternary because its arms
// need the match-aware or_expr variant (which depends on and_expr).
struct match_expr;

/// Parenthesised expression: "(" expr ")"
struct paren_expr : seq<open_paren, _, expr, _, close_paren> {};

/// primary = call | shared_block | any_age_block | match_expr | atom | "(" expr ")"
///
/// Ordering matters:
///   - `call` before `atom` (both start with `ident`, but call continues with "(")
///   - `shared_block` before `any_age_block` (shared can contain "any_age")
///   - `match_expr` before `atom` (match starts with `kw_match` keyword)
struct primary : sor<call, shared_block, any_age_block, match_expr, paren_expr, atom> {};

// -- Unary / binary / ternary -------------------------------------------------

/// unary = "not" unary | primary
struct unary : sor<seq<kw<kw_not>, _, unary>, primary> {};

/// mul_div = unary (("*" | "/") _ unary)*
struct mul_div_op : sor<op_star, op_slash> {};
struct mul_div : seq<unary, star<seq<_, mul_div_op, _, unary>>> {};

/// add_sub = mul_div (("+" | "-") _ mul_div)*
struct add_sub_op : sor<op_plus, op_minus> {};
struct add_sub : seq<mul_div, star<seq<_, add_sub_op, _, mul_div>>> {};

/// comp_op = "==" | "is" "not" | "is" | "!=" | ">=" | "<=" | ">" | "<"
/// "is not" must be tried before bare "is".
struct comp_op : sor<
	op_eq,
	seq<kw<kw_is>, _, kw<kw_not>>,   // "is not"
	kw<kw_is>,                         // bare "is"
	op_neq, op_gte, op_lte, op_gt, op_lt
> {};

/// comparison = add_sub (comp_op _ add_sub)?
struct comparison : seq<add_sub, opt<seq<_, comp_op, _, add_sub>>> {};

/// and_expr = comparison ("and" comparison)*
struct and_expr : seq<comparison, star<seq<_, kw<kw_and>, _, comparison>>> {};

/// or_expr = and_expr ("or" and_expr)*
struct or_expr : seq<and_expr, star<seq<_, kw<kw_or>, _, and_expr>>> {};

/// ternary = or_expr ("?" ternary ":" ternary)?
struct ternary : seq<or_expr, opt<seq<_, question_mark, _, ternary, _, colon, _, ternary>>> {};

/// expr = ternary
struct expr : ternary {};

// -- Match expressions (defined here because match arms need and_expr) --------

/// match_pattern = IDENT ("or" IDENT)*
struct match_pattern : list<ident, seq<_, kw<kw_or>, _>> {};

/// Lookahead: the start of the next match arm (pattern followed by ":").
/// Used to distinguish a trailing "or" (fallthrough) from a binary "or".
struct next_arm_head : seq<_, ident, star<seq<_, kw<kw_or>, _, ident>>, _, colon> {};

/// trailing_or = "or" at(next_arm_head)
/// A trailing "or" is a fallthrough marker — it's the "or" keyword followed
/// by the start of a new match arm (one or more identifiers then ":").
struct trailing_or : seq<kw<kw_or>, at<next_arm_head>> {};

/// Binary "or" inside a match arm expression — NOT a trailing fallthrough.
struct match_or_op : seq<not_at<trailing_or>, kw<kw_or>> {};

/// Match-arm-specific or_expr: like or_expr but stops before trailing "or".
struct match_or_expr : seq<and_expr, star<seq<_, match_or_op, _, and_expr>>> {};

/// Match-arm ternary: uses match_or_expr at the outermost level.
/// The ternary ? : branches use the REGULAR ternary rule because the ? :
/// delimiters scope the expression, and ternary-with-trailing-or in match
/// arms is an extremely unlikely edge case.
struct match_ternary : seq<match_or_expr, opt<seq<_, question_mark, _, ternary, _, colon, _, ternary>>> {};

/// match_arm = match_pattern ":" match_ternary trailing_or?
struct match_arm : seq<match_pattern, _, colon, _, match_ternary, _, opt<trailing_or>> {};

/// match_expr = "match" IDENT "{" match_arm+ "}"
struct match_expr : seq<
	kw<kw_match>, _, ident, _,
	open_brace, _,
	plus<seq<match_arm, _>>,
	close_brace
> {};

} // namespace rls::parser::grammar
