#pragma once

/// @file rls_match.h
/// @brief Runtime helper for evaluating RLS match expressions in generated C++.
///
/// RLS match expressions transpile into calls to rls::match(), which emulates
/// a switch/case with fallthrough using variadic template recursion.
///
/// Arguments are passed as flat triples of (condition, body, fallthrough):
///
///   rls::match(
///       condition1, body1, fallthrough1,
///       condition2, body2, fallthrough2,
///       ...
///       conditionN, bodyN, fallthroughN
///   );
///
/// - condition: a callable returning bool (typically a lambda checking the
///   discriminant, e.g. [&]{ return distance == ED_CLOSE; })
/// - body: a callable returning the result type (e.g. [&]{ return canUseSword; })
/// - fallthrough: a bool indicating whether the next arm should also be
///   evaluated if this arm matches (true = fallthrough, false = stop)
///
/// @par Semantics
/// Arms are evaluated left-to-right.  The first arm whose condition returns
/// true is the "entry point":
/// - If fallthrough is false, its body is returned immediately.
/// - If fallthrough is true, its body is evaluated.  If the result is truthy,
///   it is returned (short-circuit).  Otherwise, evaluation continues to the
///   next arm unconditionally (ignoring its condition), repeating the
///   fallthrough check.
///
/// If no arm matches, a value-initialized default is returned (false for bool,
/// 0 for int, etc.).
///
/// @par Multi-value arms
/// Arms matching multiple patterns (e.g. `ED_CLOSE or ED_SHORT_JUMPSLASH`)
/// are handled at the transpiler level by generating compound conditions:
///   [&]{ return distance == ED_CLOSE || distance == ED_SHORT_JUMPSLASH; }
///
/// @par Return type
/// The return type is deduced from the body lambdas and can be any type that
/// supports static_cast<bool> (bool, int, enum, etc.).
///
/// @par Example: simple match (no fallthrough)
/// @code
/// // RLS source:
/// //   match distance {
/// //       ED_CLOSE: can_use(RG_KOKIRI_SWORD)
/// //       ED_FAR:   can_use(RG_FAIRY_BOW)
/// //   }
/// bool result = rls::match(
///     [&]{ return distance == ED_CLOSE; }, [&]{ return logic->CanUse(RG_KOKIRI_SWORD); }, false,
///     [&]{ return distance == ED_FAR; },   [&]{ return logic->CanUse(RG_FAIRY_BOW); },   false);
/// @endcode
///
/// @par Example: fallthrough (distance hierarchy)
/// @code
/// // RLS source:
/// //   match distance {
/// //       ED_CLOSE:    can_use(RG_KOKIRI_SWORD) or
/// //       ED_HOOKSHOT: can_use(RG_HOOKSHOT) or
/// //       ED_FAR:      can_use(RG_FAIRY_BOW)
/// //   }
/// // Matching ED_CLOSE evaluates all three bodies, returning the first
/// // truthy result.  Matching ED_HOOKSHOT skips ED_CLOSE entirely.
/// bool result = rls::match(
///     [&]{ return distance == ED_CLOSE; },    [&]{ return logic->CanUse(RG_KOKIRI_SWORD); },    true,
///     [&]{ return distance == ED_HOOKSHOT; }, [&]{ return logic->CanUse(RG_HOOKSHOT); }, true,
///     [&]{ return distance == ED_FAR; },      [&]{ return logic->CanUse(RG_FAIRY_BOW); },      false);
/// @endcode

namespace rls {

/// Return type alias: the type produced by calling a body lambda.
template<typename BodyT>
using body_result_t = decltype(std::declval<BodyT>()());

/// Walk arms left-to-right looking for the first arm whose condition lambda
/// returns true.  Once found, evaluate its body and (if fallthrough)
/// continue evaluating subsequent arm bodies, returning the first truthy
/// result.
///
/// If no arm matches, returns a value-initialized default (false for bool,
/// 0 for int, etc.).
///
/// The template parameter Active is an implementation detail used for
/// fallthrough recursion — callers should never specify it.
template<bool Active = false, typename ConditionT, typename BodyT, typename... Rest>
auto match(ConditionT condition, BodyT body, bool fallthrough, Rest... rest)
	-> body_result_t<BodyT>
{
	if constexpr (sizeof...(Rest) == 0) {
		if (Active || condition()) return body();
		return body_result_t<BodyT>{};
	} else {
		if (Active || condition()) {
			if (fallthrough) {
				auto result = body();
				if (static_cast<bool>(result)) return result;
				return match<true>(rest...);
			}
			return body();
		}
		return match(rest...);
	}
}

} // namespace rls
