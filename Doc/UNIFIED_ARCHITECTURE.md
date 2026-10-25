# Unified application architecture

## Process layout

The public offline application has one Qt Widgets UI process and one Qt server process. The server owns the training JSON listener, motion backend instance, camera acquisition service, datasets, and supervised worker lifecycle. Focus and FAU training and inference execute as separate Python processes started and monitored by the server. Workers do not access motion drivers or physical cameras.

The original Qt motion server and its matching UI remain external, read-only references. Their source is not copied into this repository. The archive supplied at `/home/user/Desktop/qt-server(20261022-005017).zip` contains proprietary/library artifacts and has no source copyright or license notice; redistribution rights remain unresolved. `scripts/build_original_qt_reference.py` can build a separately supplied external `.pro` project into the ignored `build/` tree, but this does not merge its server into the unified server.

## Module boundaries

- `UI/qt-ui`: Designer-based pages and Qt JSON client. The original reference layout is not yet integrated; see `UI_PAGE_MAP.md`.
- `Src/server/tcp`: reserved for future protocol adapters. The current `TrainingServer` owns and preserves the existing versioned newline-delimited JSON protocol on port 8097.
- `Src/server/motion`: `IMotionBackend`, a validated simulation implementation, and NEC/GMRT unavailable placeholders.
- `Src/server/camera`: one server-owned `ICameraAcquisitionService`; current synthetic acquisition and an unavailable Basler placeholder.
- `Src/server/dataset`: dataset behavior currently remains in the established training server implementation; extraction into this directory is a later refactor.
- `Src/server/training` and `Src/server/inference`: Python worker supervision and a mock-only runtime state registry.
- `ML`: independent Focus MobileNetV3-Small and FAU ResNet18 keypoint workflows, with their existing offline baselines.

## Ownership and safety

The current server selects only `SimulatedMotionBackend` and `SyntheticCameraService`. Physical motion is disabled in UI and protocol. NEC and GMRT classes return explicit `UNAVAILABLE` results and call no physical APIs. The Basler placeholder does not open a device. A simulated pose is labeled `SIMULATED`, while raw counts remain unavailable.

Camera requests pass through one service instance. Returned `QImage` snapshots are copies; a future pylon implementation must keep device acquisition single-owner and pass immutable snapshots to preview, capture, and inference consumers.

Training and inference workers are separate child processes. They do not run in a cyclic motion callback. This offline architecture makes no PREEMPT_RT timing claim. Training and physical control remain mutually disabled by default.

## Intended operator workflow

1. Initialize the machine only after a physical backend is authorized and validated.
2. Use Servo ON and homing only in an authorized hardware validation session.
3. MoveL the FAU to the required imaging pose after machine limits, axis mapping, and motion state have been validated.
4. Inspect the TOP/SIDE camera ROI.
5. Capture or import images through the server-owned camera service.
6. Annotate Focus labels or FAU keypoints.
7. Train the Focus and FAU models independently.
8. Evaluate checkpoints on selected datasets and images.
9. Load compatible artifacts in a production runtime after that runtime exists.
10. Run inference independently of motion execution.

Steps 5 through 8 support synthetic/offline workflows. Hardware initialization, physical motion, Basler acquisition, compatible checkpoint loading, and production live inference are not implemented in this stage.
