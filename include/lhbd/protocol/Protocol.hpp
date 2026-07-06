#pragma once

#include <cstdint>
#include <string>

namespace lhbd::protocol {

constexpr std::uint32_t MAGIC = 0x4C484244; // ASCII: LHBD
constexpr std::uint8_t VERSION = 1;
constexpr std::size_t HEADER_SIZE = 16;
constexpr std::size_t CRC_SIZE = 4;
constexpr std::size_t MAX_PAYLOAD_SIZE = 4096;

enum class Command : std::uint8_t {
    Ping = 0x01,
    ReadRegister = 0x02,
    WriteRegister = 0x03,
    ReadSensor = 0x04,
    GetStatus = 0x05,
    Reset = 0x06
};

enum class Status : std::uint8_t {
    Ok = 0x00,
    InvalidCommand = 0x01,
    InvalidPayload = 0x02,
    CrcError = 0x03,
    DeviceBusy = 0x04,
    DeviceFault = 0x05,
    Timeout = 0x06
};

enum class SensorId : std::uint8_t {
    Temperature = 0x01,
    Voltage = 0x02,
    Current = 0x03,
    GyroX = 0x04
};

std::string toString(Command command);
std::string toString(Status status);

} // namespace lhbd::protocol
