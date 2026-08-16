#include <algorithm>
#include <stop_token>

#include <gtest/gtest.h>

#include "ast.h"
#include "parser.h"
#include "sema.h"

// Internal header for direct unit testing of individual passes.
#include "collect_declarations.h"

using namespace rls::ast;
using namespace rls::sema;

// == Helpers ==================================================================

static RegionBody makeRegionBody(const std::string& scene) {
	std::vector<RegionDataEntry> data;
	data.emplace_back(Name("name"), makeExpr(StringLiteral{"Test"}));
	data.emplace_back(Name("scene"), makeExpr(Identifier{Name(scene)}));
	return RegionBody(std::move(data), {});
}

static std::string_view sceneName(const RegionDecl& region) {
	const auto* scene = region.body.findData("scene");
	return std::get<Identifier>(scene->value->node).name.view();
}

/// Build a minimal File containing a single RegionDecl.
static File makeRegionFile(const std::string& path, const std::string& regionName,
                           const std::string& scene, Span span = {}) {
	File f;
	f.path = path;
	f.declarations.emplace_back(RegionDecl(
		Name(regionName),
		makeRegionBody(scene),
		span
	));
	return f;
}

/// Build a minimal File containing a single DefineDecl.
static File makeDefineFile(const std::string& path, const std::string& name,
                           Span span = {}) {
	File f;
	f.path = path;
	f.declarations.emplace_back(DefineDecl(
		Name(name), {}, makeExpr(BoolLiteral{true}), span
	));
	return f;
}

/// Build a minimal File containing a single ExternDefineDecl.
static File makeExternDefineFile(const std::string& path, const std::string& name,
                                 Span span = {}) {
	File f;
	f.path = path;
	f.declarations.emplace_back(ExternDefineDecl(Name(name), {}, span));
	return f;
}

/// Build a minimal File containing a single ExtendRegionDecl.
static File makeExtendFile(const std::string& path, const std::string& regionName,
                           SectionKind sectionKind, Span span = {}) {
	File f;
	f.path = path;
	std::vector<Entry> entries;
	entries.emplace_back(Name("TEST_ENTRY"), makeExpr(BoolLiteral{true}));
	std::vector<Section> sections;
	sections.emplace_back(sectionKind, std::move(entries));
	f.declarations.emplace_back(ExtendRegionDecl(
		Name(regionName), std::move(sections), span
	));
	return f;
}

/// Build a minimal File containing a single EnumDecl.
static File makeEnumFile(const std::string& path, const std::string& enumName,
	std::vector<EnumMemberDecl> members = {}, Span span = {}) {
	File f;
	f.path = path;
	f.declarations.emplace_back(EnumDecl(Name(enumName), std::move(members), span));
	return f;
}

/// Build a minimal File containing a single ExternEnumDecl.
static File makeExternEnumFile(const std::string& path, const std::string& enumName,
	std::vector<ExternEnumEntryDecl> entries = {}, Span span = {}) {
	File f;
	f.path = path;
	f.declarations.emplace_back(ExternEnumDecl(Name(enumName), std::move(entries), span));
	return f;
}

/// Count diagnostics of a given level.
static size_t countErrors(const std::vector<Diagnostic>& diags) {
	size_t n = 0;
	for (const auto& d : diags)
		if (d.level == DiagnosticLevel::Error) ++n;
	return n;
}

static size_t countWarnings(const std::vector<Diagnostic>& diags) {
	size_t n = 0;
	for (const auto& d : diags)
		if (d.level == DiagnosticLevel::Warning) ++n;
	return n;
}

// == Semantic index ===========================================================

TEST(AnalysisSnapshotTests, OwnsExplicitSourcesAndDerivedIndexes) {
	const auto snapshot = AnalysisSnapshot::Create({
		{"overlay.rls", "define check(): true\ndefine run(): check()\n"},
	}, 42);
	ASSERT_TRUE(snapshot);
	EXPECT_EQ((*snapshot)->generation(), 42u);
	ASSERT_EQ((*snapshot)->documentCount(), 1u);
	const auto* sourceText = (*snapshot)->sourceText("overlay.rls");
	ASSERT_NE(sourceText, nullptr);
	EXPECT_EQ(sourceText->content(), "define check(): true\ndefine run(): check()\n");
	const auto* sourceIndex = (*snapshot)->sourceIndex("overlay.rls");
	ASSERT_NE(sourceIndex, nullptr);
	EXPECT_TRUE(sourceIndex->nameAt({1, 8}));
	EXPECT_TRUE((*snapshot)->syntaxAt("overlay.rls", {1, 8}));
	EXPECT_TRUE((*snapshot)->nameAt("overlay.rls", {1, 8}));
	const auto symbol = (*snapshot)->symbolAt("overlay.rls", {1, 8});
	ASSERT_TRUE(symbol);
	EXPECT_TRUE((*snapshot)->declaration(*symbol));
	EXPECT_FALSE((*snapshot)->references(*symbol).empty());
	EXPECT_FALSE((*snapshot)->visibleSymbolsAt("overlay.rls", {2, 15}).empty());
	const auto type = (*snapshot)->typeAt("overlay.rls", {1, 17});
	ASSERT_TRUE(type);
	EXPECT_EQ(type->type, Type::Bool);
	EXPECT_FALSE((*snapshot)->expectedTypeAt("overlay.rls", {1, 17}));
	const auto call = (*snapshot)->callAt("overlay.rls", {2, 15});
	ASSERT_TRUE(call);
	EXPECT_TRUE(call->target);
	EXPECT_FALSE((*snapshot)->diagnosticsFor("overlay.rls").empty());
	EXPECT_TRUE((*snapshot)->diagnosticsFor("other.rls").empty());

	EXPECT_FALSE(AnalysisSnapshot::Create(
		std::vector<SourceInput>{{"bad.rls", std::string("\xC3\x28", 2)}}, 43));

	const auto overlay = AnalysisSnapshot::Create({
		{"overlay.rls", "define check(): true\n"},
		{"overlay.rls", "define check(): false\n"},
	}, 44);
	ASSERT_TRUE(overlay);
	EXPECT_EQ((*overlay)->documentCount(), 1u);
	EXPECT_EQ((*overlay)->sourceText("overlay.rls")->content(), "define check(): false\n");
}

