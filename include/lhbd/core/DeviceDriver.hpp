#pragma once

#include "lhbd/protocol/Frame.hpp"
#include "lhbd/protocol/Protocol.hpp"
#include "lhbd/transport/ITransport.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace lhbd::core {

struct DriverConfig {
    std::chrono::milliseconds timeout{200};
    int maxRetries{2};
};

struct DriverStats {
    std::uint64_t commandsSent{0};
    std::uint64_t retries{0};
    std::uint64_t crcErrors{0};
    std::uint64_t timeouts{0};
    std::uint64_t protocolErrors{0};
};

struct DeviceStatus {
    std::uint8_t mode{0};
    std::uint16_t errorFlags{0};
    float uptimeSeconds{0.0F};
};

class DeviceDriver {
public:
    DeviceDriver(std::unique_ptr<transport::ITransport> transport, DriverConfig config = {});

    bool connect();
    void disconnect();
    bool isConnected() const;

    bool ping();
    std::optional<std::uint32_t> readRegister(std::uint16_t address);
    bool writeRegister(std::uint16_t address, std::uint32_t value);
    std::optional<float> readSensor(protocol::SensorId sensor);
    std::optional<DeviceStatus> getStatus();
    bool reset();

    const DriverStats& stats() const;

private:
    std::optional<protocol::Frame> transact(protocol::Command command, const std::vector<std::uint8_t>& payload);
    std::optional<protocol::Frame> transactOnce(protocol::Command command, const std::vector<std::uint8_t>& payload, std::uint16_t sequence);
    std::uint16_t nextSequence();

    std::unique_ptr<transport::ITransport> transport_;
    DriverConfig config_;
    DriverStats stats_;
    std::uint16_t sequence_{1};
};

} // namespace lhbd::core
