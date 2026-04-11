#pragma once

#include <ostream>
#include <string>

namespace rls {

/// Abstract sink that transpilers use to create output files.
/// Production code writes to disk; tests use an in-memory implementation.
struct OutputWriter {
	virtual ~OutputWriter() = default;

	/// Open (or create) a named output file and return a stream to write to.
	virtual std::ostream& open(const std::string& filename) = 0;
};

} // namespace rls