TEST(AnalysisSnapshotTests, HonorsCancellationBeforeWorkStarts) {
	std::stop_source cancellation;
	cancellation.request_stop();

	EXPECT_FALSE(AnalysisSnapshot::Create({
		{"cancelled.rls", "define cancelled(): true\n"},
	}, 45, cancellation.get_token()));
}

TEST(AnalysisSnapshotTests, IsolatesParseFailuresAcrossExplicitSources) {
	const auto first = AnalysisSnapshot::Create({
		{"broken.rls", "define broken(\n"},
		{"valid.rls", "define valid(): true\n"},
	}, 100);
	ASSERT_TRUE(first);
	ASSERT_EQ((*first)->documentCount(), 2u);
	EXPECT_FALSE((*first)->diagnosticsFor("broken.rls").empty());
	const auto* validIndex = (*first)->sourceIndex("valid.rls");
	ASSERT_NE(validIndex, nullptr);
	EXPECT_TRUE(validIndex->nameAt({1, 8}));
	EXPECT_TRUE(std::any_of((*first)->semanticIndex().symbols().begin(),
		(*first)->semanticIndex().symbols().end(), [](const SymbolRecord& symbol) {
			return symbol.category == SymbolCategory::Define && symbol.displayName == "valid";
		}));

	const auto second = AnalysisSnapshot::Create({
		{"valid.rls", "define valid(): false\n"},
	}, 101);
	ASSERT_TRUE(second);
	EXPECT_EQ((*first)->generation(), 100u);
	EXPECT_EQ((*second)->generation(), 101u);
	EXPECT_EQ((*first)->sourceText("valid.rls")->content(), "define valid(): true\n");
	EXPECT_EQ((*second)->sourceText("valid.rls")->content(), "define valid(): false\n");
}

TEST(AnalysisSnapshotTests, AnalyzesCompleteNeighborsInMalformedDocument) {
	const auto snapshot = AnalysisSnapshot::Create({{
		"partial.rls",
		"define before(): true\n"
		"define broken(\n"
		"define after(): before()\n",
	}}, 102);
	ASSERT_TRUE(snapshot);
	EXPECT_FALSE((*snapshot)->diagnosticsFor("partial.rls").empty());

	const auto& symbols = (*snapshot)->semanticIndex().symbols();
	const auto hasDefine = [&](std::string_view name) {
		return std::any_of(symbols.begin(), symbols.end(), [&](const SymbolRecord& symbol) {
			return symbol.category == SymbolCategory::Define
				&& symbol.displayName == name;
		});
	};
	EXPECT_TRUE(hasDefine("before"));
	EXPECT_TRUE(hasDefine("after"));
	EXPECT_FALSE(hasDefine("broken"));

	const auto before = (*snapshot)->symbolAt("partial.rls", {1, 8});
	const auto reference = (*snapshot)->symbolAt("partial.rls", {3, 18});
	ASSERT_TRUE(before);
	ASSERT_TRUE(reference);
	EXPECT_EQ(*reference, *before);
	EXPECT_FALSE((*snapshot)->symbolAt("partial.rls", {2, 8}));
}

TEST(AnalysisSnapshotTests, ExposesStructuredValidationDiagnostics) {
	const auto snapshot = AnalysisSnapshot::Create({
		{"validation.rls", "region RR_TEST { events { EVENT_TEST: \"invalid\" } }\n"},
	}, 102);
	ASSERT_TRUE(snapshot);
	const auto diagnostics = (*snapshot)->diagnosticsFor("validation.rls");
	const auto diagnostic = std::find_if(diagnostics.begin(), diagnostics.end(), [](const CompilerDiagnostic& candidate) {
			return candidate.code == "RLS-V004";
		});
	ASSERT_NE(diagnostic, diagnostics.end());
	EXPECT_EQ(diagnostic->level, DiagnosticLevel::Error);
	EXPECT_EQ(diagnostic->span.file, "validation.rls");
	EXPECT_NE(diagnostic->message.find("must be Bool"), std::string::npos);
}

TEST(AnalysisSnapshotTests, PreservesStructuredDiagnosticActionData) {
	const auto snapshot = AnalysisSnapshot::Create({
		{"actions.rls", "define broken(): missing\n"},
	}, 104);
	ASSERT_TRUE(snapshot);
	const auto diagnostics = (*snapshot)->diagnosticsFor("actions.rls");
	const auto diagnostic = std::find_if(diagnostics.begin(), diagnostics.end(),
		[](const CompilerDiagnostic& candidate) { return candidate.code == "RLS-T006"; });
	ASSERT_NE(diagnostic, diagnostics.end());
	ASSERT_TRUE(diagnostic->data.has_value());
	EXPECT_EQ(diagnostic->data->version, 1u);
	EXPECT_EQ(diagnostic->data->actionKind, "rls.declareSymbol");
	ASSERT_EQ(diagnostic->data->arguments.size(), 1u);
	EXPECT_EQ(diagnostic->data->arguments[0], "missing");
}

TEST(AnalysisSnapshotTests, RelatesDuplicateRegionDataToFirstDefinition) {
	const auto snapshot = AnalysisSnapshot::Create({
		{"duplicate-data.rls", "region RR_TEST { name: \"First\" name: \"Second\" }\n"},
	}, 103);
	ASSERT_TRUE(snapshot);
	const auto diagnostics = (*snapshot)->diagnosticsFor("duplicate-data.rls");
	const auto diagnostic = std::find_if(diagnostics.begin(), diagnostics.end(),
		[](const CompilerDiagnostic& candidate) { return candidate.code == "RLS-V002"; });
	ASSERT_NE(diagnostic, diagnostics.end());
	ASSERT_EQ(diagnostic->related.size(), 1u);
	EXPECT_EQ(diagnostic->related[0].message, "first definition");
	EXPECT_EQ(diagnostic->related[0].span.file, "duplicate-data.rls");
	EXPECT_LT(diagnostic->related[0].span.start.column, diagnostic->span.start.column);
}

