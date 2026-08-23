#include "rls/lsp/document_uri.h"

#include <algorithm>
#include <cctype>

namespace rls::lsp {
namespace {

bool isSchemeCharacter(char character, bool first) {
    const auto value = static_cast<unsigned char>(character);
    return std::isalpha(value) || (!first && (std::isdigit(value)
        || character == '+' || character == '-' || character == '.'));
}

int hexValue(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

bool isUnreserved(unsigned char character) {
    return std::isalnum(character) || character == '-' || character == '.'
        || character == '_' || character == '~';
}

bool equalsIgnoringAsciiCase(std::string_view left, std::string_view right) {
    return left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a))
                == std::tolower(static_cast<unsigned char>(b));
        });
}

char upperHex(int value) {
    return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
}

bool isValidUtf8(std::string_view value) {
    for (size_t index = 0; index < value.size();) {
        const auto first = static_cast<unsigned char>(value[index]);
        if (first <= 0x7f) {
            ++index;
            continue;
        }

        size_t continuationCount = 0;
        unsigned char secondMinimum = 0x80;
        unsigned char secondMaximum = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) {
            continuationCount = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuationCount = 2;
            if (first == 0xe0) secondMinimum = 0xa0;
            if (first == 0xed) secondMaximum = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuationCount = 3;
            if (first == 0xf0) secondMinimum = 0x90;
            if (first == 0xf4) secondMaximum = 0x8f;
        } else {
            return false;
        }

        if (index + continuationCount >= value.size()) {
            return false;
        }
        const auto second = static_cast<unsigned char>(value[index + 1]);
        if (second < secondMinimum || second > secondMaximum) {
            return false;
        }
        for (size_t offset = 2; offset <= continuationCount; ++offset) {
            const auto continuation = static_cast<unsigned char>(value[index + offset]);
            if (continuation < 0x80 || continuation > 0xbf) {
                return false;
            }
        }
        index += continuationCount + 1;
    }
    return true;
}

std::optional<std::filesystem::path> canonicalPath(const std::filesystem::path& path) {
    std::error_code error;
    const auto resolved = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return resolved;
    }

    const auto absolute = std::filesystem::absolute(path, error);
    if (error) {
        return std::nullopt;
    }
    return absolute.lexically_normal();
}

} // namespace

std::optional<std::string> NormalizeDocumentUri(std::string_view uri) {
    const size_t schemeEnd = uri.find(':');
    if (schemeEnd == std::string_view::npos || schemeEnd == 0) {
        return std::nullopt;
    }
    for (size_t index = 0; index < schemeEnd; ++index) {
        if (!isSchemeCharacter(uri[index], index == 0)) {
            return std::nullopt;
        }
    }

    std::string normalized;
    normalized.reserve(uri.size());
    for (size_t index = 0; index < schemeEnd; ++index) {
        normalized.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(uri[index]))));
    }
    normalized.push_back(':');

    for (size_t index = schemeEnd + 1; index < uri.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(uri[index]);
        if (character <= 0x20 || character == 0x7f || character == '\\') {
            return std::nullopt;
        }
        if (character != '%') {
            normalized.push_back(static_cast<char>(character));
            continue;
        }
        if (index + 2 >= uri.size()) {
            return std::nullopt;
        }
        const int high = hexValue(uri[index + 1]);
        const int low = hexValue(uri[index + 2]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        const auto decoded = static_cast<unsigned char>((high << 4) | low);
        if (isUnreserved(decoded)) {
            normalized.push_back(static_cast<char>(decoded));
        } else {
            normalized.push_back('%');
            normalized.push_back(upperHex(high));
            normalized.push_back(upperHex(low));
        }
        index += 2;
    }

    if (normalized.starts_with("file:")) {
        if (!normalized.starts_with("file://")) {
            return std::nullopt;
        }
        constexpr size_t AuthorityStart = 7;
        const size_t pathStart = normalized.find('/', AuthorityStart);
        if (pathStart == std::string::npos) {
            return std::nullopt;
        }

        const std::string_view authority(
            normalized.data() + AuthorityStart, pathStart - AuthorityStart);
        if (equalsIgnoringAsciiCase(authority, "localhost")) {
            normalized.erase(AuthorityStart, authority.size());
        } else {
            std::transform(
                normalized.begin() + static_cast<std::ptrdiff_t>(AuthorityStart),
                normalized.begin() + static_cast<std::ptrdiff_t>(pathStart),
                normalized.begin() + static_cast<std::ptrdiff_t>(AuthorityStart),
                [](char character) {
                    return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                });
        }
    }
    return normalized;
}

