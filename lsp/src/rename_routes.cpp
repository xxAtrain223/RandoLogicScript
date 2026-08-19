#include "rls/lsp/route_modules.h"

#include <cstdint>
#include <limits>

#include <nlohmann/json.hpp>

#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/rename_service.h"

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

const Json& requireObject(const Json& value) {
    if (!value.is_object()) throw InvalidParams("expected object parameters");
    return value;
}

uint32_t positionComponent(const Json& value) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        throw InvalidParams("position components must be integers");
    }
    const auto component = value.get<int64_t>();
    if (component < 0
        || static_cast<uint64_t>(component) > std::numeric_limits<uint32_t>::max()) {
        throw InvalidParams("invalid position component");
    }
    return static_cast<uint32_t>(component);
}

NavigationPosition requestPosition(const Json& object) {
    const auto& value = requireObject(object.at("position"));
    return {positionComponent(value.at("line")), positionComponent(value.at("character"))};
}

Json position(const NavigationPosition& value) {
    return {{"line", value.line}, {"character", value.character}};
}

Json range(const NavigationRange& value) {
    return {{"start", position(value.start)}, {"end", position(value.end)}};
}

template<typename T>
const T& requireRename(const RenameResult<T>& result) {
    if (!result.value) throw RequestFailed(std::string(RenameErrorMessage(result.error)));
    return *result.value;
}

} // namespace

void RegisterRenameRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, RenameService& rename) {
    router.registerRequest("textDocument/prepareRename", [&rename](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        return range(requireRename(rename.prepare(
            document.at("uri").get<std::string>(), requestPosition(object))));
    });
    router.registerRequest("textDocument/rename", [&lifecycle, &rename](const Json& params) {
        const auto& object = requireObject(params);
        const auto& document = requireObject(object.at("textDocument"));
        if (!object.at("newName").is_string()) throw InvalidParams("newName must be a string");
        const auto result = rename.rename(
            document.at("uri").get<std::string>(), requestPosition(object),
            object.at("newName").get<std::string>(),
            lifecycle.supportsWorkspaceDocumentChanges());
        const auto& edit = requireRename(result);
        Json documentChanges = Json::array();
        for (const auto& documentEdit : edit.documents) {
            Json textDocument = {{"uri", documentEdit.uri}};
            textDocument["version"] = documentEdit.version
                ? Json(*documentEdit.version) : Json(nullptr);
            Json edits = Json::array();
            for (const auto& textEdit : documentEdit.edits) {
                edits.push_back({
                    {"range", range(textEdit.range)},
                    {"newText", textEdit.newText},
                });
            }
            documentChanges.push_back({
                {"textDocument", std::move(textDocument)},
                {"edits", std::move(edits)},
            });
        }
        return Json{{"documentChanges", std::move(documentChanges)}};
    });
}

} // namespace rls::lsp