TEST(SemanticIndexTests, RecordsStableValueOnlyDeclarationIdentity) {
	SemanticIndex index;
	{
		Project project;
		project.files.push_back(rls::parser::ParseString(
			"region RR_TEST { name: \"Test\" events { EVENT_TEST: true } }\n"
			"define check(value: Color): true\n"
			"extern define external(value: Color) -> Bool\n"
			"enum Color { RED }\n"
			"extern enum External { VALUE, EXT_* }\n",
			"semantic.rls"));
		analyze(project);
		index = buildSemanticIndex(project);
	}

	auto findSymbol = [&](SymbolCategory category, std::string_view displayName) {
		for (const auto& symbol : index.symbols()) {
			if (symbol.category == category && symbol.displayName == displayName) {
				return std::optional<SymbolRecord>(symbol);
			}
		}
		return std::optional<SymbolRecord>{};
	};

	ASSERT_EQ(index.symbols().size(), 12u);
	const auto region = findSymbol(SymbolCategory::Region, "RR_TEST");
	const auto define = findSymbol(SymbolCategory::Define, "check");
	const auto parameter = findSymbol(SymbolCategory::Parameter, "value");
	const auto enumType = findSymbol(SymbolCategory::Enum, "Color");
	const auto pattern = findSymbol(SymbolCategory::ExternEnumPattern, "EXT_*");
	ASSERT_TRUE(region);
	ASSERT_TRUE(define);
	ASSERT_TRUE(parameter);
	ASSERT_TRUE(enumType);
	ASSERT_TRUE(pattern);
	EXPECT_NE(region->id, define->id);
	EXPECT_EQ(parameter->container, define->id);
	EXPECT_EQ(define->signature, "define check");
	EXPECT_EQ(enumType->type, Type::Enum);
	EXPECT_EQ(enumType->enumName, "Color");
	EXPECT_EQ(pattern->provenance, SymbolProvenance::Pattern);
	EXPECT_EQ(pattern->declaration.file, "semantic.rls");

	const auto declaration = index.declaration(enumType->id);
	ASSERT_TRUE(declaration);
	EXPECT_EQ(declaration->displayName, "Color");
	const auto occurrences = index.occurrencesFor(enumType->id);
	ASSERT_EQ(occurrences.size(), 3u);
	EXPECT_EQ(occurrences[0].kind, OccurrenceKind::TypeReference);
	EXPECT_EQ(occurrences[1].kind, OccurrenceKind::TypeReference);
	EXPECT_EQ(occurrences[2].kind, OccurrenceKind::Declaration);
	EXPECT_EQ(occurrences[2].span.file, declaration->selection.file);
	EXPECT_EQ(occurrences[2].span.start.line, declaration->selection.start.line);
	EXPECT_EQ(occurrences[2].span.start.column, declaration->selection.start.column);
	EXPECT_EQ(occurrences[2].span.end.line, declaration->selection.end.line);
	EXPECT_EQ(occurrences[2].span.end.column, declaration->selection.end.column);
}

TEST(SemanticIndexTests, RecordsRegionExtensionTargetRelations) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"region RR_BASE { name: \"Base\" }\n"
		"extend region RR_BASE { events { EVENT_BASE: true } }\n"
		"extend region RR_UNKNOWN { events { EVENT_UNKNOWN: true } }\n",
		"extensions.rls"));
	analyze(project);
	const auto index = buildSemanticIndex(project);

	std::optional<SymbolRecord> base;
	std::vector<SymbolRecord> extensions;
	for (const auto& symbol : index.symbols()) {
		if (symbol.category == SymbolCategory::Region && symbol.displayName == "RR_BASE") base = symbol;
		if (symbol.category == SymbolCategory::RegionExtension) extensions.push_back(symbol);
	}
	ASSERT_TRUE(base);
	ASSERT_EQ(extensions.size(), 2u);
	EXPECT_EQ(extensions[0].container, base->id);
	EXPECT_FALSE(extensions[1].container);

	const auto validTarget = index.occurrenceAt("extensions.rls", {2, 15});
	ASSERT_TRUE(validTarget);
	EXPECT_EQ(validTarget->kind, OccurrenceKind::ExtensionTarget);
	EXPECT_EQ(validTarget->symbol, base->id);
	const auto unknownTarget = index.occurrenceAt("extensions.rls", {3, 15});
	ASSERT_TRUE(unknownTarget);
	EXPECT_EQ(unknownTarget->kind, OccurrenceKind::Unresolved);
	EXPECT_FALSE(unknownTarget->symbol);
}

TEST(SemanticIndexTests, RecordsDuplicateDeclarationDiagnostics) {
	SemanticIndex index;
	{
		Project project;
		project.files.push_back(rls::parser::ParseString(
			"region RR_DUP { name: \"First\" }\n", "first.rls"));
		project.files.push_back(rls::parser::ParseString(
			"region RR_DUP { name: \"Second\" }\n", "second.rls"));
		analyze(project);
		index = buildSemanticIndex(project);
	}

	ASSERT_EQ(index.diagnostics().size(), 1u);
	const auto& diagnostic = index.diagnostics()[0];
	EXPECT_EQ(diagnostic.code, "RLS-S001");
	EXPECT_EQ(diagnostic.level, DiagnosticLevel::Error);
	EXPECT_EQ(diagnostic.message, "duplicate region 'RR_DUP'");
	EXPECT_EQ(diagnostic.span.file, "second.rls");
	ASSERT_EQ(diagnostic.related.size(), 1u);
	EXPECT_EQ(diagnostic.related[0].message, "first declaration");
	EXPECT_EQ(diagnostic.related[0].span.file, "first.rls");
}

