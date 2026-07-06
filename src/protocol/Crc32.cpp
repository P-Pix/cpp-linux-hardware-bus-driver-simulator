#include "lhbd/protocol/Crc32.hpp"

namespace lhbd::protocol {

std::uint32_t Crc32::compute(std::span<const std::uint8_t> data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::uint8_t byte : data) {
        crc ^= static_cast<std::uint32_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const bool lsb = (crc & 1U) != 0U;
            crc >>= 1U;
            if (lsb) {
                crc ^= 0xEDB88320U;
            }
        }
    }
    return ~crc;
}

} // namespace lhbd::protocol
