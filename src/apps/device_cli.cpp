#include "lhbd/core/DeviceDriver.hpp"
#include "lhbd/transport/UnixSocketTransport.hpp"
#include "lhbd/utils/Logger.hpp"

#include <iostream>
#include <memory>
#include <string>

namespace {

void usage() {
    std::cout << "Usage:\n"
              << "  lhbd_cli --socket <path> --demo\n"
              << "  lhbd_cli --socket <path> --ping\n"
              << "  lhbd_cli --socket <path> --read-register <hex-address>\n"
              << "  lhbd_cli --socket <path> --write-register <hex-address> <hex-value>\n"
              << "  lhbd_cli --socket <path> --sensor temperature|voltage|current|gyro_x\n"
              << "  lhbd_cli --socket <path> --status\n";
}

std::uint32_t parseHex(const std::string& value) {
    return static_cast<std::uint32_t>(std::stoul(value, nullptr, 0));
}

lhbd::protocol::SensorId parseSensor(const std::string& value) {
    if (value == "temperature") return lhbd::protocol::SensorId::Temperature;
    if (value == "voltage") return lhbd::protocol::SensorId::Voltage;
    if (value == "current") return lhbd::protocol::SensorId::Current;
    return lhbd::protocol::SensorId::GyroX;
}

} // namespace

int main(int argc, char** argv) {
    std::string socketPath = "/tmp/lhbd_device.sock";
    std::string command = "--demo";
    std::string arg1;
    std::string arg2;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--socket" && i + 1 < argc) {
            socketPath = argv[++i];
        } else if (arg.rfind("--", 0) == 0) {
            command = arg;
        } else if (arg1.empty()) {
            arg1 = arg;
        } else {
            arg2 = arg;
        }
    }

    auto transport = std::make_unique<lhbd::transport::UnixSocketTransport>(socketPath);
    lhbd::core::DeviceDriver driver(std::move(transport), {.timeout = std::chrono::milliseconds(300), .maxRetries = 2});

    if (!driver.connect()) {
        std::cerr << "Unable to connect to simulated device at " << socketPath << '\n';
        return 2;
    }

    if (command == "--demo") {
        std::cout << "PING: " << (driver.ping() ? "OK" : "FAILED") << '\n';
        std::cout << "WRITE REG 0x0010: " << (driver.writeRegister(0x0010, 0x12345678) ? "OK" : "FAILED") << '\n';
        const auto reg = driver.readRegister(0x0010);
        std::cout << "READ REG 0x0010: " << (reg ? std::to_string(*reg) : "FAILED") << '\n';
        const auto temp = driver.readSensor(lhbd::protocol::SensorId::Temperature);
        std::cout << "TEMPERATURE: " << (temp ? std::to_string(*temp) : "FAILED") << '\n';
        const auto status = driver.getStatus();
        if (status) {
            std::cout << "STATUS: mode=" << static_cast<int>(status->mode)
                      << " flags=" << status->errorFlags
                      << " uptime=" << status->uptimeSeconds << '\n';
        }
    } else if (command == "--ping") {
        std::cout << (driver.ping() ? "OK" : "FAILED") << '\n';
    } else if (command == "--read-register" && !arg1.empty()) {
        const auto value = driver.readRegister(static_cast<std::uint16_t>(parseHex(arg1)));
        if (!value) return 3;
        std::cout << "0x" << std::hex << *value << std::dec << '\n';
    } else if (command == "--write-register" && !arg1.empty() && !arg2.empty()) {
        const bool ok = driver.writeRegister(static_cast<std::uint16_t>(parseHex(arg1)), parseHex(arg2));
        std::cout << (ok ? "OK" : "FAILED") << '\n';
        if (!ok) return 3;
    } else if (command == "--sensor" && !arg1.empty()) {
        const auto value = driver.readSensor(parseSensor(arg1));
        if (!value) return 3;
        std::cout << *value << '\n';
    } else if (command == "--status") {
        const auto status = driver.getStatus();
        if (!status) return 3;
        std::cout << "mode=" << static_cast<int>(status->mode)
                  << " error_flags=" << status->errorFlags
                  << " uptime=" << status->uptimeSeconds << '\n';
    } else {
        usage();
        return 1;
    }

    const auto& stats = driver.stats();
    std::cerr << "stats commands=" << stats.commandsSent
              << " retries=" << stats.retries
              << " timeouts=" << stats.timeouts
              << " crc_errors=" << stats.crcErrors
              << " protocol_errors=" << stats.protocolErrors << '\n';
    return 0;
}
