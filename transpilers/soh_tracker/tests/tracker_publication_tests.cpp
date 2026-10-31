#include <gtest/gtest.h>

#include <array>
#include <map>
#include <sstream>
#include <stdexcept>

#include "parser.h"
#include "sema.h"
#include "soh_tracker.h"
#include "tracker_emission.h"
#include "tracker_lowering.h"

namespace rls::transpilers::soh_tracker {
namespace {

class MemoryWriter : public rls::OutputWriter {
public:
	std::map<std::string, std::ostringstream> files;
	size_t opens = 0;

	std::ostream& open(const std::string& filename) override {
		++opens;
		return files[filename];
	}
};

enum class Failure { Open, Write };

class FailingBuffer : public std::stringbuf {
public:
	bool failWrite = false;

	std::streamsize xsputn(const char* data, std::streamsize size) override {
		if (failWrite) return size == 0 ? 0 : size - 1;
		return std::stringbuf::xsputn(data, size);
	}

};

class FailingWriter : public rls::OutputWriter {
public:
	FailingWriter(Failure failure, size_t position) : failure(failure), position(position) {}

	std::ostream& open(const std::string&) override {
		const auto index = opens++;
		if (index == position) {
			if (failure == Failure::Open) throw std::runtime_error("injected open error");
			buffer.failWrite = failure == Failure::Write;
			stream.exceptions(std::ios::badbit | std::ios::failbit);
		}
		return stream;
	}

	size_t opens = 0;

private:
	Failure failure;
	size_t position;
	FailingBuffer buffer;
	std::ostream stream{&buffer};
};

class SohTrackerPublication : public testing::Test {
protected:
	const std::filesystem::path sourceRoot =
		std::filesystem::temp_directory_path() / "rls_tracker_publication";
	ast::Project project;

	void analyze(const std::string& source) {
		project.files.push_back(parser::ParseString(source, (sourceRoot / "source.rls").string()));
		for (const auto& error : project.files.back().diagnostics) {
			ASSERT_NE(error.level, ast::DiagnosticLevel::Error) << error.message;
		}
		for (const auto& error : sema::analyze(project)) {
			ASSERT_NE(error.level, ast::DiagnosticLevel::Error) << error.message;
		}
	}

