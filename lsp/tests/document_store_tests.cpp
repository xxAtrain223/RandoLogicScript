#include <gtest/gtest.h>

#include "rls/lsp/document_store.h"
#include "rls/lsp/document_uri.h"

namespace fs = std::filesystem;

namespace {

using rls::lsp::DocumentStore;
using rls::lsp::DocumentUpdateResult;
using rls::lsp::FileUriToPath;
using rls::lsp::NormalizeDocumentUri;
using rls::lsp::PathToFileUri;

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

TEST(DocumentUriTests, ConvertsEscapedFileUrisToPaths) {
#ifdef _WIN32
    EXPECT_EQ(FileUriToPath("file:///C:/logic/My%20File.rls"),
        std::filesystem::path("C:/logic/My File.rls"));
    EXPECT_EQ(FileUriToPath("file://server/share/main.rls"),
        std::filesystem::path("//server/share/main.rls"));
#else
    EXPECT_EQ(FileUriToPath("file:///logic/My%20File.rls"),
        std::filesystem::path("/logic/My File.rls"));
#endif
    EXPECT_FALSE(FileUriToPath("https://example.com/main.rls").has_value());
    EXPECT_FALSE(FileUriToPath("file:///logic/bad%C3%28.rls").has_value());
}

TEST(DocumentUriTests, RoundTripsFilesystemPathsThroughFileUris) {
    const fs::path path = fs::temp_directory_path() / "RLS URI" / "main.rls";
    const auto uri = PathToFileUri(path);
    ASSERT_TRUE(uri.has_value());
    const auto roundTrip = FileUriToPath(*uri);
    ASSERT_TRUE(roundTrip.has_value());
    EXPECT_EQ(roundTrip->lexically_normal(), fs::absolute(path).lexically_normal());
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