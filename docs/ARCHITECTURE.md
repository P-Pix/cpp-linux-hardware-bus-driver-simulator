# Architecture

## Goal

The project models a realistic but reproducible hardware communication stack. The C++ side behaves like a user-space driver. The Python side behaves like a small hardware peripheral with registers, sensors and fault injection.

## Runtime architecture

```text
+----------------------+       binary frames        +-------------------------+
| C++ DeviceDriver     | <------------------------> | Python device simulator |
|                      |     UNIX domain socket     |                         |
| - command API        |                            | - register map          |
| - retry policy       |                            | - sensor values         |
| - CRC validation     |                            | - delay/drop/corrupt    |
+----------+-----------+                            +-------------------------+
           |
           v
+----------------------+
| ITransport           |
| - UnixSocketTransport|
| - MockTransport      |
+----------------------+
```

## Design choices

### User-space driver abstraction

The project intentionally does not implement a Linux kernel module. The goal is to demonstrate a driver-like C++ layer that can be tested, debugged and extended easily. This is representative of many industrial systems where application software communicates with equipment through serial ports, sockets, field buses or vendor APIs.

### Transport abstraction

`DeviceDriver` depends on `ITransport`, not directly on UNIX sockets. This separation allows tests to inject `MockTransport` and future implementations to add serial, SPI or I2C adapters without changing the driver API.

### Binary protocol

The protocol uses fixed-size headers, big-endian integers, explicit payload length and CRC32. This keeps parsing deterministic and avoids text protocol ambiguities.

### Retry strategy

Timeouts and CRC errors are considered recoverable communication faults. The driver retries the same command with the same sequence number so a response can be correlated to the request.

### Testability

Unit tests do not require the Python simulator. They use `MockTransport` to inject responses, timeouts and corrupted frames. An integration smoke test then validates the full C++/Python path.

## Complexity

Frame encoding and decoding are linear in payload size: `O(n)`, where `n` is the number of payload bytes. Register reads and writes in the simulator are average `O(1)` dictionary operations. Driver transactions are bounded by `maxRetries + 1`, so the worst-case transaction cost is `O(r * n)`, where `r` is the number of attempts.
