#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "output.h"

class DirectoryWriter : public rls::OutputWriter {
public:
    explicit DirectoryWriter(std::filesystem::path dir) : dir_(std::move(dir)) {}

    std::ostream& open(const std::string& filename) override {
        const auto path = dir_ / filename;
        auto stream = std::make_unique<std::ofstream>(path);
        if (!*stream)
            throw std::ios_base::failure("could not open output '" + path.string() + "'");
        stream->exceptions(std::ios::badbit | std::ios::failbit);
        streams_.push_back({path, std::move(stream)});
        return *streams_.back().stream;
    }

    void finish() {
        for (auto& output : streams_) {
            try {
                output.stream->flush();
                output.stream->close();
            } catch (const std::ios_base::failure& error) {
                throw std::ios_base::failure(
                    "could not complete output '" + output.path.string() + "': " + error.what());
            }
        }
    }

private:
    struct Output {
        std::filesystem::path path;
        std::unique_ptr<std::ofstream> stream;
    };

    std::filesystem::path dir_;
    std::vector<Output> streams_;
};
