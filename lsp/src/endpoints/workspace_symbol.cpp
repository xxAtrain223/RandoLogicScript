#include "framework.h"

#include "json_bind.h"
#include "language_server.h"
#include "symbol_support.h"

namespace rls::lsp::detail::endpoints {
namespace workspace_symbol {

struct WorkspaceSymbolRequest {
    std::string query;
};

std::vector<std::string> handleWorkspaceSymbolEndpoint(
    const EndpointContext& context,
    const WorkspaceSymbolRequest& request) {
    if (!context.hasId) {
        return {};
    }

    const auto symbols = support::collectSymbols(context.server.documents());

    json results = json::array();
    for (const auto& symbol : symbols) {
        // Workspace symbol search uses the preindexed declarations from all
        // open documents and filters them with a case-insensitive substring match.
        if (!support::symbolMatchesQuery(symbol.name, request.query)) {
            continue;
        }

        results.push_back({
            {"name", symbol.name},
            {"kind", symbol.kind},
            {"location", support::locationFromSpan(symbol.uri, symbol.span)},
        });
    }

    return context.ok(results);
}

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<WorkspaceSymbolRequest>(
        "workspace/symbol",
        "Search indexed symbols across open documents",
        EndpointInvocation::RequestOnly,
        BindFailureBehavior::Ignore,
        handleWorkspaceSymbolEndpoint);
});

} // namespace workspace_symbol

template <>
struct RequestBinder<workspace_symbol::WorkspaceSymbolRequest> {
    static std::optional<workspace_symbol::WorkspaceSymbolRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        if (paramsObject == nullptr) {
            return std::nullopt;
        }

        workspace_symbol::WorkspaceSymbolRequest request;
        request.query = stringMemberOr(paramsObject, "query", "");
        return request;
    }
};

} // namespace rls::lsp::detail::endpoints