TEST(SemanticIndexTests, SeparatesParameterScopesAndKeepsUnknownOccurrences) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"define first(value: Bool): value\n"
		"define second(value: Bool): value and unknown\n",
		"scopes.rls"));
	analyze(project);
	const auto index = buildSemanticIndex(project);

	std::optional<SymbolId> first;
	std::optional<SymbolId> second;
	std::vector<SymbolRecord> parameters;
	for (const auto& symbol : index.symbols()) {
		if (symbol.category == SymbolCategory::Define && symbol.displayName == "first") first = symbol.id;
		if (symbol.category == SymbolCategory::Define && symbol.displayName == "second") second = symbol.id;
		if (symbol.category == SymbolCategory::Parameter && symbol.displayName == "value") parameters.push_back(symbol);
	}
	ASSERT_TRUE(first);
	ASSERT_TRUE(second);
	ASSERT_EQ(parameters.size(), 2u);
	const auto firstParameter = parameters[0].container == first ? parameters[0] : parameters[1];
	const auto secondParameter = parameters[0].container == second ? parameters[0] : parameters[1];
	EXPECT_EQ(firstParameter.container, first);
	EXPECT_EQ(secondParameter.container, second);

	const auto firstOccurrences = index.occurrencesFor(firstParameter.id);
	const auto secondOccurrences = index.occurrencesFor(secondParameter.id);
	ASSERT_EQ(firstOccurrences.size(), 2u);
	ASSERT_EQ(secondOccurrences.size(), 2u);
	EXPECT_EQ(firstOccurrences[1].kind, OccurrenceKind::Reference);
	EXPECT_EQ(secondOccurrences[1].kind, OccurrenceKind::Reference);
	const auto firstUse = index.occurrenceAt("scopes.rls", {1, 28});
	ASSERT_TRUE(firstUse);
	EXPECT_EQ(firstUse->symbol, firstParameter.id);
	const auto secondUse = index.occurrenceAt("scopes.rls", {2, 29});
	ASSERT_TRUE(secondUse);
	EXPECT_EQ(secondUse->symbol, secondParameter.id);
	const auto unknown = index.occurrenceAt("scopes.rls", {2, 39});
	ASSERT_TRUE(unknown);
	EXPECT_EQ(unknown->kind, OccurrenceKind::Unresolved);
	EXPECT_FALSE(unknown->symbol);
}

TEST(SemanticIndexTests, CapturesCrossFileExternsAndAmbiguousEnumValues) {
	Project project;
	project.files.push_back(rls::parser::ParseString("extern define host() -> Bool\n", "host.rls"));
	project.files.push_back(rls::parser::ParseString("enum Alpha { SHARED }\n", "alpha.rls"));
	project.files.push_back(rls::parser::ParseString("enum Beta { SHARED }\n", "beta.rls"));
	project.files.push_back(rls::parser::ParseString(
		"define call(): host()\n"
		"define ambiguous(): SHARED\n", "use.rls"));
	analyze(project);
	const auto index = buildSemanticIndex(project);

	std::optional<SymbolRecord> host;
	for (const auto& symbol : index.symbols()) {
		if (symbol.category == SymbolCategory::ExternDefine && symbol.displayName == "host") host = symbol;
	}
	ASSERT_TRUE(host);
	EXPECT_EQ(host->provenance, SymbolProvenance::Extern);
	const auto hostCall = index.occurrenceAt("use.rls", {1, 16});
	ASSERT_TRUE(hostCall);
	EXPECT_EQ(hostCall->kind, OccurrenceKind::Call);
	EXPECT_EQ(hostCall->symbol, host->id);
	const auto ambiguous = index.occurrenceAt("use.rls", {2, 21});
	ASSERT_TRUE(ambiguous);
	EXPECT_EQ(ambiguous->kind, OccurrenceKind::Unresolved);
	EXPECT_FALSE(ambiguous->symbol);
}

TEST(SemanticIndexTests, RecordsConcreteValuesObservedThroughExternPatterns) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extern enum Item { RG_EXPLICIT, RG_* }\n"
		"define first(): RG_HOOKSHOT\n"
		"define repeated(): RG_HOOKSHOT\n"
		"define qualified(): Item.RG_BOW\n"
		"define explicit(): RG_EXPLICIT\n",
		"observed-enum-values.rls"));
	analyze(project);
	const auto index = buildSemanticIndex(project);

	ASSERT_EQ(index.observedEnumValues().size(), 2u);
	EXPECT_EQ(index.observedEnumValues()[0].enumName, "Item");
	EXPECT_EQ(index.observedEnumValues()[0].displayName, "RG_BOW");
	EXPECT_EQ(index.observedEnumValues()[1].enumName, "Item");
	EXPECT_EQ(index.observedEnumValues()[1].displayName, "RG_HOOKSHOT");
}

TEST(SemanticIndexTests, RecordsOperatorAndTernaryExpectedTypes) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"define check(flag: Bool, count: Int): flag ? count + 1 : count\n",
		"expected.rls"));
	analyze(project);
	const auto index = buildSemanticIndex(project);

	const auto condition = index.expectedTypeAt("expected.rls", {1, 39});
	ASSERT_TRUE(condition);
	EXPECT_EQ(condition->type, Type::Bool);
	const auto arithmeticParameter = index.expectedTypeAt("expected.rls", {1, 46});
	ASSERT_TRUE(arithmeticParameter);
	EXPECT_EQ(arithmeticParameter->type, Type::Int);
	const auto arithmeticLiteral = index.expectedTypeAt("expected.rls", {1, 54});
	ASSERT_TRUE(arithmeticLiteral);
	EXPECT_EQ(arithmeticLiteral->type, Type::Int);
}

