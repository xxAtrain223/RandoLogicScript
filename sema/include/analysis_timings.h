#pragma once

#include <chrono>

namespace rls::sema {

struct SemanticAnalysisTimings {
	std::chrono::nanoseconds declarationCollection{};
	std::chrono::nanoseconds typeResolution{};
	std::chrono::nanoseconds validation{};
};

} // namespace rls::sema
