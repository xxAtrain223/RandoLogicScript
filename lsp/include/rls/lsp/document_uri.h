#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace rls::lsp {

std::optional<std::string> NormalizeDocumentUri(std::string_view uri);
std::optional<std::string> DocumentUriKey(std::string_view uri);
std::optional<std::filesystem::path> FileUriToPath(std::string_view uri);
std::optional<std::string> PathToFileUri(const std::filesystem::path& path);

} // namespace rls::lsp