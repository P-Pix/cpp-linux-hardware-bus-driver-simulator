#include "lhbd/transport/UnixSocketTransport.hpp"

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace lhbd::transport {

UnixSocketTransport::UnixSocketTransport(std::string socketPath) : socketPath_(std::move(socketPath)) {}

UnixSocketTransport::~UnixSocketTransport() {
    close();
}

bool UnixSocketTransport::open() {
    close();

    fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) {
        return false;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (socketPath_.size() >= sizeof(address.sun_path)) {
        close();
        return false;
    }
    std::strncpy(address.sun_path, socketPath_.c_str(), sizeof(address.sun_path) - 1U);

    if (::connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close();
        return false;
    }

    return true;
}

void UnixSocketTransport::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool UnixSocketTransport::isOpen() const {
    return fd_ >= 0;
}

bool UnixSocketTransport::write(std::span<const std::uint8_t> bytes) {
    if (!isOpen()) {
        return false;
    }

    std::size_t total = 0;
    while (total < bytes.size()) {
        const auto written = ::send(fd_, bytes.data() + total, bytes.size() - total, 0);
        if (written <= 0) {
            return false;
        }
        total += static_cast<std::size_t>(written);
    }
    return true;
}

std::optional<std::vector<std::uint8_t>> UnixSocketTransport::read(std::chrono::milliseconds timeout) {
    if (!isOpen()) {
        return std::nullopt;
    }

    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd_, &readFds);

    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    const int ready = ::select(fd_ + 1, &readFds, nullptr, nullptr, &tv);
    if (ready <= 0) {
        return std::nullopt;
    }

    std::uint8_t header[16]{};
    std::size_t received = 0;
    while (received < sizeof(header)) {
        const auto n = ::recv(fd_, header + received, sizeof(header) - received, 0);
        if (n <= 0) {
            return std::nullopt;
        }
        received += static_cast<std::size_t>(n);
    }

    const std::uint32_t payloadLength = (static_cast<std::uint32_t>(header[12]) << 24U) |
                                        (static_cast<std::uint32_t>(header[13]) << 16U) |
                                        (static_cast<std::uint32_t>(header[14]) << 8U) |
                                        static_cast<std::uint32_t>(header[15]);
    if (payloadLength > 4096U) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> frame;
    frame.insert(frame.end(), std::begin(header), std::end(header));
    frame.resize(16U + payloadLength + 4U);

    std::size_t remainingReceived = 0;
    const std::size_t remaining = static_cast<std::size_t>(payloadLength) + 4U;
    while (remainingReceived < remaining) {
        const auto n = ::recv(fd_, frame.data() + 16U + remainingReceived, remaining - remainingReceived, 0);
        if (n <= 0) {
            return std::nullopt;
        }
        remainingReceived += static_cast<std::size_t>(n);
    }

    return frame;
}

} // namespace lhbd::transport
