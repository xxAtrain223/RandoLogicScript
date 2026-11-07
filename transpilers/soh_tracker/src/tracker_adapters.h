#pragma once

#include <span>
#include <string>

#include "tracker_ir.h"
#include "soh_tracker.h"

namespace rls::transpilers::soh_tracker::ir {

using ::rls::transpilers::soh_tracker::HostEnumBinding;

struct RenderedAdapter {
	std::string symbol;
	std::string definition;
};

std::optional<std::string> GetHostEnumType(const ast::EnumInfo& declaration,
	std::span<const HostEnumBinding> customEnums = {});

std::vector<ast::Diagnostic> ValidateHostAdapters(const Graph& graph,
	std::span<const HostEnumBinding> customEnums = {});

// Requires successful host-adapter preflight.
std::vector<RenderedAdapter> RenderValidatedHostAdapters(const Graph& graph,
	std::span<const HostEnumBinding> customEnums = {});

} // namespace rls::transpilers::soh_tracker::ir
