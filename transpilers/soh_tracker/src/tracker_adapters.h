#pragma once

#include <span>
#include <string>

#include "tracker_ir.h"

namespace rls::transpilers::soh_tracker::ir {

struct HostEnumBinding {
	std::string rlsName;
	std::string cppType;
};

struct RenderedAdapter {
	AdapterHandle handle;
	std::string symbol;
	std::string definition;
};

using AdapterRenderingResult =
	std::variant<std::vector<RenderedAdapter>, std::vector<ast::Diagnostic>>;

std::optional<std::string> GetHostEnumType(const ast::EnumInfo& declaration,
	std::span<const HostEnumBinding> customEnums = {});

std::vector<ast::Diagnostic> ValidateHostAdapters(const Graph& graph,
	std::span<const HostEnumBinding> customEnums = {});

// Definitions use the emitter's CheckedHostInt helper for integral/enum returns.
// Custom enum mappings supply host type names only.
AdapterRenderingResult RenderHostAdapters(const Graph& graph,
	std::span<const HostEnumBinding> customEnums = {});

} // namespace rls::transpilers::soh_tracker::ir
