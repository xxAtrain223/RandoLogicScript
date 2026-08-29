// Tests for SoH host-call rewrites and special-cases: how `trick`, `check_price`,
// wallet-capacity and triforce-hunt comparisons, and age-conditional ternaries are lowered
// to the oot_soh runtime helpers. OptionFilter / setting rendering is covered by
// option_filter_tests.cpp.
#include "helpers.h"

using namespace rls::transpilers::soh_ap_tests;

// `trick(...)` is rewritten to the SoH can_do_trick host call, threading the bundle
// receiver and prefixing the value with the Tricks enum class.
TEST(SohApHostRewrites, TrickCallRewritesToCanDoTrick) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    trick(RT_FOO)\n",
		"test")),
		"can_do_trick(bundle, Tricks.RT_FOO)");
}

// `check_price(check) <= wallet_capacity()` collapses to just the affordability check.
TEST(SohApHostRewrites, WalletCapacityComparisonCollapsesToAffordCheck) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define check_price(check: Location) -> Int\n"
		"extern define wallet_capacity() -> Int\n"
		"define test():\n"
		"    check_price(RC_FOO) <= wallet_capacity()\n",
		"test")),
		"can_afford_slot(Locations.RC_FOO)");
}

// `collected_triforce_pieces() >= required_triforce_pieces()` collapses to CanWinTriforceHunt().
TEST(SohApHostRewrites, TriforceHuntComparisonCollapses) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define collected_triforce_pieces() -> Int\n"
		"extern define required_triforce_pieces() -> Int\n"
		"define test():\n"
		"    collected_triforce_pieces() >= required_triforce_pieces()\n",
		"test")),
		"CanWinTriforceHunt()");
}

// A host-provided define (has_bottle) is lowered as an opaque host rule, so it classifies as
// a Rule and composes with other rules under or/and -- even though its RLS body is the runtime
// value `bottle_count() >= 1`. Regression test: the classifier used to inline the body and
// reject the combination as "runtime value combined with a rule".
TEST(SohApHostRewrites, HostProvidedDefineComposesAsRule) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define bottle_count() -> Int\n"
		"define has_bottle():\n"
		"    bottle_count() >= 1\n"
		"define test():\n"
		"    has(RG_HOOKSHOT) or has_bottle()\n",
		"test")),
		"has_item(bundle, Items.RG_HOOKSHOT) | has_bottle(bundle)");
}

// `fire_timer() >= N` lowers to the world's fire_timer_at_least(bundle, N) host rule, so the
// state-dependent count re-evaluates instead of being frozen at build time.
TEST(SohApHostRewrites, TimerThresholdLowersToAtLeastRule) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define fire_timer() -> Int\n"
		"define test():\n"
		"    fire_timer() >= 48\n",
		"test")),
		"fire_timer_at_least(bundle, 48)");
}

// A strict `>` threshold is normalized to `>= N + 1` (Hearts() > 1 means at least 2 hearts).
TEST(SohApHostRewrites, StrictThresholdNormalizesToAtLeastPlusOne) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define hearts() -> Int\n"
		"define test():\n"
		"    hearts() > 1\n",
		"test")),
		"hearts_at_least(bundle, 2)");
}

// A `!= N` threshold normalizes to `>= N + 1` (effective_health() != 1 means at least 2, valid
// because the count floors at 1). Faithful to the C++ `EffectiveHealth() != 1`.
TEST(SohApHostRewrites, NotEqualThresholdNormalizesToAtLeastPlusOne) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define effective_health() -> Int\n"
		"define test():\n"
		"    effective_health() != 1\n",
		"test")),
		"effective_health_at_least(bundle, 2)");
}

// The lowered threshold is a Rule, so it composes with other rules under and/or -- the count
// comparison no longer trips the "runtime value combined with a rule" diagnostic.
TEST(SohApHostRewrites, TimerThresholdComposesAsRule) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define fire_timer() -> Int\n"
		"define test():\n"
		"    fire_timer() >= 16 and can_use(RG_LONGSHOT)\n",
		"test")),
		"fire_timer_at_least(bundle, 16) & can_use(bundle, Items.RG_LONGSHOT)");
}

