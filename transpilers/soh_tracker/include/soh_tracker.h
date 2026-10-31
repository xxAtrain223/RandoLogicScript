#pragma once

#include <filesystem>
#include <span>

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh_tracker {

struct HostEnumBinding {
	std::string rlsName;
	std::string cppType;
};

class SohTrackerTranspiler {
public:
	explicit SohTrackerTranspiler(const rls::ast::Project& project);

	// sourceRoot must be absolute; all artifacts are rendered before opening outputs.
	// IO exceptions propagate as in SoH; the caller owns flushing/closing the streams.
	std::vector<rls::ast::Diagnostic> Transpile(rls::OutputWriter& out,
		const std::filesystem::path& sourceRoot,
		std::span<const HostEnumBinding> customEnums = {}) const;

private:
	const rls::ast::Project& project;
};

std::vector<rls::ast::Diagnostic> Transpile(const rls::ast::Project& project,
	rls::OutputWriter& out, const std::filesystem::path& sourceRoot,
	std::span<const HostEnumBinding> customEnums = {});

} // namespace rls::transpilers::soh_tracker
