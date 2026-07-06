#pragma once

#include "lhbd/protocol/Frame.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lhbd::protocol {

class FrameCodec {
public:
    static std::vector<std::uint8_t> encode(const Frame& frame);
    static std::optional<DecodedFrame> decode(std::span<const std::uint8_t> bytes, std::string* error = nullptr);

    static std::uint16_t readU16(std::span<const std::uint8_t> bytes, std::size_t offset);
    static std::uint32_t readU32(std::span<const std::uint8_t> bytes, std::size_t offset);
    static float readF32(std::span<const std::uint8_t> bytes, std::size_t offset);

    static void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value);
    static void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value);
    static void appendF32(std::vector<std::uint8_t>& out, float value);
};

} // namespace lhbd::protocol
