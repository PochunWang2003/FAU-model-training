# Motion backend migration

## Current interface

`Src/server/motion/IMotionBackend.h` defines initialization/shutdown, machine and motion status, actual pose, raw feedback through `IMotionFeedbackProvider`, Servo ON/OFF, fault reset, homing, MoveL, stop, and motion configuration operations.

`SimulatedMotionBackend` is the only selected backend. It reports simulated feedback and rejects Servo, fault reset, homing, and MoveL. Explicit `MOTION_SIMULATE` requests continue through server-side limit validation. They do not represent actuator commands.

`NecMotionBackend` and `GmrtMotionBackend` are intentionally unavailable placeholders. They link no NEC or GMRT library and access no hardware. `MOTION_BACKEND_SELECT` rejects either physical backend.

## NEC adapter integration

The original NEC server source is external and its redistribution rights are unresolved. Do not copy it or proprietary headers/libraries into this repository. A later locally authorized adapter must preserve the existing execution path and command ownership, with no relocation of cyclic logic into Qt, Python, or general-purpose workers. Builds without the local proprietary dependencies must continue to support simulation.

## GMRT transition gate

Do not implement hardware calls until GMRT validation is separately authorized. Before enabling a GMRT adapter, validate:

- master/slave/axis identity and PDO mapping;
- CiA 402 state transitions, fault reset, Servo ON/OFF, and status confirmation;
- initialization, homing completion and reference pose semantics;
- pose/count feedback validity and freshness;
- machine limits, axis order, units, and MoveL equivalence;
- stop/cancel behavior and error classification;
- timing and CPU/IRQ behavior on the target PREEMPT_RT system.

Do not assume NEC and GMRT command semantics match. Only select a physical backend after offline, code, and hardware reviews pass. UI visibility must be gated by server-reported availability, not by a configuration string alone.