TEST(SemanticIndexTests, CopiesResolvedTypesCallsAndMemberOccurrences) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"enum Color { RED }\n"
		"define identity(value: Color): value\n"
		"define check(): identity(RED)\n",
		"calls.rls"));
	analyze(project);
	const auto index = buildSemanticIndex(project);

	auto findSymbol = [&](SymbolCategory category, std::string_view displayName) {
		for (const auto& symbol : index.symbols()) {
			if (symbol.category == category && symbol.displayName == displayName) {
				return std::optional<SymbolRecord>(symbol);
			}
		}
		return std::optional<SymbolRecord>{};
	};

	const auto identity = findSymbol(SymbolCategory::Define, "identity");
	const auto value = findSymbol(SymbolCategory::Parameter, "value");
	const auto color = findSymbol(SymbolCategory::Enum, "Color");
	const auto red = findSymbol(SymbolCategory::EnumMember, "RED");
	ASSERT_TRUE(identity);
	ASSERT_TRUE(value);
	ASSERT_TRUE(color);
	ASSERT_TRUE(red);
	ASSERT_EQ(index.calls().size(), 1u);
	const auto& call = index.calls()[0];
	EXPECT_EQ(call.target, identity->id);
	ASSERT_EQ(call.argumentRanges.size(), 1u);
	ASSERT_EQ(call.normalizedBindings.size(), 1u);
	EXPECT_EQ(call.normalizedBindings[0], 0u);
	const auto callAt = index.callAt("calls.rls", {3, 17});
	ASSERT_TRUE(callAt);
	EXPECT_EQ(callAt->target, identity->id);
	const auto occurrenceAt = index.occurrenceAt("calls.rls", {3, 17});
	ASSERT_TRUE(occurrenceAt);
	EXPECT_EQ(occurrenceAt->symbol, identity->id);
	EXPECT_EQ(occurrenceAt->kind, OccurrenceKind::Call);
	const auto typeAt = index.typeAt("calls.rls", {3, 26});
	ASSERT_TRUE(typeAt);
	EXPECT_EQ(typeAt->type, Type::Enum);
	EXPECT_EQ(typeAt->enumName, "Color");
	const auto expectedTypeAt = index.expectedTypeAt("calls.rls", {3, 26});
	ASSERT_TRUE(expectedTypeAt);
	EXPECT_EQ(expectedTypeAt->type, Type::Enum);
	EXPECT_EQ(expectedTypeAt->enumName, "Color");
	const auto typeReference = index.occurrenceAt("calls.rls", {2, 24});
	ASSERT_TRUE(typeReference);
	EXPECT_EQ(typeReference->kind, OccurrenceKind::TypeReference);
	EXPECT_EQ(typeReference->symbol, color->id);

	ASSERT_FALSE(index.types().empty());
	EXPECT_TRUE(std::any_of(index.types().begin(), index.types().end(), [](const TypeRecord& record) {
		return record.type == Type::Enum && record.enumName == "Color";
	}));
	const auto memberOccurrences = index.occurrencesFor(red->id);
	ASSERT_EQ(memberOccurrences.size(), 2u);
	EXPECT_EQ(memberOccurrences[0].kind, OccurrenceKind::Declaration);
	EXPECT_EQ(memberOccurrences[1].kind, OccurrenceKind::Reference);
	EXPECT_LT(memberOccurrences[0].span.start.line, memberOccurrences[1].span.start.line);

	const auto visibleInIdentity = index.visibleSymbolsAt("calls.rls", {2, 17});
	EXPECT_TRUE(std::find(visibleInIdentity.begin(), visibleInIdentity.end(), identity->id) != visibleInIdentity.end());
	EXPECT_TRUE(std::find(visibleInIdentity.begin(), visibleInIdentity.end(), value->id) != visibleInIdentity.end());
	const auto visibleInCheck = index.visibleSymbolsAt("calls.rls", {3, 17});
	EXPECT_TRUE(std::find(visibleInCheck.begin(), visibleInCheck.end(), identity->id) != visibleInCheck.end());
	EXPECT_TRUE(std::find(visibleInCheck.begin(), visibleInCheck.end(), value->id) == visibleInCheck.end());
}

TEST(SemanticIndexTests, TypesAndLinksDeclaredDomainValues) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"region RR_TARGET {\n"
		"  events { EVENT_OPEN: true }\n"
		"  locations { RC_CHEST: true }\n"
		"}\n"
		"define region_value(): RR_TARGET\n"
		"define event_value(): EVENT_OPEN\n"
		"define location_value(): RC_CHEST\n",
		"domain-values.rls"));
	analyze(project);
	const auto index = buildSemanticIndex(project);

	const auto find = [&](SymbolCategory category, std::string_view name) {
		return std::find_if(index.symbols().begin(), index.symbols().end(),
			[&](const SymbolRecord& symbol) {
				return symbol.category == category && symbol.displayName == name;
			});
	};
	const auto region = find(SymbolCategory::Region, "RR_TARGET");
	const auto event = find(SymbolCategory::SectionEntry, "EVENT_OPEN");
	const auto location = find(SymbolCategory::SectionEntry, "RC_CHEST");
	ASSERT_NE(region, index.symbols().end());
	ASSERT_NE(event, index.symbols().end());
	ASSERT_NE(location, index.symbols().end());
	EXPECT_EQ(region->type, Type::Region);
	EXPECT_EQ(event->type, Type::Event);
	EXPECT_EQ(location->type, Type::Location);

	const auto regionUse = index.occurrenceAt("domain-values.rls", {5, 25});
	const auto eventUse = index.occurrenceAt("domain-values.rls", {6, 24});
	const auto locationUse = index.occurrenceAt("domain-values.rls", {7, 27});
	ASSERT_TRUE(regionUse && eventUse && locationUse);
	EXPECT_EQ(regionUse->symbol, region->id);
	EXPECT_EQ(eventUse->symbol, event->id);
	EXPECT_EQ(locationUse->symbol, location->id);

	const auto visible = index.visibleSymbolsAt("domain-values.rls", {5, 25});
	EXPECT_NE(std::find(visible.begin(), visible.end(), region->id), visible.end());
	EXPECT_NE(std::find(visible.begin(), visible.end(), event->id), visible.end());
	EXPECT_NE(std::find(visible.begin(), visible.end(), location->id), visible.end());
}

// == Empty project ============================================================

TEST(CollectDeclarations, EmptyProject) {
	Project project;
	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_TRUE(project.RegionDecls.empty());
	EXPECT_TRUE(project.ExtendRegionDecls.empty());
	EXPECT_TRUE(project.DefineDecls.empty());
	EXPECT_TRUE(project.ExternDefineDecls.empty());
	EXPECT_TRUE(project.EnumInfos.empty());
}

TEST(CollectDeclarations, EmptyFile) {
	Project project;
	File f;
	f.path = "empty.rls";
	project.files.push_back(std::move(f));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_TRUE(project.RegionDecls.empty());
}

// == Single declarations ======================================================

TEST(CollectDeclarations, SingleRegion) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_TEST", "SCENE_TEST"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.RegionDecls.size(), 1u);
	ASSERT_TRUE(project.RegionDecls.contains("RR_TEST"));
	EXPECT_EQ(project.RegionDecls.at("RR_TEST")->key, "RR_TEST");
}

