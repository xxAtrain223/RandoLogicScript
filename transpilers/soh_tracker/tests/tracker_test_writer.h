#pragma once

#include <map>
#include <sstream>

#include "output.h"

namespace rls::transpilers::soh_tracker::test {

struct CapturedOutput {
	std::string header;
	std::string source;
	bool operator==(const CapturedOutput&) const = default;
};

class MemoryWriter : public rls::OutputWriter {
public:
	std::map<std::string, std::ostringstream> files;
	size_t opens = 0;

	std::ostream& open(const std::string& filename) override {
		++opens;
		return files[filename];
	}

	CapturedOutput capture() const {
		return {files.at("logic_tracker.gen.h").str(), files.at("logic_tracker.gen.cpp").str()};
	}
};

} // namespace rls::transpilers::soh_tracker::test
