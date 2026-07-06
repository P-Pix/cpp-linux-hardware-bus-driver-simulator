#include "lhbd/protocol/FrameCodec.hpp"

#include "lhbd/protocol/Crc32.hpp"

#include <cstring>
#include <limits>

namespace lhbd::protocol {

namespace {

void setError(std::string* error, const std::string& value) {
    if (error != nullptr) {
        *error = value;
    }
}

} // namespace

std::string toString(Command command) {
    switch (command) {
        case Command::Ping: return "PING";
        case Command::ReadRegister: return "READ_REGISTER";
        case Command::WriteRegister: return "WRITE_REGISTER";
        case Command::ReadSensor: return "READ_SENSOR";
        case Command::GetStatus: return "GET_STATUS";
        case Command::Reset: return "RESET";
    }
    return "UNKNOWN_COMMAND";
}

std::string toString(Status status) {
    switch (status) {
        case Status::Ok: return "OK";
        case Status::InvalidCommand: return "INVALID_COMMAND";
        case Status::InvalidPayload: return "INVALID_PAYLOAD";
        case Status::CrcError: return "CRC_ERROR";
        case Status::DeviceBusy: return "DEVICE_BUSY";
        case Status::DeviceFault: return "DEVICE_FAULT";
        case Status::Timeout: return "TIMEOUT";
    }
    return "UNKNOWN_STATUS";
}

std::vector<std::uint8_t> FrameCodec::encode(const Frame& frame) {
    if (frame.payload.size() > MAX_PAYLOAD_SIZE) {
        return {};
    }

    std::vector<std::uint8_t> out;
    out.reserve(HEADER_SIZE + frame.payload.size() + CRC_SIZE);

    appendU32(out, MAGIC);
    out.push_back(VERSION);
    out.push_back(static_cast<std::uint8_t>(frame.command));
    out.push_back(static_cast<std::uint8_t>(frame.status));
    out.push_back(0x00); // Reserved for future bus flags.
    appendU16(out, frame.sequence);
    appendU16(out, 0x0000); // Reserved field.
    appendU32(out, static_cast<std::uint32_t>(frame.payload.size()));
    out.insert(out.end(), frame.payload.begin(), frame.payload.end());

    const std::uint32_t crc = Crc32::compute(out);
    appendU32(out, crc);
    return out;
}

std::optional<DecodedFrame> FrameCodec::decode(std::span<const std::uint8_t> bytes, std::string* error) {
    if (bytes.size() < HEADER_SIZE + CRC_SIZE) {
        setError(error, "frame too short");
        return std::nullopt;
    }

    const std::uint32_t magic = readU32(bytes, 0);
    if (magic != MAGIC) {
        setError(error, "invalid magic");
        return std::nullopt;
    }

    const auto version = bytes[4];
    if (version != VERSION) {
        setError(error, "unsupported protocol version");
        return std::nullopt;
    }

    const auto payloadSize = static_cast<std::size_t>(readU32(bytes, 12));
    if (payloadSize > MAX_PAYLOAD_SIZE) {
        setError(error, "payload too large");
        return std::nullopt;
    }

    const std::size_t expectedSize = HEADER_SIZE + payloadSize + CRC_SIZE;
    if (bytes.size() != expectedSize) {
        setError(error, "invalid frame size");
        return std::nullopt;
    }

    const std::uint32_t expectedCrc = readU32(bytes, HEADER_SIZE + payloadSize);
    const std::uint32_t actualCrc = Crc32::compute(bytes.subspan(0, HEADER_SIZE + payloadSize));
    const bool crcValid = expectedCrc == actualCrc;

    Frame frame;
    frame.command = static_cast<Command>(bytes[5]);
    frame.status = static_cast<Status>(bytes[6]);
    frame.sequence = readU16(bytes, 8);
    frame.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(HEADER_SIZE),
                         bytes.begin() + static_cast<std::ptrdiff_t>(HEADER_SIZE + payloadSize));

    return DecodedFrame{frame, crcValid};
}

std::uint16_t FrameCodec::readU16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                      static_cast<std::uint16_t>(bytes[offset + 1U]));
}

std::uint32_t FrameCodec::readU32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3U]);
}

float FrameCodec::readF32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    const std::uint32_t raw = readU32(bytes, offset);
    float value = 0.0F;
    static_assert(sizeof(value) == sizeof(raw));
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

void FrameCodec::appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void FrameCodec::appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void FrameCodec::appendF32(std::vector<std::uint8_t>& out, float value) {
    std::uint32_t raw = 0;
    static_assert(sizeof(value) == sizeof(raw));
    std::memcpy(&raw, &value, sizeof(raw));
    appendU32(out, raw);
}

} // namespace lhbd::protocol
