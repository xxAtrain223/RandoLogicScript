#pragma once

#include "tracker_graph.h"

namespace rls::transpilers::soh_tracker::ir {

// Borrowed inputs to expression lowering, not yet numbered graph roots.
struct RootSource {
	ast::SectionKind kind;
	const ast::RegionDecl* owner;
	const ast::Entry* entry;
	uint32_t occurrence = 0;
};

// Requires a successfully analyzed project.
// Preserve source order, including extensions that precede their base declaration.
std::variant<std::vector<RootSource>, std::vector<ast::Diagnostic>> CollectRoots(
	const ast::Project& project, const std::filesystem::path& sourceRoot);

} // namespace rls::transpilers::soh_tracker::ir
