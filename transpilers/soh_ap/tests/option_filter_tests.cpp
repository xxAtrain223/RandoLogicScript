// SoH-specific rendering tests, exercised through expression generation. The generic
// OptionFilter mechanics (wrapping, "ne", bare/negated truthiness, composition, parens)
// are owned by the base ApTranspiler and tested under transpilers/ap. These tests cover
// only what the SoH hooks add on top: enum-class prefixes, the generic yes/no value
// mapping, and host-call rewrites.
#include "helpers.h"

using namespace rls::transpilers::soh_ap_tests;

namespace {
struct ResolvedExpression {
	rls::ast::Project project;
	rls::ast::ExprPtr expr;
};
} // namespace

static std::string GenerateExpression(const ResolvedExpression& resolved) {
	return rls::transpilers::soh_ap::SohApTranspiler(resolved.project).GenerateExpression(resolved.expr);
}

// Resolve a define from inline RLS source and hand back its body expression.
static ResolvedExpression sourceToExpression(const std::string& source, const std::string& defineName) {
	auto project = resolveFromSource(source);
	auto defineDecl = project.DefineDecls.find(defineName);
	if (defineDecl == project.DefineDecls.end()) {
		return { std::move(project), nullptr };
	}

	return {
		std::move(project),
		std::move(const_cast<rls::ast::DefineDecl*>(defineDecl->second)->body)
	};
}

// A setting value identifier is rendered with the SoH RandomizerSettingKey enum prefix.
TEST(SohApRendering, SettingValueGetsEnumPrefix) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_FOO) is RO_BAR\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, RandomizerSettingKey.RO_BAR)])");
}

// `is RO_GENERIC_YES` maps to the bare Python True via the SoH enum-value override.
TEST(SohApRendering, GenericYesMapsToTrue) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_FOO) is RO_GENERIC_YES\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, True)])");
}

// `is RO_GENERIC_NO` maps to the bare Python False.
TEST(SohApRendering, GenericNoMapsToFalse) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_FOO) is RO_GENERIC_NO\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, False)])");
}

// End-to-end SoH rendering across a complex rule: prefixes, host calls, yes-mapping and
// the base precedence/parenthesization all compose.
TEST(SohApRendering, ComplexSettingParens) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) or setting(RSK_FOO) is RO_BAR and (setting(RSK_BAR) is RO_FOO or can_kill(RE_GOLD_SKULLTULA)) and (flag(LOGIC_BAZ) or setting(RSK_BAZ) is RO_GENERIC_YES)\n",
		"test")),
		"has_item(bundle, Items.RG_HOOKSHOT) | "
		"True_(options=[OptionFilter(RSK_FOO, RandomizerSettingKey.RO_BAR)]) & "
		"(True_(options=[OptionFilter(RSK_BAR, RandomizerSettingKey.RO_FOO)]) | can_kill(bundle, Enemies.RE_GOLD_SKULLTULA)) & "
		"(has_item(bundle, Events.LOGIC_BAZ) | True_(options=[OptionFilter(RSK_BAZ, True)]))");
}
