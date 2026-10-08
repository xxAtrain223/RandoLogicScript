#pragma once

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "ast.h"

namespace rls::transpilers::ap {

using EntryWriter = std::function<void(const rls::ast::Entry&)>;

// Invoke `writer` for every entry in every section of the given kind.
inline void WriteEntries(
	const std::vector<rls::ast::Section>& sections,
	rls::ast::SectionKind sectionKind,
	const EntryWriter& writer)
{
	for (const auto& section : sections) {
		if (section.kind == sectionKind) {
			for (const auto& entry : section.entries) {
				writer(entry);
			}
		}
	}
}

// A region's human-readable display name: the `name: "..."` data entry. Region bodies
// carry arbitrary key/value data, so the key is looked up rather than being a fixed field;
// a missing or non-string `name` yields "" (sema/the target's Validate() reports it).
inline std::string RegionDisplayName(const rls::ast::RegionDecl& region) {
	const auto* entry = region.body.findData("name");
	if (entry == nullptr) {
		return "";
	}
	const auto* literal = std::get_if<rls::ast::StringLiteral>(&entry->value->node);
	return literal != nullptr ? literal->value : "";
}

// A region's scene, as the bare `SCENE_*` token. Unlike `name`, the data value is an enum
// identifier rather than a string literal; a missing or non-identifier `scene` yields "".
inline std::string RegionScene(const rls::ast::RegionDecl& region) {
	const auto* entry = region.body.findData("scene");
	if (entry == nullptr) {
		return "";
	}
	const auto* ident = std::get_if<rls::ast::Identifier>(&entry->value->node);
	return ident != nullptr ? ident->name.text : "";
}

// Collect the names of every entry in every section of the given kind.
inline void InsertToSet(
	const std::vector<rls::ast::Section>& sections,
	rls::ast::SectionKind sectionKind,
	std::set<std::string>& emittedValues)
{
	for (const auto& section : sections) {
		if (section.kind == sectionKind) {
			for (const auto& entry : section.entries) {
				emittedValues.insert(entry.name.text);
			}
		}
	}
}

} // namespace rls::transpilers::ap
