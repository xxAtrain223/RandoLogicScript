#include <gtest/gtest.h>

#include "language_server.h"

namespace {

using rls::lsp::LanguageServer;
using rls::lsp::TextDocumentSyncKind;

TEST(LanguageServerTests, DefaultCapabilitiesMatchPhaseOneScope) {
    const LanguageServer server;
    const auto& caps = server.capabilities();

    EXPECT_EQ(caps.textDocumentSync, TextDocumentSyncKind::Full);
    EXPECT_TRUE(caps.definitionProvider);
    EXPECT_TRUE(caps.referencesProvider);
    EXPECT_TRUE(caps.hoverProvider);
    EXPECT_TRUE(caps.completionProvider);
    EXPECT_TRUE(caps.documentSymbolProvider);
    EXPECT_TRUE(caps.workspaceSymbolProvider);
}

TEST(LanguageServerTests, ExposesDocumentStore) {
    LanguageServer server;
    server.documents().open("file:///x.rls", "rls", 1, "define x(): true");

    const auto* doc = server.documents().find("file:///x.rls");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 1);
}

} // namespace