// `ocarina_buttons() >= N` lowers to the world's has_enough_ocarina_buttons(bundle, N) host rule --
// the state-dependent button count re-evaluates instead of being frozen. Faithful to SoH's
// `OcarinaButtons() >= 2` inside ScarecrowsSong.
TEST(SohApHostRewrites, OcarinaButtonsThresholdLowersToHasEnoughRule) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define ocarina_buttons() -> Int\n"
		"define test():\n"
		"    ocarina_buttons() >= 2\n",
		"test")),
		"has_enough_ocarina_buttons(bundle, 2)");
}

// A ternary whose condition is a pure setting comparison and whose branches are build-time ints
// (small_keys' count arg) lowers to a plain Python conditional over OptionFilter.check(), which
// reads world.options (bundle[1]) at build time -- no Rule in the condition, so bool(rule) never
// fires. Faithful to SoH's `SmallKeys(scene, GerudoFortress.Is(NORMAL) ? 4 : 1)`.
TEST(SohApHostRewrites, SettingConditionedIntTernaryUsesOptionFilterCheck) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define small_keys(sc: Scene, count: Int) -> Bool\n"
		"define test():\n"
		"    small_keys(SCENE_THIEVES_HIDEOUT, setting(RSK_GERUDO_FORTRESS) is RO_GF_CARPENTERS_NORMAL ? 4 : 1)\n",
		"test")),
		"small_keys(bundle, Items.RG_GERUDO_FORTRESS_SMALL_KEY, "
		"4 if OptionFilter(RSK_GERUDO_FORTRESS, RandomizerSettingKey.RO_GF_CARPENTERS_NORMAL).check(bundle[1].options) "
		"else 1)");
}

// The same build-time lowering composes as a plain Python conditional selecting between two
// rules -- resolved at generation, so it needs no conditional host rule at all.
TEST(SohApHostRewrites, SettingConditionedRuleTernaryUsesOptionFilterCheck) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_GERUDO_FORTRESS) is RO_GF_CARPENTERS_NORMAL ? has(RG_HOOKSHOT) : has(RG_BOOMERANG)\n",
		"test")),
		"has_item(bundle, Items.RG_HOOKSHOT) "
		"if OptionFilter(RSK_GERUDO_FORTRESS, RandomizerSettingKey.RO_GF_CARPENTERS_NORMAL).check(bundle[1].options) "
		"else has_item(bundle, Items.RG_BOOMERANG)");
}

// A rule-conditioned ternary whose branches are enum VALUES cannot be handed to the conditional
// rule directly (its children must be rules). Instead the call is distributed over the ternary,
// so the conditional picks between two rules at solve time -- the faithful mirror of the C++
// `CanUse(IsAdult ? RG_HOOKSHOT : RG_LONGSHOT)`.
TEST(SohApHostRewrites, AgeConditionedItemArgTernaryDistributesToConditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define is_adult() -> Bool\n"
		"define test():\n"
		"    can_use(is_adult() ? RG_HOOKSHOT : RG_LONGSHOT)\n",
		"test")),
		"rls_conditional(bundle, is_adult(bundle), "
		"can_use(bundle, Items.RG_HOOKSHOT), can_use(bundle, Items.RG_LONGSHOT))");
}

