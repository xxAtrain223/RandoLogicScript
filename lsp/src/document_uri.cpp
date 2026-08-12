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

} // namespace rls::lsp