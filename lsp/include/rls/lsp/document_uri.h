#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace rls::lsp {

std::optional<std::string> NormalizeDocumentUri(std::string_view uri);
std::optional<std::string> DocumentUriKey(std::string_view uri);

} // namespace rls::lsp