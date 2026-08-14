#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include "project.h"

namespace rls::project::diagnostics {

inline ConfigurationDiagnostic ManifestUnavailable(
    std::filesystem::path path, std::string_view requestedPath) {
    return {std::move(path), "RLS-C001",
        "could not open manifest: " + std::string(requestedPath), 0, 0,
        ConfigurationDiagnosticData{1, "rls.openManifest", {std::string(requestedPath)}}};
}

inline ConfigurationDiagnostic InvalidJson(
    std::filesystem::path path, std::string_view detail,
    size_t startByte, size_t endByte) {
    return {std::move(path), "RLS-C002",
        "invalid JSON: " + std::string(detail), startByte, endByte,
        ConfigurationDiagnosticData{1, "rls.fixManifestJson", {std::string(detail)}}};
}

inline ConfigurationDiagnostic ManifestMustBeObject(std::filesystem::path path) {
    return {std::move(path), "RLS-C003", "manifest must be a JSON object", 0, 0};
}

inline ConfigurationDiagnostic UnknownManifestField(
    std::filesystem::path path, std::string_view field) {
    return {std::move(path), "RLS-C003",
        "unknown manifest field: " + std::string(field), 0, 0,
        ConfigurationDiagnosticData{1, "rls.removeManifestField", {std::string(field)}}};
}

inline ConfigurationDiagnostic UnsupportedManifestVersion(std::filesystem::path path) {
    return {std::move(path), "RLS-C003", "unsupported manifest version", 0, 0};
}

inline ConfigurationDiagnostic SourcesRequired(std::filesystem::path path) {
    return {std::move(path), "RLS-C003",
        "manifest requires a non-empty sources array", 0, 0};
}

inline ConfigurationDiagnostic SourceEntryMustBeString(std::filesystem::path path) {
    return {std::move(path), "RLS-C003", "sources entries must be strings", 0, 0};
}

inline ConfigurationDiagnostic ManifestPathMustBeRelative(
    std::filesystem::path path, std::string_view value) {
    return {std::move(path), "RLS-C003",
        "manifest paths must be relative: " + std::string(value), 0, 0};
}

inline ConfigurationDiagnostic ManifestPathEscapesRoot(
    std::filesystem::path path, std::string_view value) {
    return {std::move(path), "RLS-C003",
        "manifest path escapes the project root: " + std::string(value), 0, 0};
}

inline ConfigurationDiagnostic ExcludeMustBeArray(std::filesystem::path path) {
    return {std::move(path), "RLS-C003", "exclude must be an array", 0, 0};
}

inline ConfigurationDiagnostic ExcludeEntryMustBeString(std::filesystem::path path) {
    return {std::move(path), "RLS-C003", "exclude entries must be strings", 0, 0};
}

inline ConfigurationDiagnostic TranspilersMustBeObject(std::filesystem::path path) {
    return {std::move(path), "RLS-C003", "transpilers must be an object", 0, 0};
}

inline ConfigurationDiagnostic InvalidTranspilerConfiguration(
    std::filesystem::path path, std::string_view name) {
    return {std::move(path), "RLS-C003",
        "invalid transpiler configuration: " + std::string(name), 0, 0};
}

inline ConfigurationDiagnostic SourceCollectionFailed(
    std::filesystem::path path, std::string message) {
    ConfigurationDiagnosticData data{
        1, "rls.configureManifestSources", {message}};
    return {std::move(path), "RLS-C004", std::move(message), 0, 0,
        std::move(data)};
}

} // namespace rls::project::diagnostics