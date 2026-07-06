BUILD_DIR ?= build
BUILD_TYPE ?= Release

.PHONY: configure build test clean run-simulator run-cli integration zip

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) -j

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

run-simulator:
	python3 simulator/device_simulator.py --socket /tmp/lhbd_device.sock

run-cli: build
	$(BUILD_DIR)/lhbd_cli --socket /tmp/lhbd_device.sock --demo

integration: build
	python3 scripts/run_integration_test.py --binary $(BUILD_DIR)/lhbd_cli --socket /tmp/lhbd_integration.sock

zip:
	cd .. && zip -r cpp-linux-hardware-bus-driver-simulator.zip cpp-linux-hardware-bus-driver-simulator -x '*/build/*' '*/__pycache__/*'