std::optional<std::string> DocumentUriKey(std::string_view uri) {
    auto normalized = NormalizeDocumentUri(uri);
    if (!normalized) {
        return std::nullopt;
    }
    if (normalized->starts_with("file:///")) {
        const auto path = FileUriToPath(*normalized);
        if (!path) {
            return std::nullopt;
        }
        const auto canonicalUri = PathToFileUri(*path);
        if (!canonicalUri) {
            return std::nullopt;
        }
        normalized = canonicalUri;
    }
#ifdef _WIN32
    if (normalized->starts_with("file:")) {
        for (size_t index = 0; index < normalized->size(); ++index) {
            if ((*normalized)[index] == '%' && index + 2 < normalized->size()) {
                index += 2;
                continue;
            }
            (*normalized)[index] = static_cast<char>(
                std::tolower(static_cast<unsigned char>((*normalized)[index])));
        }
    }
#endif
    return normalized;
}

std::optional<std::filesystem::path> FileUriToPath(std::string_view uri) {
    const auto normalized = NormalizeDocumentUri(uri);
    if (!normalized || !normalized->starts_with("file://")) {
        return std::nullopt;
    }

    constexpr size_t AuthorityStart = 7;
    const size_t pathStart = normalized->find('/', AuthorityStart);
    if (pathStart == std::string::npos) {
        return std::nullopt;
    }

    const std::string authority = normalized->substr(
        AuthorityStart, pathStart - AuthorityStart);
    std::string path;
    path.reserve(normalized->size() - pathStart);
    for (size_t index = pathStart; index < normalized->size(); ++index) {
        if ((*normalized)[index] != '%') {
            path.push_back((*normalized)[index]);
            continue;
        }

        const int high = hexValue((*normalized)[index + 1]);
        const int low = hexValue((*normalized)[index + 2]);
        const char decoded = static_cast<char>((high << 4) | low);
        if (decoded == '\0') {
            return std::nullopt;
        }
        path.push_back(decoded);
        index += 2;
    }

    if (!authority.empty()) {
        path = "//" + authority + path;
    }
#ifdef _WIN32
    if (authority.empty() && path.size() >= 3 && path[0] == '/'
        && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':') {
        path.erase(0, 1);
    }
#endif
    if (!isValidUtf8(path)) {
        return std::nullopt;
    }
    std::u8string utf8Path;
    utf8Path.reserve(path.size());
    for (const unsigned char byte : path) {
        utf8Path.push_back(static_cast<char8_t>(byte));
    }
    const std::filesystem::path filesystemPath(utf8Path);
    return authority.empty()
        ? canonicalPath(filesystemPath)
        : std::optional<std::filesystem::path>(filesystemPath);
}

std::optional<std::string> PathToFileUri(const std::filesystem::path& path) {
    const auto canonical = canonicalPath(path);
    if (!canonical) {
        return std::nullopt;
    }

    const auto generic = canonical->generic_u8string();
    const std::string_view genericBytes(
        reinterpret_cast<const char*>(generic.data()), generic.size());
    if (!isValidUtf8(genericBytes)) {
        return std::nullopt;
    }
    std::string uri = "file://";
#ifdef _WIN32
    if (generic.size() >= 2 && generic[1] == u8':') {
        uri.push_back('/');
    }
#endif
    constexpr std::string_view Hex = "0123456789ABCDEF";
    for (const char8_t byteValue : generic) {
        const auto byte = static_cast<unsigned char>(byteValue);
        if (isUnreserved(byte) || byte == '/' || byte == ':') {
            uri.push_back(static_cast<char>(byte));
        } else {
            uri.push_back('%');
            uri.push_back(Hex[byte >> 4]);
            uri.push_back(Hex[byte & 0x0f]);
        }
    }
    return NormalizeDocumentUri(uri);
}

} // namespace rls::lsp