#include "framework.h"

#include <string>
#include <variant>

#include "ast.h"
#include "json_bind.h"
#include "language_server.h"
#include "parser.h"

namespace rls::lsp::detail::endpoints {
namespace document_symbol {

struct TextDocumentRequest {
    std::string uri;
};

int symbolKindForDecl(const rls::ast::Decl& decl) {
    return std::visit([](const auto& d) -> int {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, rls::ast::DefineDecl> || std::is_same_v<T, rls::ast::ExternDefineDecl>) {
            return 12; // Function
        }
        if constexpr (std::is_same_v<T, rls::ast::RegionDecl> || std::is_same_v<T, rls::ast::ExtendRegionDecl>) {
            return 5; // Class
        }
        return 13; // Variable
    }, decl);
}

std::string symbolNameForDecl(const rls::ast::Decl& decl) {
    return std::visit([](const auto& d) -> std::string {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, rls::ast::RegionDecl>) {
            return d.key;
        }
        if constexpr (std::is_same_v<T, rls::ast::ExtendRegionDecl>) {
            return d.name;
        }
        if constexpr (std::is_same_v<T, rls::ast::DefineDecl>) {
            return d.name;
        }
        if constexpr (std::is_same_v<T, rls::ast::ExternDefineDecl>) {
            return d.name;
        }
        return {};
    }, decl);
}

std::vector<std::string> handleDocumentSymbolEndpoint(
    const EndpointContext& context,
    const TextDocumentRequest& request) {
    if (!context.hasId) {
        return {};
    }

    const TextDocument* doc = context.server.documents().find(request.uri);
    if (doc == nullptr) {
        return context.ok(json::array());
    }

    // Reparse the current open snapshot and surface only top-level declarations
    // as flat document symbols.
    auto file = rls::parser::ParseString(doc->text, request.uri);
    json symbols = json::array();

    for (const auto& decl : file.declarations) {
        const std::string name = symbolNameForDecl(decl);
        const int kind = symbolKindForDecl(decl);
        const rls::ast::Span span = std::visit([](const auto& d) {
            return d.span;
        }, decl);

        symbols.push_back({
            {"name", name},
            {"kind", kind},
            {"range", toLspRange(span)},
            // The parser tracks declaration spans, not narrower identifier-only
            // spans, so selectionRange matches the declaration range here.
            {"selectionRange", toLspRange(span)},
        });
    }

    return context.ok(symbols);
}

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<TextDocumentRequest>(
        "textDocument/documentSymbol",
        "Return top-level symbols for a document",
        EndpointInvocation::RequestOnly,
        BindFailureBehavior::Ignore,
        handleDocumentSymbolEndpoint);
});

} // namespace document_symbol

template <>
struct RequestBinder<document_symbol::TextDocumentRequest> {
    static std::optional<document_symbol::TextDocumentRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        const json* textDocument = findObjectMember(paramsObject, "textDocument");
        if (textDocument == nullptr) {
            return std::nullopt;
        }

        document_symbol::TextDocumentRequest request;
        request.uri = normalizeUri(stringMemberOr(textDocument, "uri", ""));
        return request;
    }
};

} // namespace rls::lsp::detail::endpoints
