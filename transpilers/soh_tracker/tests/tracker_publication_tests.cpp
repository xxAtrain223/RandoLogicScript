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
#include "tracker_test_writer.h"
#include "tracker_test_helpers.h"

namespace rls::transpilers::soh_tracker {
namespace {

using test::MemoryWriter;

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
	MemoryWriter expectedWriter;
	ASSERT_TRUE(ir::EmitArtifacts(project, std::get<ir::Graph>(lowered), expectedWriter,
		sourceRoot, bindings).empty());
	const auto expected = expectedWriter.capture();
	MemoryWriter writer;
	const auto& analyzedProject = project;
	const auto diagnostics = SohTrackerTranspiler(analyzedProject).Transpile(writer, sourceRoot, bindings);
	EXPECT_TRUE(diagnostics.empty());
	EXPECT_EQ(writer.opens, 2);
	ASSERT_EQ(writer.files.size(), 2);
	EXPECT_EQ(writer.files.at("logic_tracker.gen.h").str(), expected.header);
	EXPECT_EQ(writer.files.at("logic_tracker.gen.cpp").str(), expected.source);
}

TEST_F(SohTrackerPublication, FreeApiSupportsEmptyProjectsAndOpenOnlyMemoryWriters) {
	struct OpenOnlyWriter : rls::OutputWriter {
		std::map<std::string, std::ostringstream> files;
		std::ostream& open(const std::string& name) override { return files[name]; }
	} writer;
	EXPECT_TRUE(Transpile(project, writer, sourceRoot).empty());
	ASSERT_EQ(writer.files.size(), 2);
	EXPECT_TRUE(writer.files.contains("logic_tracker.gen.h"));
	EXPECT_NE(writer.files.at("logic_tracker.gen.cpp").str().find("return 0;"), std::string::npos);
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

TEST_F(SohTrackerPublication, InvalidGraphsAndSourcePathsFailPreflightBeforeOpeningOutputs) {
	analyze("region RR_TEST { locations { RC_A: true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto lowered = ir::LowerRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<ir::Graph>(lowered));
	for (const bool pathError : {false, true}) {
		SCOPED_TRACE(pathError);
		auto graph = std::get<ir::Graph>(lowered);
		if (pathError) {
			// Graph validation succeeds, but emission preflight must reject the missing path.
			graph.nodes[0].provenance.span.file.clear();
			ASSERT_TRUE(ir::testing_helpers::ValidateGraph(project, graph, sourceRoot).empty());
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
	const std::array<std::string, 2> filenames{
		"logic_tracker.gen.h", "logic_tracker.gen.cpp"};
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

TEST_F(SohTrackerPublication, PreflightChecksAllReachableNodePathsBeforeGeneration) {
	analyze("region RR_TEST { locations { RC_A: true and true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	auto lowered = ir::LowerRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<ir::Graph>(lowered));
	auto& graph = std::get<ir::Graph>(lowered);
	const auto& children = std::get<ir::Logical>(graph.nodes[graph.roots[0].expression.slot].payload).children;
	ASSERT_EQ(children.size(), 2);
	graph.nodes[children[0].slot].provenance.span.file.clear();
	graph.nodes[children[1].slot].provenance.span.file = sourceRoot.string();
	ASSERT_TRUE(ir::testing_helpers::ValidateGraph(project, graph, sourceRoot).empty());
	MemoryWriter writer;
	writer.files["logic_tracker.gen.h"] << "prior successful artifact";
	const auto diagnostics = ir::EmitArtifacts(project, graph, writer, sourceRoot);
	ASSERT_EQ(diagnostics.size(), 2);
	EXPECT_NE(diagnostics[0].message.find("requires a source file path"), std::string::npos);
	EXPECT_NE(diagnostics[1].message.find("cannot be made project-relative"), std::string::npos);
	expectUntouched(writer);
}

TEST_F(SohTrackerPublication, WriterAndStreamsRemainOwnedByTheCaller) {
	analyze("region RR_TEST { locations { RC_A: true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	MemoryWriter writer;
	EXPECT_TRUE(Transpile(project, writer, sourceRoot).empty());
	EXPECT_EQ(writer.opens, 2);
	auto& stream = writer.files.at("logic_tracker.gen.cpp");
	const auto generated = stream.str();
	stream << "caller-owned content\n";
	EXPECT_EQ(stream.str(), generated + "caller-owned content\n");
	writer.open("caller.txt") << "still usable";
	EXPECT_EQ(writer.files.at("caller.txt").str(), "still usable");
}

} // namespace
} // namespace rls::transpilers::soh_tracker
