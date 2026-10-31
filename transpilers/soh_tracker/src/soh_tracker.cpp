#include "soh_tracker.h"

#include "tracker_emission.h"
#include "tracker_lowering.h"

namespace rls::transpilers::soh_tracker {

SohTrackerTranspiler::SohTrackerTranspiler(const rls::ast::Project& project)
	: project(project) {}

std::vector<rls::ast::Diagnostic> SohTrackerTranspiler::Transpile(rls::OutputWriter& out,
	const std::filesystem::path& sourceRoot, std::span<const HostEnumBinding> customEnums) const
{
	auto lowered = ir::LowerRoots(project, sourceRoot);
	if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&lowered)) return *errors;
	return ir::EmitArtifacts(project, std::get<ir::Graph>(lowered), out, sourceRoot, customEnums);
}

std::vector<rls::ast::Diagnostic> Transpile(const rls::ast::Project& project,
	rls::OutputWriter& out, const std::filesystem::path& sourceRoot,
	std::span<const HostEnumBinding> customEnums)
{
	return SohTrackerTranspiler(project).Transpile(out, sourceRoot, customEnums);
}

} // namespace rls::transpilers::soh_tracker
