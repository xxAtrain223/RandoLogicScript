#include "rls/lsp/outbound_message_queue.h"

#include <utility>

namespace rls::lsp {

bool OutboundMessageQueue::push(std::string payload) {
    {
        std::lock_guard lock(mutex_);
        if (closed_) {
            return false;
        }
        messages_.push_back(std::move(payload));
    }
    ready_.notify_one();
    return true;
}

std::optional<std::string> OutboundMessageQueue::tryPop() {
    std::lock_guard lock(mutex_);
    if (messages_.empty()) {
        return std::nullopt;
    }
    std::string payload = std::move(messages_.front());
    messages_.pop_front();
    return payload;
}

std::optional<std::string> OutboundMessageQueue::waitPop() {
    std::unique_lock lock(mutex_);
    ready_.wait(lock, [this] { return closed_ || !messages_.empty(); });
    if (messages_.empty()) {
        return std::nullopt;
    }
    std::string payload = std::move(messages_.front());
    messages_.pop_front();
    return payload;
}

void OutboundMessageQueue::close() {
    {
        std::lock_guard lock(mutex_);
        closed_ = true;
    }
    ready_.notify_all();
}

} // namespace rls::lsp