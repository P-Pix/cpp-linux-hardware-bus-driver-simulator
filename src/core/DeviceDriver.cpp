#include "lhbd/core/DeviceDriver.hpp"

#include "lhbd/protocol/FrameCodec.hpp"
#include "lhbd/utils/Logger.hpp"

#include <sstream>
#include <utility>

namespace lhbd::core {

using lhbd::protocol::Command;
using lhbd::protocol::Frame;
using lhbd::protocol::FrameCodec;
using lhbd::protocol::Status;
using lhbd::utils::Logger;

DeviceDriver::DeviceDriver(std::unique_ptr<transport::ITransport> transport, DriverConfig config)
    : transport_(std::move(transport)), config_(config) {}

bool DeviceDriver::connect() {
    if (transport_ == nullptr) {
        Logger::instance().error("driver has no transport");
        return false;
    }
    const bool ok = transport_->open();
    if (ok) {
        Logger::instance().info("device transport opened");
    } else {
        Logger::instance().error("failed to open device transport");
    }
    return ok;
}

void DeviceDriver::disconnect() {
    if (transport_ != nullptr) {
        transport_->close();
    }
}

bool DeviceDriver::isConnected() const {
    return transport_ != nullptr && transport_->isOpen();
}

bool DeviceDriver::ping() {
    const auto response = transact(Command::Ping, {});
    return response.has_value() && response->status == Status::Ok;
}

std::optional<std::uint32_t> DeviceDriver::readRegister(std::uint16_t address) {
    std::vector<std::uint8_t> payload;
    FrameCodec::appendU16(payload, address);

    const auto response = transact(Command::ReadRegister, payload);
    if (!response.has_value() || response->status != Status::Ok || response->payload.size() != 4U) {
        return std::nullopt;
    }
    return FrameCodec::readU32(response->payload, 0);
}

bool DeviceDriver::writeRegister(std::uint16_t address, std::uint32_t value) {
    std::vector<std::uint8_t> payload;
    FrameCodec::appendU16(payload, address);
    FrameCodec::appendU32(payload, value);

    const auto response = transact(Command::WriteRegister, payload);
    return response.has_value() && response->status == Status::Ok;
}

std::optional<float> DeviceDriver::readSensor(protocol::SensorId sensor) {
    std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(sensor)};
    const auto response = transact(Command::ReadSensor, payload);
    if (!response.has_value() || response->status != Status::Ok || response->payload.size() != 4U) {
        return std::nullopt;
    }
    return FrameCodec::readF32(response->payload, 0);
}

std::optional<DeviceStatus> DeviceDriver::getStatus() {
    const auto response = transact(Command::GetStatus, {});
    if (!response.has_value() || response->status != Status::Ok || response->payload.size() != 7U) {
        return std::nullopt;
    }

    DeviceStatus status;
    status.mode = response->payload[0];
    status.errorFlags = FrameCodec::readU16(response->payload, 1);
    status.uptimeSeconds = FrameCodec::readF32(response->payload, 3);
    return status;
}

bool DeviceDriver::reset() {
    const auto response = transact(Command::Reset, {});
    return response.has_value() && response->status == Status::Ok;
}

const DriverStats& DeviceDriver::stats() const {
    return stats_;
}

std::optional<Frame> DeviceDriver::transact(Command command, const std::vector<std::uint8_t>& payload) {
    const std::uint16_t sequence = nextSequence();

    for (int attempt = 0; attempt <= config_.maxRetries; ++attempt) {
        if (attempt > 0) {
            ++stats_.retries;
            std::ostringstream oss;
            oss << "retrying command " << protocol::toString(command) << " attempt=" << attempt;
            Logger::instance().warn(oss.str());
        }

        auto response = transactOnce(command, payload, sequence);
        if (response.has_value()) {
            return response;
        }
    }

    return std::nullopt;
}

std::optional<Frame> DeviceDriver::transactOnce(Command command, const std::vector<std::uint8_t>& payload, std::uint16_t sequence) {
    if (!isConnected()) {
        Logger::instance().error("cannot transact: transport is not connected");
        return std::nullopt;
    }

    Frame request;
    request.command = command;
    request.status = Status::Ok;
    request.sequence = sequence;
    request.payload = payload;

    const auto encoded = FrameCodec::encode(request);
    if (encoded.empty() || !transport_->write(encoded)) {
        Logger::instance().error("failed to write request frame");
        ++stats_.protocolErrors;
        return std::nullopt;
    }

    ++stats_.commandsSent;
    const auto bytes = transport_->read(config_.timeout);
    if (!bytes.has_value()) {
        ++stats_.timeouts;
        Logger::instance().warn("device transaction timed out");
        return std::nullopt;
    }

    std::string error;
    const auto decoded = FrameCodec::decode(*bytes, &error);
    if (!decoded.has_value()) {
        ++stats_.protocolErrors;
        Logger::instance().error("invalid response frame: " + error);
        return std::nullopt;
    }

    if (!decoded->crcValid) {
        ++stats_.crcErrors;
        Logger::instance().warn("response CRC check failed");
        return std::nullopt;
    }

    if (decoded->frame.sequence != sequence) {
        ++stats_.protocolErrors;
        Logger::instance().warn("response sequence mismatch");
        return std::nullopt;
    }

    return decoded->frame;
}

std::uint16_t DeviceDriver::nextSequence() {
    const std::uint16_t current = sequence_;
    ++sequence_;
    if (sequence_ == 0U) {
        sequence_ = 1U;
    }
    return current;
}

} // namespace lhbd::core
