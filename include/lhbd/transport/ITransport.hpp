#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lhbd::transport {

class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool write(std::span<const std::uint8_t> bytes) = 0;
    virtual std::optional<std::vector<std::uint8_t>> read(std::chrono::milliseconds timeout) = 0;
};

} // namespace lhbd::transport
