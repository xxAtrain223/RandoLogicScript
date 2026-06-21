#include "ap_transpiler.h"
#include "section_walk.h"

#include <sstream>
#include <vector>

namespace rls::transpilers::ap {

void ApTranspiler::GenerateRegionsSource(rls::OutputWriter& out) const {
	auto& source = out.open("regions.gen.py");
	source << regionsPreamble();

	// Helper-call names are the same for every region; resolve them once.
	const std::string eventsFn = addEventsFn();
	const std::string locationsFn = addLocationsFn();
	const std::string exitsFn = connectRegionsFn();

	for (const auto& [regionName, region] : project.RegionDecls) {
		const auto extendRegionIt = project.ExtendRegionDecls.find(region->key.text);

		std::vector<const rls::ast::ExtendRegionDecl*> extendRegionDecls;
		if (extendRegionIt != project.ExtendRegionDecls.end()) {
			extendRegionDecls = extendRegionIt->second;
		}

		const std::string creationArgs = regionCreationArgs(region->key.text);

		// Build one `<label> / <fn>(<creationArgs> ... )` section, walking the region's own
		// sections plus any extend-region sections. Returns "" when the section has no entries
		// so empty helper calls are elided from the output.
		auto buildSection = [&](const char* label, const std::string& fnName,
			rls::ast::SectionKind kind, auto&& formatEntry) -> std::string {
			std::ostringstream body;
			bool wroteEntry = false;
			auto writeFrom = [&](const std::vector<rls::ast::Section>& sections) {
				WriteEntries(sections, kind, [&](const rls::ast::Entry& entry) {
					body << formatEntry(entry);
					wroteEntry = true;
				});
			};
			writeFrom(region->body.sections);
			for (const auto* extendRegion : extendRegionDecls) {
				writeFrom(extendRegion->sections);
			}
			if (!wroteEntry) {
				return "";
			}
			return "    # " + std::string(label) + "\n    " + fnName + "(" + creationArgs + body.str() + "    ])\n";
		};

		const std::string events = buildSection("Events", eventsFn, rls::ast::SectionKind::Events,
			[&](const rls::ast::Entry& entry) {
				return eventEntryLine(region->key.text, entry.name.text, GenerateExpression(entry.condition));
			});
		const std::string locations = buildSection("Locations", locationsFn, rls::ast::SectionKind::Locations,
			[&](const rls::ast::Entry& entry) {
				SetCurrentLocation(std::optional<std::string>(entry.name.text));
				std::string line = locationEntryLine(entry.name.text, GenerateExpression(entry.condition));
				SetCurrentLocation(std::nullopt);
				return line;
			});
		const std::string exits = buildSection("Exits", exitsFn, rls::ast::SectionKind::Exits,
			[&](const rls::ast::Entry& entry) {
				return exitEntryLine(entry.name.text, GenerateExpression(entry.condition));
			});

		source << "    # " << region->body.name << "\n";
		source << events << locations << exits << "\n";
	}
}

} // namespace rls::transpilers::ap
