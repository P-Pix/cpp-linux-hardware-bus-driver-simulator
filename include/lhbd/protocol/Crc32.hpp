#pragma once

#include <cstdint>
#include <span>

namespace lhbd::protocol {

class Crc32 {
public:
    static std::uint32_t compute(std::span<const std::uint8_t> data) noexcept;
};

} // namespace lhbd::protocol
