# Project guidance

This repository is a unified Qt application architecture for motion control, camera acquisition, focus/FAU training, testing, and inference. Keep one Qt UI and one server process; keep motion, camera, dataset, training, and inference responsibilities in separate modules with explicit interfaces.

## Safety and hardware

- Physical motion is disabled by default. NEC and GMRT physical backends remain unavailable until integration is separately authorized and hardware validation is documented.
- The simulation backend is the only motion backend that may be enabled in the public offline build. Label simulated pose and values explicitly; never fabricate raw encoder counts.
- Do not invoke EtherCAT, servo, homing, or physical MoveL functions during offline builds or tests.
- Preserve server-side motion-limit validation. Do not infer machine identity, axis mapping, limits, homing state, or motion semantics.
- Keep blocking operations, allocation, disk I/O, logging, and UI/TCP waits out of real-time cyclic callbacks. ML workers must be supervised separate processes and must never access servo drivers.
- Do not claim PREEMPT_RT timing guarantees without measurements on the target system.

## Architecture and compatibility

- Preserve the existing FAU training protocol and behavior. Keep the original motion protocol behind an explicit compatibility boundary; do not silently translate or rename commands.
- Camera acquisition has one server-side owner. Training and inference consume immutable snapshots from that service rather than opening a physical camera independently.
- Keep Qt UI, server transport, machine configuration, camera service, datasets, training workers, and runtime inference in separate modules.
- Keep the original Qt server/UI archive external and read-only. Do not copy its source, proprietary headers, libraries, binaries, machine data, or logs into the public repository until redistribution rights are confirmed.
- The offline/simulation build must work without the original source, NEC dependencies, Basler pylon, PyTorch, or CUDA. Report unavailable features accurately.
- Keep generated images, datasets, runs, checkpoints, downloaded weights, and local reference sources out of Git.
- Training runs must be requested by the UI and supervised by the unified server. Training and physical control remain mutually disabled by default.
- Use Python's standard library for the offline baseline. PyTorch and pylon are optional backends.

## Code quality

- Keep code readable, maintainable, and easy to extend. Use lowercase local variable names and uppercase-leading global names. Start each word in new function names with an uppercase letter.
- Avoid unrelated rewrites and preserve existing behavior unless the requested integration requires a change.
- Add offline tests for structural behavior. Tests must not invoke real motion or camera hardware.