// The condition may be any rule, and other (non-ternary) arguments -- here the enemy and the
// distance being selected -- ride along unchanged into each distributed call. Mirrors
// water_temple.cpp: CanKillEnemy(RE_GS, HasItem(RG_BRONZE_SCALE) && IsAdult ? ED_SHORT_JUMPSLASH : ED_BOOMERANG).
TEST(SohApHostRewrites, CompoundConditionedDistanceArgTernaryDistributesToConditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define is_adult() -> Bool\n"
		"extern define can_kill_at(e: Enemy, distance: Distance) -> Bool\n"
		"define test():\n"
		"    can_kill_at(RE_GOLD_SKULLTULA, has(RG_BRONZE_SCALE) and is_adult() ? ED_SHORT_JUMPSLASH : ED_BOOMERANG)\n",
		"test")),
		"rls_conditional(bundle, has_item(bundle, Items.RG_BRONZE_SCALE) & is_adult(bundle), "
		"can_kill_at(bundle, Enemies.RE_GOLD_SKULLTULA, EnemyDistance.ED_SHORT_JUMPSLASH), "
		"can_kill_at(bundle, Enemies.RE_GOLD_SKULLTULA, EnemyDistance.ED_BOOMERANG))");
}

// Distribution runs before the host-call rewrite, so a rule-conditioned count ternary fed to the
// small_keys host rewrite distributes too: each branch re-renders through the rewrite, keeping the
// scene->key mapping. Mirrors spirit_temple.cpp: SmallKeys(SCENE_SPIRIT_TEMPLE, (climb||hookshot)&&bracelet ? 2 : 3).
TEST(SohApHostRewrites, RuleConditionedCountTernaryDistributesThroughSmallKeysRewrite) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define small_keys(sc: Scene, count: Int) -> Bool\n"
		"define test():\n"
		"    small_keys(SCENE_SPIRIT_TEMPLE, (has(RG_CLIMB) or can_use(RG_HOOKSHOT)) and has(RG_POWER_BRACELET) ? 2 : 3)\n",
		"test")),
		"rls_conditional(bundle, "
		"(has_item(bundle, Items.RG_CLIMB) | can_use(bundle, Items.RG_HOOKSHOT)) & has_item(bundle, Items.RG_POWER_BRACELET), "
		"small_keys(bundle, Items.RG_SPIRIT_TEMPLE_SMALL_KEY, 2), "
		"small_keys(bundle, Items.RG_SPIRIT_TEMPLE_SMALL_KEY, 3))");
}

// Distribution is scoped to RULE conditions. A build-time condition (a Bool parameter) stays an
// ordinary Python `if` selecting the enum value in place -- no conditional() rule, no call
// duplication.
TEST(SohApHostRewrites, BuildTimeConditionedItemArgTernaryStaysPythonIf) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(flag: Bool):\n"
		"    can_use(flag ? RG_HOOKSHOT : RG_LONGSHOT)\n",
		"test")),
		"can_use(bundle, Items.RG_HOOKSHOT if flag else Items.RG_LONGSHOT)");
}

// stone_count() == 3 lowers to has_enough_stones(bundle, 3): spiritual stones cap at 3, so SoH's
// `StoneCount() == 3` ("all stones") is the `>= 3` cap check. `==` is allowed only for this entry.
TEST(SohApHostRewrites, StoneCountEqualsCapLowersToHasEnoughStones) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define stone_count() -> Int\n"
		"define test():\n"
		"    stone_count() == 3 and has(RG_OCARINA_OF_TIME)\n",
		"test")),
		"has_enough_stones(bundle, 3) & has_item(bundle, Items.RG_OCARINA_OF_TIME)");
}

// get_gs_count() >= N reuses the generic has_item count check keyed on the GS token item.
TEST(SohApHostRewrites, GsCountThresholdLowersToHasItemCount) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define get_gs_count() -> Int\n"
		"define test():\n"
		"    get_gs_count() >= 50\n",
		"test")),
		"has_item(bundle, Items.RG_GOLD_SKULLTULA_TOKEN, 50)");
}

