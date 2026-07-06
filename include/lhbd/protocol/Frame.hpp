#pragma once

#include "lhbd/protocol/Protocol.hpp"

#include <cstdint>
#include <vector>

namespace lhbd::protocol {

struct Frame {
    Command command{};
    Status status{Status::Ok};
    std::uint16_t sequence{0};
    std::vector<std::uint8_t> payload;
};

struct DecodedFrame {
    Frame frame;
    bool crcValid{false};
};

} // namespace lhbd::protocol
