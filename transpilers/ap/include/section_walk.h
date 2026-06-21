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
