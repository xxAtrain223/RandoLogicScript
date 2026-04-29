#include <gtest/gtest.h>

#include "document_store.h"

namespace {

using rls::lsp::DocumentStore;

TEST(DocumentStoreTests, OpenStoresDocument) {
    DocumentStore store;

    store.open("file:///test.rls", "rls", 1, "define foo(): true");

    const auto* doc = store.find("file:///test.rls");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->languageId, "rls");
    EXPECT_EQ(doc->version, 1);
    EXPECT_EQ(doc->text, "define foo(): true");
    EXPECT_EQ(store.size(), 1u);
}

TEST(DocumentStoreTests, FullChangeRejectsStaleVersion) {
    DocumentStore store;
    store.open("file:///test.rls", "rls", 3, "old");

    EXPECT_FALSE(store.applyFullChange("file:///test.rls", 2, "stale"));

    const auto* doc = store.find("file:///test.rls");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 3);
    EXPECT_EQ(doc->text, "old");
}

TEST(DocumentStoreTests, FullChangeAcceptsNewerVersion) {
    DocumentStore store;
    store.open("file:///test.rls", "rls", 1, "old");

    EXPECT_TRUE(store.applyFullChange("file:///test.rls", 2, "new"));

    const auto* doc = store.find("file:///test.rls");
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->version, 2);
    EXPECT_EQ(doc->text, "new");
}

TEST(DocumentStoreTests, CloseRemovesDocument) {
    DocumentStore store;
    store.open("file:///test.rls", "rls", 1, "x");

    EXPECT_TRUE(store.close("file:///test.rls"));
    EXPECT_EQ(store.find("file:///test.rls"), nullptr);
    EXPECT_EQ(store.size(), 0u);
}

} // namespace