	void expectUntouched(const MemoryWriter& writer) const {
		EXPECT_EQ(writer.opens, 0);
		ASSERT_EQ(writer.files.size(), 1);
		EXPECT_EQ(writer.files.at("logic_tracker.gen.h").str(), "prior successful artifact");
	}
};

TEST_F(SohTrackerPublication, PublicApiWritesAllArtifacts) {
	analyze(
		"extern enum Custom { CUSTOM_* }\n"
		"extern define host(value: Custom) -> Bool\n"
		"region RR_TEST { locations { RC_A: host(CUSTOM_ONE) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const std::vector<HostEnumBinding> bindings{{"Custom", "HostCustom"}};
	const auto lowered = ir::LowerRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<ir::Graph>(lowered));
	const auto rendered = ir::RenderArtifacts(project, std::get<ir::Graph>(lowered), sourceRoot, bindings);
	ASSERT_TRUE(std::holds_alternative<ir::RenderedArtifacts>(rendered));
	const auto& expected = std::get<ir::RenderedArtifacts>(rendered);
	MemoryWriter writer;
	const auto& analyzedProject = project;
	const auto diagnostics = SohTrackerTranspiler(analyzedProject).Transpile(writer, sourceRoot, bindings);
	EXPECT_TRUE(diagnostics.empty());
	EXPECT_EQ(writer.opens, 3);
	ASSERT_EQ(writer.files.size(), 3);
	EXPECT_EQ(writer.files.at("logic_tracker.gen.h").str(), expected.header);
	EXPECT_EQ(writer.files.at("logic_tracker.gen.cpp").str(), expected.source);
	EXPECT_EQ(writer.files.at("logic_tracker.gen.txt").str(), expected.manifest);
}

TEST_F(SohTrackerPublication, FreeApiSupportsEmptyProjectsAndOpenOnlyMemoryWriters) {
	struct OpenOnlyWriter : rls::OutputWriter {
		std::map<std::string, std::ostringstream> files;
		std::ostream& open(const std::string& name) override { return files[name]; }
	} writer;
	EXPECT_TRUE(Transpile(project, writer, sourceRoot).empty());
	ASSERT_EQ(writer.files.size(), 3);
	EXPECT_NE(writer.files.at("logic_tracker.gen.cpp").str().find("return 0ULL;"), std::string::npos);
	EXPECT_EQ(writer.files.at("logic_tracker.gen.txt").str(), "nodes 0\nroots 0\n");
}

TEST_F(SohTrackerPublication, LoweringAndPreflightErrorsNeverOpenOutputs) {
	analyze("extern enum Custom { CUSTOM_* }\n"
		"region RR_TEST { locations { RC_A: CUSTOM_ONE == CUSTOM_ONE } }\n");
	ASSERT_FALSE(HasFatalFailure());
	for (const bool invalidRoot : {false, true}) {
		SCOPED_TRACE(invalidRoot);
		MemoryWriter writer;
		writer.files["logic_tracker.gen.h"] << "prior successful artifact";
		const auto diagnostics = Transpile(project, writer, invalidRoot ? "relative" : sourceRoot);
		ASSERT_FALSE(diagnostics.empty());
		EXPECT_EQ(diagnostics[0].level, ast::DiagnosticLevel::Error);
		expectUntouched(writer);
	}
}

TEST_F(SohTrackerPublication, MissingAnalyzedMetadataNeverOpensOutputs) {
	project.files.push_back(parser::ParseString(
		"region RR_TEST { locations { RC_A: true } }\n", (sourceRoot / "source.rls").string()));
	MemoryWriter writer;
	writer.files["logic_tracker.gen.h"] << "prior successful artifact";
	EXPECT_FALSE(Transpile(project, writer, sourceRoot).empty());
	expectUntouched(writer);
}

TEST_F(SohTrackerPublication, InvalidGraphsAndLateRenderingErrorsNeverOpenOutputs) {
	analyze("region RR_TEST { locations { RC_A: true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto lowered = ir::LowerRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<ir::Graph>(lowered));
	for (const bool renderError : {false, true}) {
		SCOPED_TRACE(renderError);
		auto graph = std::get<ir::Graph>(lowered);
		if (renderError) {
			// Root source identity is intact; the listing cannot normalize this node's path.
			graph.nodes[0].provenance.span.file.clear();
			ASSERT_TRUE(ir::ValidateGraph(project, graph, sourceRoot).empty());
		} else {
			graph.nodes.push_back({ir::Not{ir::NodeHandle{999}}, graph.nodes[0].valueType,
				graph.nodes[0].provenance});
		}
		MemoryWriter writer;
		writer.files["logic_tracker.gen.h"] << "prior successful artifact";
		const auto diagnostics = ir::EmitArtifacts(project, graph, writer, sourceRoot);
		ASSERT_FALSE(diagnostics.empty());
		EXPECT_EQ(diagnostics[0].level, ast::DiagnosticLevel::Error);
		expectUntouched(writer);
	}
}

TEST_F(SohTrackerPublication, PropagatesWriterExceptionsForEveryArtifact) {
	analyze("region RR_TEST { locations { RC_A: true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const std::array<std::string, 3> filenames{
		"logic_tracker.gen.h", "logic_tracker.gen.cpp", "logic_tracker.gen.txt"};
	for (const auto failure : {Failure::Open, Failure::Write}) {
		for (size_t position = 0; position < filenames.size(); ++position) {
			SCOPED_TRACE(static_cast<int>(failure));
			SCOPED_TRACE(filenames[position]);
			FailingWriter writer(failure, position);
			EXPECT_THROW(Transpile(project, writer, sourceRoot), std::runtime_error);
			EXPECT_EQ(writer.opens, position + 1);
		}
	}
}

TEST_F(SohTrackerPublication, WriterAndStreamsRemainOwnedByTheCaller) {
	analyze("region RR_TEST { locations { RC_A: true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	MemoryWriter writer;
	EXPECT_TRUE(Transpile(project, writer, sourceRoot).empty());
	EXPECT_EQ(writer.opens, 3);
	auto& stream = writer.files.at("logic_tracker.gen.txt");
	const auto generated = stream.str();
	stream << "caller-owned content\n";
	EXPECT_EQ(stream.str(), generated + "caller-owned content\n");
	writer.open("caller.txt") << "still usable";
	EXPECT_EQ(writer.files.at("caller.txt").str(), "still usable");
}

} // namespace
} // namespace rls::transpilers::soh_tracker
