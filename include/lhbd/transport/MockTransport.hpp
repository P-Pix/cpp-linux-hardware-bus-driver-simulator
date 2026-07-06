#pragma once

#include "lhbd/transport/ITransport.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <vector>

namespace lhbd::transport {

class MockTransport final : public ITransport {
public:
    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool write(std::span<const std::uint8_t> bytes) override;
    std::optional<std::vector<std::uint8_t>> read(std::chrono::milliseconds timeout) override;

    void pushResponse(std::vector<std::uint8_t> response);
    void pushTimeout();
    const std::vector<std::vector<std::uint8_t>>& writes() const;

private:
    bool open_{false};
    std::deque<std::optional<std::vector<std::uint8_t>>> responses_;
    std::vector<std::vector<std::uint8_t>> writes_;
};

} // namespace lhbd::transport
