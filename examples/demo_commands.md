# Demo commands

Start the simulated device:

```bash
python3 simulator/device_simulator.py --socket /tmp/lhbd_device.sock
```

Run the full demo:

```bash
./build/lhbd_cli --socket /tmp/lhbd_device.sock --demo
```

Read a register:

```bash
./build/lhbd_cli --socket /tmp/lhbd_device.sock --read-register 0x0001
```

Write a register:

```bash
./build/lhbd_cli --socket /tmp/lhbd_device.sock --write-register 0x0010 0x12345678
```

Read a sensor:

```bash
./build/lhbd_cli --socket /tmp/lhbd_device.sock --sensor temperature
```

Inject faults:

```bash
python3 simulator/device_simulator.py --socket /tmp/lhbd_device.sock --drop-rate 0.3 --corrupt-rate 0.2 --delay-ms 50
```