// A threshold does not have to be a literal. A define's parameters are BuildTime (bound at the
// call that builds the rule), so an arithmetic expression over them threads into the helper --
// this is SoH's CanKillEnemy(RE_SHABOM), whose threshold scales with the enemy quantity.
TEST(SohApHostRewrites, BuildTimeThresholdThreadsIntoHelper) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define effective_health() -> Int\n"
		"define test(quantity: Int):\n"
		"    effective_health() >= quantity / 2 + 1 or has(RG_HOOKSHOT)\n",
		"test")),
		"effective_health_at_least(bundle, quantity // 2 + 1) | has_item(bundle, Items.RG_HOOKSHOT)");
}

// `> <build-time expr>` normalizes to `>= expr + 1` in the emitted Python, parenthesized so the
// `+ 1` applies to the whole threshold rather than its last term.
TEST(SohApHostRewrites, BuildTimeThresholdGreaterThanAddsOne) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define hearts() -> Int\n"
		"define test(quantity: Int):\n"
		"    hearts() > quantity * 2\n",
		"test")),
		"hearts_at_least(bundle, (quantity * 2) + 1)");
}

// A Runtime right operand (a count that moves with collection state) is still not a threshold:
// freezing it at build time would miscompile, so it stays a raw comparison and is diagnosed.
TEST(SohApHostRewrites, RuntimeThresholdIsNotRewritten) {
	auto resolved = sourceToExpression(
		"extern define effective_health() -> Int\n"
		"extern define bottle_count() -> Int\n"
		"define test():\n"
		"    effective_health() >= bottle_count() and has(RG_HOOKSHOT)\n",
		"test");
	rls::transpilers::soh_ap::SohApTranspiler transpiler(resolved.project);
	transpiler.GenerateExpression(resolved.expr);
	EXPECT_FALSE(transpiler.Diagnostics().empty());
}

// A non-cap `==` (hearts() == 3 means exactly 3, not >= 3) is NOT a threshold rewrite: hearts has
// no allowEq, so it stays a raw runtime comparison and is rejected when combined with a rule.
TEST(SohApHostRewrites, ExactEqualityCountIsNotRewritten) {
	auto resolved = sourceToExpression(
		"extern define hearts() -> Int\n"
		"define test():\n"
		"    hearts() == 3 and has(RG_HOOKSHOT)\n",
		"test");
	rls::transpilers::soh_ap::SohApTranspiler transpiler(resolved.project);
	transpiler.GenerateExpression(resolved.expr);
	EXPECT_FALSE(transpiler.Diagnostics().empty());
}

// small_keys(SCENE_X, N) maps to the world's small_keys(bundle, key, N): the scene becomes the
// dungeon's small-key item and the bundle receiver is the first argument.
TEST(SohApHostRewrites, SmallKeysMapsSceneToKeyItemBundleFirst) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define small_keys(sc: Scene, count: Int) -> Bool\n"
		"define test():\n"
		"    small_keys(SCENE_SHADOW_TEMPLE, 2)\n",
		"test")),
		"small_keys(bundle, Items.RG_SHADOW_TEMPLE_SMALL_KEY, 2)");
}

// The scene->key names diverge for Thieves' Hideout (it uses the Gerudo Fortress key). The
// lowered call is a Rule, so it composes with other rules under and/or.
TEST(SohApHostRewrites, SmallKeysThievesHideoutUsesGerudoFortressKey) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define small_keys(sc: Scene, count: Int) -> Bool\n"
		"define test():\n"
		"    small_keys(SCENE_THIEVES_HIDEOUT, 4) and can_use(RG_LONGSHOT)\n",
		"test")),
		"small_keys(bundle, Items.RG_GERUDO_FORTRESS_SMALL_KEY, 4) & can_use(bundle, Items.RG_LONGSHOT)");
}

// A no-argument Bool host query like take_damage needs no rewrite: the default call form
// prepends the bundle receiver, yielding take_damage(bundle) that composes as a Rule.
TEST(SohApHostRewrites, TakeDamageRendersBundleFirst) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define take_damage() -> Bool\n"
		"define test():\n"
		"    take_damage() or has(RG_BOTTLE_WITH_FAIRY)\n",
		"test")),
		"take_damage(bundle) | has_item(bundle, Items.RG_BOTTLE_WITH_FAIRY)");
}

