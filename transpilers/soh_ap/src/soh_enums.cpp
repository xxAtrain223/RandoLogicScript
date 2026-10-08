#include "soh_ap.h"
#include "section_walk.h"

#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#include <filesystem>

namespace rls::transpilers::soh_ap {

using ap::WriteEntries;
using ap::InsertToSet;
using ap::RegionDisplayName;
using ap::RegionScene;

namespace {

// Python string literal, with the two characters that could end or escape it neutralized.
std::string pyString(std::string_view value) {
	std::string out = "\"";
	for (const char c : value) {
		if (c == '\\' || c == '"') {
			out += '\\';
		}
		out += c;
	}
	return out + "\"";
}

// Which quest variant a region belongs to, taken from the file that declares it. SoH keeps the
// two dungeon layouts in <dungeon>_mq.rls and <dungeon>_vanilla.rls, and the regions common to
// both -- entryways and boss rooms, the ones actually carrying the is_mq()/is_vanilla() branch --
// in <dungeon>.rls. "" therefore means "exists in both quests", which is a third state no naming
// rule can see: RR_DEKU_TREE_ENTRYWAY and RR_DEKU_TREE_LOBBY look alike, but only the first is
// shared, and dropping it along with the vanilla half would disconnect the dungeon entirely.
std::string regionQuest(const rls::ast::RegionDecl& region) {
	const std::filesystem::path file{region.span.file};
	if (file.extension() != ".rls") {
		return "";
	}
	const std::string stem = file.stem().string();
	if (stem.ends_with("_mq")) {
		return "mq";
	}
	if (stem.ends_with("_vanilla")) {
		return "vanilla";
	}
	return "";
}


// Emit a StrEnum whose members are `auto()`-valued. The shared _generate_next_value_ turns
// a member name into its display string by dropping `stripPrefix` and title-casing the rest
// (RC_SONG_FROM_SARIA -> "Song From Saria"), so the values never have to be spelled out.
void writeAutoStrEnum(std::ostream& out, std::string_view className,
	std::string_view stripPrefix, const std::vector<std::string>& members)
{
	out << "\nclass " << className << "(StrEnum):\n"
		<< "    @staticmethod\n"
		<< "    def _generate_next_value_(name, start, count, last_values):\n"
		<< "        return name.replace(\"" << stripPrefix << "\", \"\").replace(\"_\", \" \").title()\n";
	for (const auto& member : members) {
		out << "    " << member << " = auto()\n";
	}
}

// Emit the Python class for one RLS `enum Foo { A, B = 3 }`. Members carry the values sema
// assigned them; a member that somehow lacks one falls back to auto(), since a Python enum
// member must be given a value.
void writeDeclaredEnum(std::ostream& out, std::string_view className, const rls::ast::EnumInfo& info) {
	out << "\nclass " << className << "(IntEnum):\n";
	bool wroteMember = false;
	for (const auto& entry : info.entries) {
		const auto* member = std::get_if<rls::ast::EnumMemberInfo>(&entry);
		if (member == nullptr) {
			// A pattern entry (`RG_*`) names no member -- only extern enums have these, and
			// those are skipped wholesale by the caller.
			continue;
		}
		out << "    " << member->name.text << " = ";
		if (member->value.has_value()) {
			out << *member->value;
		} else {
			out << "auto()";
		}
		out << "\n";
		wroteMember = true;
	}
	if (!wroteMember) {
		out << "    pass\n";
	}
}

// Every LOGIC_* an expression names. An event no region declares -- Ship's LOGIC_BUY_BOMBCHUS is
// the logic value of a shop item, not something a region sets -- still needs an enum member, so the
// generated code can name it and the host can give it a meaning.
void collectEventRefs(const rls::ast::Expr* expr, std::set<std::string>& out) {
	if (expr == nullptr) {
		return;
	}
	std::visit([&](const auto& node) {
		using T = std::decay_t<decltype(node)>;
		if constexpr (std::is_same_v<T, rls::ast::Identifier>) {
			if (node.name.text.rfind("LOGIC_", 0) == 0) {
				out.insert(node.name.text);
			}
		} else if constexpr (std::is_same_v<T, rls::ast::UnaryExpr>) {
			collectEventRefs(node.operand.get(), out);
		} else if constexpr (std::is_same_v<T, rls::ast::BinaryExpr>) {
			collectEventRefs(node.left.get(), out);
			collectEventRefs(node.right.get(), out);
		} else if constexpr (std::is_same_v<T, rls::ast::TernaryExpr>) {
			collectEventRefs(node.condition.get(), out);
			collectEventRefs(node.thenBranch.get(), out);
			collectEventRefs(node.elseBranch.get(), out);
		} else if constexpr (std::is_same_v<T, rls::ast::CallExpr>) {
			for (const auto& arg : node.args) {
				collectEventRefs(arg.value.get(), out);
			}
		} else if constexpr (std::is_same_v<T, rls::ast::InvokeExpr>) {
			collectEventRefs(node.callee.get(), out);
		} else if constexpr (std::is_same_v<T, rls::ast::MatchExpr>) {
			collectEventRefs(node.discriminant.get(), out);
			for (const auto& arm : node.arms) {
				for (const auto& pattern : arm.patterns) {
					collectEventRefs(pattern.get(), out);
				}
				collectEventRefs(arm.body.get(), out);
			}
		} else if constexpr (std::is_same_v<T, rls::ast::ListExpr>) {
			for (const auto& element : node.elements) {
				collectEventRefs(element.get(), out);
			}
		}
	}, expr->node);
}

void collectEventRefs(const std::vector<rls::ast::Section>& sections, std::set<std::string>& out) {
	for (const auto& section : sections) {
		for (const auto& entry : section.entries) {
			collectEventRefs(entry.condition.get(), out);
		}
	}
}

} // namespace

void SohApTranspiler::writeEnums(rls::OutputWriter& out) const {
	auto& source = out.open("enums_gen.py");

	source
		<< "# Generated by RLS soh_ap transpiler\n"
		<< "from enum import StrEnum, IntEnum, auto\n";

	// == Enums materialized from the region declarations ======================
	// Region and Event are extern enums: RLS knows them only as glob patterns, but this project
	// *declares* the values that matter (every region and event), so their classes are built
	// from the region walk rather than from the enum declaration.
	std::vector<std::string> eventLocations;   // region x event pairs, in region order
	std::vector<std::string> regions;          // `RR_X = "Display Name"` lines
	std::vector<std::pair<std::string, std::string>> regionScenes;  // region key -> SCENE_ token
	std::vector<std::pair<std::string, std::string>> regionQuests;  // region key -> "mq"/"vanilla"/""
	std::set<std::string> events;              // deduplicated across regions and extensions

	for (const auto& [regionName, region] : project.RegionDecls) {
		regions.push_back(region->key.text + " = \"" + RegionDisplayName(*region) + "\"");
		regionScenes.emplace_back(region->key.text, RegionScene(*region));
		regionQuests.emplace_back(region->key.text, regionQuest(*region));

		// A region's own sections plus every `extend region` block that targets it.
		std::vector<const rls::ast::ExtendRegionDecl*> extendRegionDecls;
		if (const auto it = project.ExtendRegionDecls.find(region->key.text);
			it != project.ExtendRegionDecls.end()) {
			extendRegionDecls = it->second;
		}

		auto collectFrom = [&](const std::vector<rls::ast::Section>& sections) {
			WriteEntries(sections, rls::ast::SectionKind::Events, [&](const rls::ast::Entry& entry) {
				eventLocations.push_back(region->key.text + "_" + entry.name.text);
			});
			InsertToSet(sections, rls::ast::SectionKind::Events, events);
		};

		collectFrom(region->body.sections);
		collectEventRefs(region->body.sections, events);
		for (const auto* extendRegion : extendRegionDecls) {
			collectFrom(extendRegion->sections);
			collectEventRefs(extendRegion->sections, events);
		}
	}
	for (const auto& [name, define] : project.DefineDecls) {
		collectEventRefs(define->body.get(), events);
		for (const auto& param : define->params) {
			collectEventRefs(param.defaultValue.get(), events);
		}
	}

	writeAutoStrEnum(source, "EventLocations", "RR_", eventLocations);
	writeAutoStrEnum(source, "Events", "LOGIC_", {events.begin(), events.end()});

	// RR_NONE is Ship's sentinel region (RandomizerRegion.h). host.rls uses it as the default for
	// spirit_shared's optional region parameters, so the generated code references it, but no
	// region declares it and the walk above never produces it. It is always emitted: this is the
	// SoH target, and the sentinel is part of its host ABI rather than of any one project.
	regions.push_back("RR_NONE = \"None\"");
	regionScenes.emplace_back("RR_NONE", "");
	regionQuests.emplace_back("RR_NONE", "");

	// Unlike the auto() StrEnums above, this class has no _generate_next_value_ to fall back
	// on: every member carries its own display name.
	source << "\nclass Regions(StrEnum):\n";
	for (const auto& region : regions) {
		source << "    " << region << "\n";
	}

	// Which scene each region sits in. The host needs this to answer is_mq()/is_vanilla():
	// a rule's region identifies its dungeon only via the scene -- RR_GANONS_TOWER_ENTRYWAY is
	// SCENE_INSIDE_GANONS_CASTLE, so it follows Ganon's Castle, which no name rule would get right.
	source << "\nREGION_SCENE: dict[Regions, str] = {\n";
	for (const auto& [regionKey, scene] : regionScenes) {
		source << "    Regions." << regionKey << ": " << pyString(scene) << ",\n";
	}
	source << "}\n";

	// Which quest each region belongs to; "" for the ones both quests share.
	source << "\nREGION_QUEST: dict[Regions, str] = {\n";
	for (const auto& [regionKey, quest] : regionQuests) {
		source << "    Regions." << regionKey << ": " << pyString(quest) << ",\n";
	}
	source << "}\n";

	// No Locations or Items class: both are extern and glob-only (`extern enum Location { RC_* }`),
	// so RLS never learns their names. Their values are a name contract with the Ship client --
	// title-casing an identifier gives "Kf Kokiri Sword Chest" where Ship has "KF Kokiri Sword
	// Chest" -- so the host generates both from Shipwright itself.

	// == Enums declared directly in RLS =======================================
	// `enum Foo { ... }` carries its members in the AST, so the class is generated here and
	// referenced by its own name (see enumClassName). `extern enum Foo { RG_* }` is a glob
	// pattern over host names -- RLS never learns its members -- so the hand-written world
	// supplies those classes and they are skipped.
	for (const auto& [enumName, info] : project.EnumInfos) {
		if (info.kind != rls::ast::EnumKind::Normal) {
			continue;
		}
		writeDeclaredEnum(source, enumName, info);
	}
}

} // namespace rls::transpilers::soh_ap