TEST(CollectDeclarations, SingleDefine) {
	Project project;
	project.files.push_back(makeDefineFile("a.rls", "has_explosives"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.DefineDecls.size(), 1u);
	ASSERT_TRUE(project.DefineDecls.contains("has_explosives"));
	EXPECT_EQ(project.DefineDecls.at("has_explosives")->name, "has_explosives");
}

TEST(CollectDeclarations, SingleExternDefine) {
	Project project;
	project.files.push_back(makeExternDefineFile("a.rls", "can_hit_switch"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.ExternDefineDecls.size(), 1u);
	ASSERT_TRUE(project.ExternDefineDecls.contains("can_hit_switch"));
	EXPECT_EQ(project.ExternDefineDecls.at("can_hit_switch")->name, "can_hit_switch");
}

TEST(CollectDeclarations, SingleExtendRegion) {
	Project project;
	project.files.push_back(makeExtendFile("a.rls", "RR_TEST", SectionKind::Locations));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_TRUE(project.ExtendRegionDecls.contains("RR_TEST"));
	ASSERT_EQ(project.ExtendRegionDecls.at("RR_TEST").size(), 1u);
}

TEST(CollectDeclarations, SingleEnum) {
	Project project;
	std::vector<EnumMemberDecl> members;
	members.emplace_back(Name("RG_HOOKSHOT"));
	members.emplace_back(Name("RG_FAIRY_BOW"), 7);
	project.files.push_back(makeEnumFile("a.rls", "Item", std::move(members)));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.EnumInfos.size(), 1u);
	ASSERT_TRUE(project.EnumInfos.contains("Item"));
	const auto& info = project.EnumInfos.at("Item");
	EXPECT_EQ(info.kind, EnumKind::Normal);
	ASSERT_EQ(info.entries.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<EnumMemberInfo>(info.entries[0]));
	ASSERT_TRUE(std::holds_alternative<EnumMemberInfo>(info.entries[1]));
	EXPECT_EQ(std::get<EnumMemberInfo>(info.entries[0]).name, "RG_HOOKSHOT");
	EXPECT_FALSE(std::get<EnumMemberInfo>(info.entries[0]).value.has_value());
	EXPECT_EQ(std::get<EnumMemberInfo>(info.entries[1]).name, "RG_FAIRY_BOW");
	ASSERT_TRUE(std::get<EnumMemberInfo>(info.entries[1]).value.has_value());
	EXPECT_EQ(*std::get<EnumMemberInfo>(info.entries[1]).value, 7);
}

TEST(CollectDeclarations, SingleExternEnum) {
	Project project;
	std::vector<ExternEnumEntryDecl> entries;
	entries.emplace_back(EnumMemberDecl(Name("RG_HOOKSHOT")));
	entries.emplace_back(EnumPatternDecl("RG_*"));
	project.files.push_back(makeExternEnumFile("a.rls", "Item", std::move(entries)));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.EnumInfos.size(), 1u);
	ASSERT_TRUE(project.EnumInfos.contains("Item"));
	const auto& info = project.EnumInfos.at("Item");
	EXPECT_EQ(info.kind, EnumKind::Extern);
	ASSERT_EQ(info.entries.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<EnumMemberInfo>(info.entries[0]));
	ASSERT_TRUE(std::holds_alternative<EnumPatternInfo>(info.entries[1]));
	EXPECT_EQ(std::get<EnumPatternInfo>(info.entries[1]).pattern, "RG_*");
}

// == Multiple declarations across files =======================================

TEST(CollectDeclarations, MultipleRegionsAcrossFiles) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_FOYER", "SCENE_SPIRIT"));
	project.files.push_back(makeRegionFile("b.rls", "RR_STATUE", "SCENE_SPIRIT"));
	project.files.push_back(makeRegionFile("c.rls", "RR_FIELD", "SCENE_FIELD"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.RegionDecls.size(), 3u);
	EXPECT_TRUE(project.RegionDecls.contains("RR_FOYER"));
	EXPECT_TRUE(project.RegionDecls.contains("RR_STATUE"));
	EXPECT_TRUE(project.RegionDecls.contains("RR_FIELD"));
}

TEST(CollectDeclarations, MixedDeclsInOneFile) {
	Project project;
	File f;
	f.path = "mixed.rls";

	f.declarations.emplace_back(RegionDecl(
		Name("RR_TEST"), makeRegionBody("SCENE_TEST")
	));
	f.declarations.emplace_back(DefineDecl(
		Name("helper"), {}, makeExpr(BoolLiteral{true})
	));

	std::vector<Entry> entries;
	entries.emplace_back(Name("RC_POT"), makeExpr(BoolLiteral{true}));
	std::vector<Section> sections;
	sections.emplace_back(SectionKind::Locations, std::move(entries));
	f.declarations.emplace_back(ExtendRegionDecl(Name("RR_TEST"), std::move(sections)));

	project.files.push_back(std::move(f));
	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_TRUE(project.ExtendRegionDecls.contains("RR_TEST"));
	EXPECT_EQ(project.ExtendRegionDecls.at("RR_TEST").size(), 1u);
}

TEST(CollectDeclarations, MultipleExtendsForSameRegion) {
	Project project;
	project.files.push_back(
		makeExtendFile("pots.rls", "RR_FOYER", SectionKind::Locations));
	project.files.push_back(
		makeExtendFile("crates.rls", "RR_FOYER", SectionKind::Locations));
	project.files.push_back(
		makeExtendFile("grass.rls", "RR_FOYER", SectionKind::Locations));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_TRUE(project.ExtendRegionDecls.contains("RR_FOYER"));
	EXPECT_EQ(project.ExtendRegionDecls.at("RR_FOYER").size(), 3u);
}

// == Duplicate declarations ===================================================

TEST(CollectDeclarations, DuplicateRegionError) {
	Project project;
	Span span1{"a.rls", {1, 1}, {3, 1}};
	Span span2{"b.rls", {5, 1}, {7, 1}};
	project.files.push_back(makeRegionFile("a.rls", "RR_TEST", "SCENE_A", span1));
	project.files.push_back(makeRegionFile("b.rls", "RR_TEST", "SCENE_B", span2));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate region 'RR_TEST'"), std::string::npos);
	// The first one wins
	ASSERT_TRUE(project.RegionDecls.contains("RR_TEST"));
	EXPECT_EQ(sceneName(*project.RegionDecls.at("RR_TEST")), "SCENE_A");
}

