#include "lhbd/transport/MockTransport.hpp"

#include <thread>

namespace lhbd::transport {

bool MockTransport::open() {
    open_ = true;
    return true;
}

void MockTransport::close() {
    open_ = false;
}

bool MockTransport::isOpen() const {
    return open_;
}

bool MockTransport::write(std::span<const std::uint8_t> bytes) {
    if (!open_) {
        return false;
    }
    writes_.emplace_back(bytes.begin(), bytes.end());
    return true;
}

std::optional<std::vector<std::uint8_t>> MockTransport::read(std::chrono::milliseconds timeout) {
    if (!open_) {
        return std::nullopt;
    }
    if (responses_.empty()) {
        std::this_thread::sleep_for(timeout);
        return std::nullopt;
    }
    auto response = responses_.front();
    responses_.pop_front();
    if (!response.has_value()) {
        std::this_thread::sleep_for(timeout);
    }
    return response;
}

void MockTransport::pushResponse(std::vector<std::uint8_t> response) {
    responses_.push_back(std::move(response));
}

void MockTransport::pushTimeout() {
    responses_.push_back(std::nullopt);
}

const std::vector<std::vector<std::uint8_t>>& MockTransport::writes() const {
    return writes_;
}

} // namespace lhbd::transport