// A collapsed special-case call is atomic, so it composes with surrounding rules without
// extra parentheses (GetPythonPrecedence treats the rewrite as a tightly-bound call).
TEST(SohApHostRewrites, CollapsedSpecialCaseNeedsNoParensUnderAnd) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define check_price(check: Location) -> Int\n"
		"extern define wallet_capacity() -> Int\n"
		"define test():\n"
		"    has(RG_HOOKSHOT) and check_price(RC_FOO) <= wallet_capacity()\n",
		"test")),
		"has_item(bundle, Items.RG_HOOKSHOT) & can_afford_slot(Locations.RC_FOO)");
}

// A rule-conditioned ternary `is_child() ? a : b` cannot be a Python `if`, so both branches go
// to the host conditional rule, which picks one at solve time. No complement (is_adult) is
// synthesized and the else-branch stays gated by the condition being false. This verifies the
// generic lowering (ternary_tests.cpp) threads the bundle receiver and enum prefixes through
// the SoH hooks.
TEST(SohApHostRewrites, AgeConditionalTernaryLowersToConditionalRule) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define is_child() -> Bool\n"
		"define test():\n"
		"    is_child() ? has(RG_HOOKSHOT) : has(RG_BOOMERANG)\n",
		"test")),
		"rls_conditional(bundle, is_child(bundle), has_item(bundle, Items.RG_HOOKSHOT), "
		"has_item(bundle, Items.RG_BOOMERANG))");
}

// A compound else branch is a single argument to the conditional rule, so it needs no
// parenthesization and stays reachable only when the condition is false.
TEST(SohApHostRewrites, AgeConditionalKeepsElseOrBranchGated) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define is_child() -> Bool\n"
		"define test():\n"
		"    is_child() ? has(RG_HOOKSHOT) : (has(RG_BOOMERANG) or has(RG_FAIRY_BOW))\n",
		"test")),
		"rls_conditional(bundle, is_child(bundle), has_item(bundle, Items.RG_HOOKSHOT), "
		"has_item(bundle, Items.RG_BOOMERANG) | has_item(bundle, Items.RG_FAIRY_BOW))");
}

// A fallthrough rule match renders the SoH host-call rewrites and enum prefixes inside the
// arm lambdas, and threads the bundle receiver -- the helper |-combines matched arms.
TEST(SohApHostRewrites, RuleMatchThreadsBundleAndEnumPrefixes) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE: can_use(RG_MEGATON_HAMMER) or\n"
		"        ED_HOOKSHOT: can_use(RG_HOOKSHOT)\n"
		"    }\n",
		"test")),
		"rls_match_rule((lambda d=d: d == EnemyDistance.ED_CLOSE), "
		"(lambda: can_use(bundle, Items.RG_MEGATON_HAMMER)), True, "
		"(lambda d=d: d == EnemyDistance.ED_HOOKSHOT), "
		"(lambda: can_use(bundle, Items.RG_HOOKSHOT)), False)");
}

// check_price(RC_UNKNOWN_CHECK) uses the location set on the transpiler during region
// generation to emit can_afford_slot(Locations.<current location>).
TEST(SohApHostRewrites, CheckPriceUnknownUsesCurrentLocation) {
	auto resolved = sourceToExpression(
		"extern define check_price(check: Location) -> Int\n"
		"extern define wallet_capacity() -> Int\n"
		"define test():\n"
		"    check_price(RC_UNKNOWN_CHECK) <= wallet_capacity()\n",
		"test");
	rls::transpilers::soh_ap::SohApTranspiler transpiler(resolved.project);
	transpiler.SetCurrentLocation("RC_KF_SHOP_ITEM_1");
	EXPECT_EQ(transpiler.GenerateExpression(resolved.expr),
		"can_afford_slot(Locations.RC_KF_SHOP_ITEM_1)");
}
