#include "lhbd/core/DeviceDriver.hpp"
#include "lhbd/protocol/FrameCodec.hpp"
#include "lhbd/transport/MockTransport.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << message << '\n';
    } else {
        std::cout << "[ OK ] " << message << '\n';
    }
}

std::vector<std::uint8_t> makeResponse(lhbd::protocol::Command command,
                                       lhbd::protocol::Status status,
                                       std::uint16_t sequence,
                                       const std::vector<std::uint8_t>& payload) {
    lhbd::protocol::Frame frame;
    frame.command = command;
    frame.status = status;
    frame.sequence = sequence;
    frame.payload = payload;
    return lhbd::protocol::FrameCodec::encode(frame);
}

void testFrameRoundTrip() {
    lhbd::protocol::Frame frame;
    frame.command = lhbd::protocol::Command::ReadSensor;
    frame.status = lhbd::protocol::Status::Ok;
    frame.sequence = 42;
    frame.payload = {0x01, 0x02, 0x03};

    const auto bytes = lhbd::protocol::FrameCodec::encode(frame);
    std::string error;
    const auto decoded = lhbd::protocol::FrameCodec::decode(bytes, &error);
    require(decoded.has_value(), "frame decodes after encoding");
    require(decoded->crcValid, "frame CRC is valid");
    require(decoded->frame.sequence == 42, "frame sequence round-trips");
    require(decoded->frame.payload == frame.payload, "frame payload round-trips");
}

void testCrcCorruptionDetection() {
    lhbd::protocol::Frame frame;
    frame.command = lhbd::protocol::Command::Ping;
    frame.status = lhbd::protocol::Status::Ok;
    frame.sequence = 1;
    auto bytes = lhbd::protocol::FrameCodec::encode(frame);
    bytes[8] ^= 0xFFU;

    std::string error;
    const auto decoded = lhbd::protocol::FrameCodec::decode(bytes, &error);
    require(decoded.has_value(), "corrupted frame is structurally decodable");
    require(!decoded->crcValid, "corrupted frame has invalid CRC");
}

void testDriverPing() {
    auto mock = std::make_unique<lhbd::transport::MockTransport>();
    auto* mockPtr = mock.get();
    mockPtr->pushResponse(makeResponse(lhbd::protocol::Command::Ping, lhbd::protocol::Status::Ok, 1, {}));

    lhbd::core::DeviceDriver driver(std::move(mock), {.timeout = std::chrono::milliseconds(1), .maxRetries = 0});
    require(driver.connect(), "driver connects to mock transport");
    require(driver.ping(), "driver ping succeeds");
    require(driver.stats().commandsSent == 1, "driver sent one command");
}

void testReadWriteRegister() {
    auto mock = std::make_unique<lhbd::transport::MockTransport>();
    auto* mockPtr = mock.get();
    mockPtr->pushResponse(makeResponse(lhbd::protocol::Command::WriteRegister, lhbd::protocol::Status::Ok, 1, {}));

    std::vector<std::uint8_t> valuePayload;
    lhbd::protocol::FrameCodec::appendU32(valuePayload, 0xAABBCCDDU);
    mockPtr->pushResponse(makeResponse(lhbd::protocol::Command::ReadRegister, lhbd::protocol::Status::Ok, 2, valuePayload));

    lhbd::core::DeviceDriver driver(std::move(mock), {.timeout = std::chrono::milliseconds(1), .maxRetries = 0});
    require(driver.connect(), "driver connects for register test");
    require(driver.writeRegister(0x0010, 0xAABBCCDDU), "write register succeeds");
    const auto value = driver.readRegister(0x0010);
    require(value.has_value(), "read register returns value");
    require(*value == 0xAABBCCDDU, "read register value matches");
}

void testTimeoutRetry() {
    auto mock = std::make_unique<lhbd::transport::MockTransport>();
    auto* mockPtr = mock.get();
    mockPtr->pushTimeout();
    mockPtr->pushResponse(makeResponse(lhbd::protocol::Command::Ping, lhbd::protocol::Status::Ok, 1, {}));

    lhbd::core::DeviceDriver driver(std::move(mock), {.timeout = std::chrono::milliseconds(1), .maxRetries = 1});
    require(driver.connect(), "driver connects for retry test");
    require(driver.ping(), "driver succeeds after timeout retry");
    require(driver.stats().timeouts == 1, "driver counted one timeout");
    require(driver.stats().retries == 1, "driver counted one retry");
}

void testCrcRetry() {
    auto mock = std::make_unique<lhbd::transport::MockTransport>();
    auto* mockPtr = mock.get();
    auto corrupted = makeResponse(lhbd::protocol::Command::Ping, lhbd::protocol::Status::Ok, 1, {});
    corrupted.back() ^= 0xFFU;
    mockPtr->pushResponse(corrupted);
    mockPtr->pushResponse(makeResponse(lhbd::protocol::Command::Ping, lhbd::protocol::Status::Ok, 1, {}));

    lhbd::core::DeviceDriver driver(std::move(mock), {.timeout = std::chrono::milliseconds(1), .maxRetries = 1});
    require(driver.connect(), "driver connects for CRC retry test");
    require(driver.ping(), "driver succeeds after CRC retry");
    require(driver.stats().crcErrors == 1, "driver counted one CRC error");
}

void testSensorAndStatus() {
    auto mock = std::make_unique<lhbd::transport::MockTransport>();
    auto* mockPtr = mock.get();

    std::vector<std::uint8_t> tempPayload;
    lhbd::protocol::FrameCodec::appendF32(tempPayload, 21.5F);
    mockPtr->pushResponse(makeResponse(lhbd::protocol::Command::ReadSensor, lhbd::protocol::Status::Ok, 1, tempPayload));

    std::vector<std::uint8_t> statusPayload;
    statusPayload.push_back(2U);
    lhbd::protocol::FrameCodec::appendU16(statusPayload, 0x0004U);
    lhbd::protocol::FrameCodec::appendF32(statusPayload, 12.0F);
    mockPtr->pushResponse(makeResponse(lhbd::protocol::Command::GetStatus, lhbd::protocol::Status::Ok, 2, statusPayload));

    lhbd::core::DeviceDriver driver(std::move(mock), {.timeout = std::chrono::milliseconds(1), .maxRetries = 0});
    require(driver.connect(), "driver connects for sensor/status test");
    const auto temp = driver.readSensor(lhbd::protocol::SensorId::Temperature);
    require(temp.has_value(), "sensor returns value");
    require(std::fabs(*temp - 21.5F) < 0.001F, "sensor value matches");
    const auto status = driver.getStatus();
    require(status.has_value(), "status returns value");
    require(status->mode == 2U && status->errorFlags == 4U, "status fields match");
}

} // namespace

int main() {
    testFrameRoundTrip();
    testCrcCorruptionDetection();
    testDriverPing();
    testReadWriteRegister();
    testTimeoutRetry();
    testCrcRetry();
    testSensorAndStatus();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
