#include <stdexcept>

#include <gtest/gtest.h>

#include "rls/lsp/lifecycle_service.h"

namespace {

using rls::lsp::LifecycleService;

TEST(LifecycleServiceTests, AcceptsDocumentsOnlyAfterInitialized) {
    LifecycleService lifecycle;
    EXPECT_FALSE(lifecycle.acceptsDocumentUpdates());

    lifecycle.initialize();
    EXPECT_FALSE(lifecycle.acceptsDocumentUpdates());

    lifecycle.initialized();
    EXPECT_TRUE(lifecycle.acceptsDocumentUpdates());

    lifecycle.shutdown();
    EXPECT_FALSE(lifecycle.acceptsDocumentUpdates());
}

TEST(LifecycleServiceTests, RejectsDuplicateLifecycleTransitions) {
    LifecycleService lifecycle;
    lifecycle.initialize();
    EXPECT_THROW(lifecycle.initialize(), std::logic_error);

    lifecycle.initialized();
    EXPECT_THROW(lifecycle.initialized(), std::logic_error);

    lifecycle.shutdown();
    EXPECT_THROW(lifecycle.shutdown(), std::logic_error);
}

TEST(LifecycleServiceTests, ExitCodeReflectsCleanShutdown) {
    LifecycleService earlyExit;
    earlyExit.exit();
    EXPECT_TRUE(earlyExit.shouldExit());
    EXPECT_EQ(earlyExit.exitCode(), 1);

    LifecycleService cleanExit;
    cleanExit.initialize();
    cleanExit.shutdown();
    cleanExit.exit();
    EXPECT_EQ(cleanExit.exitCode(), 0);
}

TEST(LifecycleServiceTests, StoresNegotiatedCompletionSnippetSupport) {
    LifecycleService unsupported;
    unsupported.initialize();
    EXPECT_FALSE(unsupported.supportsCompletionSnippets());

    LifecycleService supported;
    supported.initialize(false, false, true);
    EXPECT_TRUE(supported.supportsCompletionSnippets());
}

TEST(LifecycleServiceTests, DefaultsSectionSnippetIndentationToServer) {
    LifecycleService lifecycle;
    lifecycle.initialize();
    EXPECT_EQ(lifecycle.sectionSnippetIndentation(),
        rls::lsp::SectionSnippetIndentation::Server);

    LifecycleService clientIndented;
    clientIndented.initialize(false, false, true,
        rls::lsp::SectionSnippetIndentation::Client);
    EXPECT_EQ(clientIndented.sectionSnippetIndentation(),
        rls::lsp::SectionSnippetIndentation::Client);
}

} // namespace