TEST(CollectDeclarations, DuplicateDefineError) {
	Project project;
	Span span1{"helpers.rls", {1, 1}, {1, 30}};
	Span span2{"other.rls", {10, 1}, {10, 30}};
	project.files.push_back(makeDefineFile("helpers.rls", "my_func", span1));
	project.files.push_back(makeDefineFile("other.rls", "my_func", span2));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate define 'my_func'"), std::string::npos);
	// The span on the diagnostic should point to the duplicate (second one)
	EXPECT_EQ(diags[0].span.file, "other.rls");
}

TEST(CollectDeclarations, DuplicateExternDefineError) {
	Project project;
	Span span1{"externs.rls", {1, 1}, {1, 40}};
	Span span2{"externs2.rls", {3, 1}, {3, 40}};
	project.files.push_back(makeExternDefineFile("externs.rls", "can_hit_switch", span1));
	project.files.push_back(makeExternDefineFile("externs2.rls", "can_hit_switch", span2));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate extern define 'can_hit_switch'"), std::string::npos);
	EXPECT_EQ(diags[0].span.file, "externs2.rls");
}

TEST(CollectDeclarations, DefineExternCollisionError) {
	Project project;
	project.files.push_back(makeDefineFile("helpers.rls", "can_hit_switch"));
	project.files.push_back(makeExternDefineFile("externs.rls", "can_hit_switch"));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate function 'can_hit_switch'"), std::string::npos);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.ExternDefineDecls.size(), 0u);
}

TEST(CollectDeclarations, ExternDefineCollisionError) {
	Project project;
	project.files.push_back(makeExternDefineFile("externs.rls", "can_hit_switch"));
	project.files.push_back(makeDefineFile("helpers.rls", "can_hit_switch"));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate function 'can_hit_switch'"), std::string::npos);
	EXPECT_EQ(project.DefineDecls.size(), 0u);
	EXPECT_EQ(project.ExternDefineDecls.size(), 1u);
}

TEST(CollectDeclarations, DuplicateRegionInSameFile) {
	Project project;
	File f;
	f.path = "bad.rls";
	Span span1{"bad.rls", {1, 1}, {3, 1}};
	Span span2{"bad.rls", {5, 1}, {7, 1}};
	f.declarations.emplace_back(RegionDecl(
		Name("RR_DUP"), makeRegionBody("SCENE_A"), span1
	));
	f.declarations.emplace_back(RegionDecl(
		Name("RR_DUP"), makeRegionBody("SCENE_B"), span2
	));
	project.files.push_back(std::move(f));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	// First wins
	EXPECT_EQ(sceneName(*project.RegionDecls.at("RR_DUP")), "SCENE_A");
}

TEST(CollectDeclarations, MultipleDuplicateErrors) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_A", "SCENE_A"));
	project.files.push_back(makeRegionFile("b.rls", "RR_A", "SCENE_A"));
	project.files.push_back(makeDefineFile("c.rls", "helper"));
	project.files.push_back(makeDefineFile("d.rls", "helper"));

	auto diags = collectDeclarations(project);

	EXPECT_EQ(countErrors(diags), 2u);
}

TEST(CollectDeclarations, DuplicateEnumError) {
	Project project;
	project.files.push_back(makeEnumFile("a.rls", "Item", {EnumMemberDecl(Name("RG_A"))}));
	project.files.push_back(makeExternEnumFile("b.rls", "Item", {EnumPatternDecl("RG_*")}));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate enum 'Item'"), std::string::npos);
	EXPECT_EQ(project.EnumInfos.size(), 1u);
}

// == Different decl types can share names =====================================

TEST(CollectDeclarations, SameNameDifferentDeclTypes) {
	// A region and define can share the same name — they live in separate
	// namespaces.
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "SAME_NAME", "SCENE_A"));
	project.files.push_back(makeDefineFile("b.rls", "SAME_NAME"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
}

// == Idempotency ==============================================================

TEST(CollectDeclarations, IdempotentOnRerun) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_TEST", "SCENE_TEST"));
	project.files.push_back(makeDefineFile("b.rls", "helper"));
	project.files.push_back(makeExternDefineFile("c.rls", "has"));

	auto diags1 = collectDeclarations(project);
	EXPECT_TRUE(diags1.empty());
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.ExternDefineDecls.size(), 1u);
	EXPECT_EQ(project.EnumInfos.size(), 0u);

	// Run again — should produce the same result, not accumulate.
	auto diags2 = collectDeclarations(project);
	EXPECT_TRUE(diags2.empty());
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.ExternDefineDecls.size(), 1u);
	EXPECT_EQ(project.EnumInfos.size(), 0u);
}

// == Pointer stability ========================================================

TEST(CollectDeclarations, PointersRefToOriginalDecl) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_TEST", "SCENE_TEST"));
	project.files.push_back(makeDefineFile("b.rls", "helper"));
	project.files.push_back(makeExternDefineFile("c.rls", "has"));

	collectDeclarations(project);

	// Pointers should point into the File's declaration vector.
	const auto* regionPtr = project.RegionDecls.at("RR_TEST");
	const auto& fileDecl = std::get<RegionDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(regionPtr, &fileDecl);

	const auto* definePtr = project.DefineDecls.at("helper");
	const auto& fileDefine = std::get<DefineDecl>(project.files[1].declarations[0]);
	EXPECT_EQ(definePtr, &fileDefine);

	const auto* externDefinePtr = project.ExternDefineDecls.at("has");
	const auto& fileExternDefine = std::get<ExternDefineDecl>(project.files[2].declarations[0]);
	EXPECT_EQ(externDefinePtr, &fileExternDefine);
}

// == Integration with parser ==================================================

TEST(CollectDeclarations, ParsedRegion) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"region RR_SPIRIT_TEMPLE_FOYER {\n"
		"    name: \"Spirit Temple Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_SPIRIT_TEMPLE_ENTRYWAY: always\n"
		"    }\n"
		"}\n",
		"spirit_temple.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.RegionDecls.size(), 1u);
	const auto* r = project.RegionDecls.at("RR_SPIRIT_TEMPLE_FOYER");
	EXPECT_EQ(r->key, "RR_SPIRIT_TEMPLE_FOYER");
	EXPECT_EQ(sceneName(*r), "SCENE_SPIRIT_TEMPLE");
	ASSERT_EQ(r->body.sections.size(), 1u);
	EXPECT_EQ(r->body.sections[0].kind, SectionKind::Exits);
}

