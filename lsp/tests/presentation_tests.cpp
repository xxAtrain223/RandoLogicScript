#include <gtest/gtest.h>

#include "rls/lsp/presentation.h"

namespace {

using rls::lsp::DocumentationBlock;
using rls::lsp::PresentationCallable;
using rls::lsp::PresentationLocation;
using rls::lsp::PresentationParameter;
using rls::lsp::PresentationProvenance;
using rls::lsp::PresentationRenderer;
using rls::lsp::PresentationSymbol;
using rls::lsp::PresentationSymbolKind;
using rls::lsp::PresentationType;

TEST(PresentationRendererTests, RendersCallableTypesDefaultsAndDocumentation) {
    PresentationSymbol symbol{
        .kind = PresentationSymbolKind::Function,
        .name = "can_use",
        .provenance = PresentationProvenance::Source,
        .callable = PresentationCallable{
            .name = "can_use",
            .parameters = {
                PresentationParameter{
                    .name = "item",
                    .type = PresentationType{.name = "Enum", .enumIdentity = "Item"},
                },
                PresentationParameter{
                    .name = "distance",
                    .type = PresentationType{.name = "Int"},
                    .defaultValue = "0",
                    .optional = true,
                },
            },
            .returnType = PresentationType{.name = "Bool"},
        },
        .documentation = {
            DocumentationBlock{.heading = "Usage", .markdown = "Checks whether an item is usable."},
        },
    };

    const auto rendered = PresentationRenderer{}.render(symbol);

    EXPECT_EQ(rendered.detail, "can_use(item: Item, distance: Int = 0) -> Bool");
    EXPECT_EQ(rendered.documentation,
        "**Usage**\n\nChecks whether an item is usable.");
}

TEST(PresentationRendererTests, RendersProvenanceWithoutEmbeddingSourceLocation) {
    PresentationSymbol symbol{
        .kind = PresentationSymbolKind::EnumMember,
        .name = "RG_HOOKSHOT",
        .provenance = PresentationProvenance::Extern,
        .type = PresentationType{.name = "Enum", .enumIdentity = "Item"},
        .declaration = PresentationLocation{
            .uri = "file:///project/extern.rls",
            .range = {{4, 2}, {4, 13}},
        },
    };

    const auto rendered = PresentationRenderer{}.render(symbol);

    EXPECT_EQ(rendered.detail, "extern RG_HOOKSHOT: Item");
    EXPECT_EQ(rendered.documentation, "*External declaration.*");
    EXPECT_EQ(rendered.detail.find("extern.rls"), std::string::npos);
    ASSERT_TRUE(symbol.declaration);
    EXPECT_EQ(symbol.declaration->range.start.line, 4u);
}

TEST(PresentationRendererTests, DistinguishesBuiltInAndPatternProvenance) {
    PresentationSymbol builtIn{
        .name = "true",
        .provenance = PresentationProvenance::BuiltIn,
        .type = PresentationType{.name = "Bool"},
    };
    PresentationSymbol pattern{
        .name = "RG_*",
        .provenance = PresentationProvenance::Pattern,
        .type = PresentationType{.name = "Enum", .enumIdentity = "Item"},
    };

    const auto renderedBuiltIn = PresentationRenderer{}.render(builtIn);
    const auto renderedPattern = PresentationRenderer{}.render(pattern);

    EXPECT_EQ(renderedBuiltIn.detail, "built-in true: Bool");
    EXPECT_EQ(renderedBuiltIn.documentation, "*Built-in symbol.*");
    EXPECT_EQ(renderedPattern.detail, "extern pattern RG_*: Item");
    EXPECT_EQ(renderedPattern.documentation,
        "*External pattern; no source declaration.*");
}

} // namespace