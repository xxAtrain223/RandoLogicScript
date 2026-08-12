#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"

namespace {

using rls::lsp::DocumentStore;
using rls::lsp::DocumentUpdateResult;
using rls::lsp::NormalizeDocumentUri;

TEST(DocumentUriTests, NormalizesSchemeEscapesAndLocalhost) {
    EXPECT_EQ(NormalizeDocumentUri("FILE:///Logic%2fMain%2Erls"),
        "file:///Logic%2FMain.rls");
    EXPECT_EQ(NormalizeDocumentUri("file://localhost/work/main.rls"),
        "file:///work/main.rls");
    EXPECT_EQ(NormalizeDocumentUri("file://SERVER/share/main.rls"),
        "file://server/share/main.rls");
}

TEST(DocumentUriTests, RejectsMalformedUris) {
    EXPECT_FALSE(NormalizeDocumentUri("C:\\logic\\main.rls").has_value());
    EXPECT_FALSE(NormalizeDocumentUri("file:///bad%2").has_value());
    EXPECT_FALSE(NormalizeDocumentUri("file://relative").has_value());
}

TEST(DocumentStoreTests, StoresDocumentsUnderNormalizedUris) {
    DocumentStore store;
    EXPECT_EQ(store.open("FILE:///work/My%2Erls", "rls", 1, "old"),
        DocumentUpdateResult::Applied);

    const auto* document = store.find("file:///work/My.rls");
    ASSERT_NE(document, nullptr);
    EXPECT_EQ(document->uri, "file:///work/My.rls");
    EXPECT_EQ(document->text, "old");
}

TEST(DocumentStoreTests, RequiresStrictlyIncreasingVersions) {
    DocumentStore store;
    ASSERT_EQ(store.open("file:///work/main.rls", "rls", 3, "current"),
        DocumentUpdateResult::Applied);

    EXPECT_EQ(store.applyFullChange("file:///work/main.rls", 3, "same"),
        DocumentUpdateResult::StaleVersion);
    EXPECT_EQ(store.applyFullChange("file:///work/main.rls", 2, "older"),
        DocumentUpdateResult::StaleVersion);
    EXPECT_EQ(store.applyFullChange("file:///work/main.rls", 4, "newer"),
        DocumentUpdateResult::Applied);
    EXPECT_EQ(store.find("file:///work/main.rls")->text, "newer");
}

TEST(DocumentStoreTests, ReportsInvalidUnknownAndClosedDocuments) {
    DocumentStore store;
    EXPECT_EQ(store.open("not a uri", "rls", 1, "text"),
        DocumentUpdateResult::InvalidUri);
    EXPECT_EQ(store.applyFullChange("file:///missing.rls", 2, "text"),
        DocumentUpdateResult::NotOpen);

    ASSERT_EQ(store.open("file:///open.rls", "rls", 1, "text"),
        DocumentUpdateResult::Applied);
    EXPECT_TRUE(store.close("file:///open.rls"));
    EXPECT_EQ(store.size(), 0);
}

} // namespace