TEST(CollectDeclarations, ParsedDefine) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"define has_explosives():\n"
		"    has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)\n",
		"helpers.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_TRUE(project.DefineDecls.contains("has_explosives"));
}

TEST(CollectDeclarations, ParsedExternDefine) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extern define can_hit_switch(distance: Distance = ED_CLOSE, inWater = false) -> Bool\n",
		"externs.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.ExternDefineDecls.size(), 1u);
	EXPECT_TRUE(project.ExternDefineDecls.contains("can_hit_switch"));
}

TEST(CollectDeclarations, ParsedExtendRegion) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extend region RR_SPIRIT_TEMPLE_FOYER {\n"
		"    locations {\n"
		"        RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"    }\n"
		"}\n",
		"pots.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_TRUE(project.ExtendRegionDecls.contains("RR_SPIRIT_TEMPLE_FOYER"));
	ASSERT_EQ(project.ExtendRegionDecls.at("RR_SPIRIT_TEMPLE_FOYER").size(), 1u);
}

TEST(CollectDeclarations, ParsedEnum) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"enum Item { RG_HOOKSHOT, RG_FAIRY_BOW = 7 }\n",
		"enums.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_TRUE(project.EnumInfos.contains("Item"));
	const auto& info = project.EnumInfos.at("Item");
	EXPECT_EQ(info.kind, EnumKind::Normal);
	ASSERT_EQ(info.entries.size(), 2u);
}

TEST(CollectDeclarations, ParsedExternEnum) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extern enum Item { RG_HOOKSHOT, RG_* }\n",
		"extern_enums.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_TRUE(project.EnumInfos.contains("Item"));
	const auto& info = project.EnumInfos.at("Item");
	EXPECT_EQ(info.kind, EnumKind::Extern);
	ASSERT_EQ(info.entries.size(), 2u);
}

TEST(CollectDeclarations, ParsedMultiFileProject) {
	Project project;

	project.files.push_back(rls::parser::ParseString(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_ENTRYWAY: always\n"
		"    }\n"
		"}\n"
		"region RR_STATUE {\n"
		"    name: \"Statue\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n",
		"spirit_temple.rls"
	));

	project.files.push_back(rls::parser::ParseString(
		"define has_explosives():\n"
		"    has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)\n"
		"define blast_or_smash():\n"
		"    has_explosives() or can_use(RG_MEGATON_HAMMER)\n",
		"helpers.rls"
	));

	project.files.push_back(rls::parser::ParseString(
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT_1: can_break_pots()\n"
		"        RC_POT_2: can_break_pots()\n"
		"    }\n"
		"}\n",
		"pots.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.RegionDecls.size(), 2u);
	EXPECT_EQ(project.DefineDecls.size(), 2u);
	EXPECT_TRUE(project.ExtendRegionDecls.contains("RR_FOYER"));
	EXPECT_EQ(project.ExtendRegionDecls.at("RR_FOYER").size(), 1u);

	EXPECT_TRUE(project.RegionDecls.contains("RR_FOYER"));
	EXPECT_TRUE(project.RegionDecls.contains("RR_STATUE"));
	EXPECT_TRUE(project.DefineDecls.contains("has_explosives"));
	EXPECT_TRUE(project.DefineDecls.contains("blast_or_smash"));
}

TEST(CollectDeclarations, ParsedDuplicateRegionAcrossFiles) {
	Project project;

	project.files.push_back(rls::parser::ParseString(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n",
		"a.rls"
	));

	project.files.push_back(rls::parser::ParseString(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_FOREST_TEMPLE\n"
		"}\n",
		"b.rls"
	));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate region 'RR_FOYER'"), std::string::npos);
	// First declaration wins
	EXPECT_EQ(sceneName(*project.RegionDecls.at("RR_FOYER")),
	          "SCENE_SPIRIT_TEMPLE");
}

// == Diagnostic span quality ==================================================

TEST(CollectDeclarations, DiagnosticSpanPointsToDuplicate) {
	Project project;
	Span firstSpan{"first.rls", {1, 1}, {3, 1}};
	Span dupSpan{"dup.rls", {10, 5}, {12, 1}};
	project.files.push_back(makeRegionFile("first.rls", "RR_X", "SCENE_A", firstSpan));
	project.files.push_back(makeRegionFile("dup.rls", "RR_X", "SCENE_B", dupSpan));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(diags.size(), 1u);
	// The diagnostic's span should be the *duplicate* (second declaration)
	EXPECT_EQ(diags[0].span.file, "dup.rls");
	EXPECT_EQ(diags[0].span.start.line, 10u);
	EXPECT_EQ(diags[0].span.start.column, 5u);
	// The message should reference the *first* declaration's location
	EXPECT_NE(diags[0].message.find("first.rls"), std::string::npos);
	EXPECT_NE(diags[0].message.find("1"), std::string::npos);
}

// == analyze() entry point ====================================================

TEST(Analyze, PopulatesDeclMaps) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extern enum Item { RG_* }\n"
		"extern enum Scene { SCENE_* }\n"
		"extern define has(item: Item) -> Bool\n"
		"extern define can_use(item: Item) -> Bool\n"
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_ENTRYWAY: always\n"
		"    }\n"
		"}\n"
		"define has_explosives():\n"
		"    has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)\n"
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT: has(RG_POWER_BRACELET)\n"
		"    }\n"
		"}\n",
		"all.rls"
	));

	auto diags = analyze(project);

	// Filter out validation warnings (e.g. unused defines) — only check for errors.
	size_t errors = 0;
	for (const auto& d : diags)
		if (d.level == DiagnosticLevel::Error) ++errors;
	EXPECT_EQ(errors, 0u);
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_TRUE(project.ExtendRegionDecls.contains("RR_FOYER"));
	EXPECT_EQ(project.ExtendRegionDecls.at("RR_FOYER").size(), 1u);
}

TEST(Analyze, ReturnsDiagnosticsFromAllPasses) {
	Project project;
	project.files.push_back(rls::parser::ParseString("extern enum Scene { SCENE_* }\n"));
	project.files.push_back(makeRegionFile("a.rls", "RR_DUP", "SCENE_A"));
	project.files.push_back(makeRegionFile("b.rls", "RR_DUP", "SCENE_B"));

	auto diags = analyze(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate region 'RR_DUP'"), std::string::npos);
}
