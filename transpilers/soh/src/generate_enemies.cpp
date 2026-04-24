#include "soh.h"
#include "generate_expression.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <vector>

namespace rls::transpilers::soh {

// ── Fixed parameter definitions for each enemy field kind ───────────────────
//
// These mirror the existing handwritten C++ signatures exactly:
//   CanKillEnemy(enemy, distance=ED_CLOSE, wallOrFloor=true, quantity=1, timer=false, inWater=false)
//   CanPassEnemy(enemy, distance=ED_CLOSE, wallOrFloor=true)
//   CanAvoidEnemy(enemy, grounded=false, quantity=1)
//   CanGetEnemyDrop(enemy, distance=ED_CLOSE, aboveLink=false)

struct FieldParam {
	std::string type;
	std::string name;
	std::string defaultValue;
};

struct EnemyFunctionDef {
	rls::ast::EnemyFieldKind kind;
	std::string cppName;
	std::vector<FieldParam> params; // excludes the leading `RandomizerEnemy enemy`
	std::string preamble;           // code inserted before the switch
	std::string switchDefault;      // body of the `default:` case
};

static const std::array<EnemyFunctionDef, 4>& enemyFunctions() {
	static const std::array<EnemyFunctionDef, 4> defs = {{
		{
			rls::ast::EnemyFieldKind::Kill,
			"CanKillEnemy",
			{
				{ "EnemyDistance", "distance", "ED_CLOSE" },
				{ "bool", "wallOrFloor", "true" },
				{ "uint8_t", "quantity", "1" },
				{ "bool", "timer", "false" },
				{ "bool", "inWater", "false" },
			},
			"",
			"return false;",
		},
		{
			rls::ast::EnemyFieldKind::Pass,
			"CanPassEnemy",
			{
				{ "EnemyDistance", "distance", "ED_CLOSE" },
				{ "bool", "wallOrFloor", "true" },
			},
			"    if (CanKillEnemy(enemy, distance, wallOrFloor)) {\n"
			"        return true;\n"
			"    }\n",
			"return true;",
		},
		{
			rls::ast::EnemyFieldKind::Avoid,
			"CanAvoidEnemy",
			{
				{ "bool", "grounded", "false" },
				{ "uint8_t", "quantity", "1" },
			},
			"    if (CanKillEnemy(enemy, ED_CLOSE, true, quantity)) {\n"
			"        return true;\n"
			"    }\n",
			"return true;",
		},
		{
			rls::ast::EnemyFieldKind::Drop,
			"CanGetEnemyDrop",
			{
				{ "EnemyDistance", "distance", "ED_CLOSE" },
				{ "bool", "aboveLink", "false" },
			},
			"    if (!CanKillEnemy(enemy, distance)) {\n"
			"        return false;\n"
			"    }\n"
			"    if (distance <= ED_MASTER_SWORD_JUMPSLASH) {\n"
			"        return true;\n"
			"    }\n",
			"return aboveLink || (distance <= ED_BOOMERANG && logic->CanUse(RG_BOOMERANG));",
		},
	}};
	return defs;
}

// ── Helpers ─────────────────────────────────────────────────────────────────

/// Find the field of a given kind in an enemy declaration, or nullptr.
static const rls::ast::EnemyField* findField(
	const rls::ast::EnemyDecl& decl,
	rls::ast::EnemyFieldKind kind)
{
	for (const auto& field : decl.fields) {
		if (field.kind == kind) {
			return &field;
		}
	}
	return nullptr;
}

/// Build the C++ signature string for a dispatch function (definition form).
///   bool CanKillEnemy(RandomizerEnemy enemy, EnemyDistance distance, ...)
static std::string functionSignature(const EnemyFunctionDef& def) {
	std::ostringstream sig;
	sig << "bool " << def.cppName << "(RandomizerEnemy enemy";
	for (const auto& p : def.params) {
		sig << ", " << p.type << " " << p.name;
	}
	sig << ")";
	return sig.str();
}

/// Build the C++ declaration string with default parameter values.
///   bool CanKillEnemy(RandomizerEnemy enemy, EnemyDistance distance = ED_CLOSE, ...);
static std::string functionDeclaration(const EnemyFunctionDef& def) {
	std::ostringstream sig;
	sig << "bool " << def.cppName << "(RandomizerEnemy enemy";
	for (const auto& p : def.params) {
		sig << ", " << p.type << " " << p.name << " = " << p.defaultValue;
	}
	sig << ")";
	return sig.str();
}

/// Generate the body expression for a specific enemy's field.
/// Only called when the enemy has an explicit field of this kind.
static std::string fieldBody(
	const rls::ast::EnemyDecl& enemy,
	const EnemyFunctionDef& def)
{
	const auto* field = findField(enemy, def.kind);

	if (field) {
		return GenerateExpression(field->body);
	}

	// Field not explicitly defined — apply defaults per the spec:
	//   kill: required (sema enforces this, should never reach here)
	//   pass: always (true)
	//   avoid: always (true)
	//   drop: no case emitted, falls through to switch default
	switch (def.kind) {
		case rls::ast::EnemyFieldKind::Pass:
			return "true";
		case rls::ast::EnemyFieldKind::Avoid:
			return "true";
		default:
			return "false";
	}
}

// ── Sorted enemy names ──────────────────────────────────────────────────────

/// Return enemy declarations sorted by name for deterministic output.
static std::vector<const rls::ast::EnemyDecl*> sortedEnemies(
	const rls::ast::Project& project)
{
	std::vector<const rls::ast::EnemyDecl*> enemies;
	enemies.reserve(project.EnemyDecls.size());
	for (const auto& [name, decl] : project.EnemyDecls) {
		enemies.push_back(decl);
	}
	std::sort(enemies.begin(), enemies.end(),
		[](const rls::ast::EnemyDecl* a, const rls::ast::EnemyDecl* b) {
			return a->name < b->name;
		});
	return enemies;
}

// ── Generator entry points ──────────────────────────────────────────────────

void SohTranspiler::GenerateEnemiesHeader(rls::OutputWriter& out) const {
	auto& header = out.open("enemies.gen.h");
	header << "// Generated by RLS soh transpiler\n"
	       << "#pragma once\n"
	       << "\n"
	       << "#include \"soh/Enhancements/randomizer/logic.h\"\n"
	       << "\n";

	for (const auto& def : enemyFunctions()) {
		header << functionDeclaration(def) << ";\n";
	}
}

void SohTranspiler::GenerateEnemiesSource(rls::OutputWriter& out) const {
	auto& source = out.open("enemies.gen.cpp");
	source << "// Generated by RLS soh transpiler\n"
	       << "#include \"enemies.gen.h\"\n"
           << "#include \"functions.gen.h\"\n";

	const auto enemies = sortedEnemies(project);

	for (const auto& def : enemyFunctions()) {
		source << "\n"
		       << functionSignature(def) << " {\n";

		// Emit preamble (e.g. CanKillEnemy check for pass/avoid/drop)
		if (!def.preamble.empty()) {
			source << def.preamble;
		}

		source << "    switch (enemy) {\n";

		for (const auto* enemy : enemies) {
			const auto* field = findField(*enemy, def.kind);

			// For fields with an explicit default that matches the switch
			// default, we can omit the case. But for correctness and
			// readability, we always emit a case for enemies with an
			// explicit field, and also for kill (required) and for drop
			// when it defaults to kill (which may differ from the switch
			// default).
			// Only emit a case when the enemy has an explicit field
			// of this kind.  Without one, pass/avoid default to true
			// (matching the switch default) and drop falls through to
			// the switch default collection logic.
			bool needsCase = (field != nullptr);

			if (needsCase) {
				source << "        case " << enemy->name << ":\n"
				       << "            return " << fieldBody(*enemy, def) << ";\n";
			}
		}

		source << "        default:\n"
		       << "            " << def.switchDefault << "\n"
		       << "    }\n"
		       << "}\n";
	}
}

} // namespace rls::transpilers::soh
