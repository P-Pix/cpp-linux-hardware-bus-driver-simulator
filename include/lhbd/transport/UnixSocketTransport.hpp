#pragma once

#include "lhbd/transport/ITransport.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lhbd::transport {

class UnixSocketTransport final : public ITransport {
public:
    explicit UnixSocketTransport(std::string socketPath);
    ~UnixSocketTransport() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool write(std::span<const std::uint8_t> bytes) override;
    std::optional<std::vector<std::uint8_t>> read(std::chrono::milliseconds timeout) override;

private:
    std::string socketPath_;
    int fd_{-1};
};

} // namespace lhbd::transport
