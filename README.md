# litmus-xsk

This repository contains litmus tests for the ring buffer implementation in the Linux kernel used by the AF_XDP socket (XSK) subsystem. It also introduces new litmus tests to support multiple producers and consumers (MPSC) within the kernel.

Currently, AF_XDP rings support only single producer / single consumer (SPSC) setups. They use load-acquire/store-release barriers to ensure strict memory ordering between the producer and consumer. (Depending on the direction of the data flow in an AF_XDP context, either the kernel or the user-space application can act as the producer or consumer).

The rings were updated (https://github.com/torvalds/linux/commit/a23b3f5697e6cf8affa7adf3e967e5ab569ea757) from general smp_{r,w,}mb() barriers in kernel to the more specific load-acquire/store-release barriers.

Further updating the ring buffer to support multiple producers or consumers allows for efficient packet redirections between multiple AF_XDP sockets. This concept was demonstrated in: "FLASH: Fast Linked AF_XDP Sockets for High Performance Network Function Chains", SoCC'25 (https://doi.org/10.1145/3772052.3772258)

> The litmus tests for multiple producer support are currently being actively developed and may not be complete. Additional litmus tests, specifically for multiple consumer support will be added soon.

## Directory structure

The repository is organized by ring buffer implementation versions::
- `litmus-tests/spsc-v1-barrier/`: Tests for the old version of the ring buffer implementation that used general smp_{r,w,}mb() barriers.
- `litmus-tests/spsc-v2-acqrel/`: Tests for the current version that uses load-acquire/store-release barriers.
- `litmus-tests/spsc-v3-acqrelnew/`: Tests for the new MPSC-capable version using load-acquire/store-release barriers, but evaluated under single producer/consumer conditions.
- `litmus-tests/mpsc/`: Tests for the new MPSC version evaluating multiple producers in the kernel.

There are also config files for the herd7 tool and diff files that show the changes in the ring buffer implementation for v1 to v2 and v2 to v3.

Because these MPSC changes are contained entirely within the kernel, the user-space (whether acting as a consumer or producer) continues to operate under and expect standard single producer/consumer semantics.

## Running the litmus tests

To run the tests, you will need the herd7 tool. Once installed, you can execute the litmus tests using the provided configuration file:

```bash
herd7 -conf linux-kernel.cfg ./litmus-tests/<test-directory>/<test-file>
```

## Acknowledgements

The work is a fork of the original litmus tests for the Linux kernel ring buffer implementation, which can be found at https://github.com/bjoto/litmus-xsk

