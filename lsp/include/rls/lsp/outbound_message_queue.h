#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace rls::lsp {

class OutboundMessageQueue {
public:
    bool push(std::string payload);
    std::optional<std::string> tryPop();
    std::optional<std::string> waitPop();
    void close();

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::string> messages_;
    bool closed_ = false;
};

} // namespace rls::lsp