# FAU Motion and Vision Training

This repository is being structured as one Qt UI plus one unified Qt server for motion status, camera acquisition, Focus/FAU training, model testing, and future model runtime inference. The public build currently runs in offline/simulation mode.

## Current implementation

- The Qt Designer UI contains Motion Control, Cameras and Dataset, Focus Training, FAU Training, Model Testing, Model Runtime, Settings and Logs, and Diagnostics pages.
- Focus training preserves ImageNet-pretrained MobileNetV3-Small support and the statistical offline baseline.
- FAU training preserves ImageNet-pretrained ResNet18 keypoint support and the deterministic gradient-centroid baseline.
- Testing uses the existing dataset/checkpoint and worker-backed inference protocol.
- The model runtime page exercises mock load/unload state transitions only. It does not load checkpoints or infer.
- The server owns one synthetic camera service for TOP/SIDE preview and capture. Basler pylon is not integrated.
- `IMotionBackend` defines the backend boundary. Simulation is selected; NEC and GMRT placeholders are unavailable. Physical initialization, Servo, fault reset, homing, and MoveL are disabled.
- The existing versioned JSON protocol remains on port 8097. Original motion TCP compatibility on port 8087 is documented but not hosted by the unified server yet.

The original Qt server/UI archive at `/home/user/Desktop/qt-server(20261022-005017).zip` remains external and read-only. Redistribution rights have not been confirmed. Do not copy its source, proprietary libraries, headers, binaries, machine data, or logs into this repository. The optional reference build helper is not an integrated motion server.

## Build and test

From the repository root:

```sh
python3 scripts/build.py
python3 scripts/test.py
```

Build outputs are ignored under `build/`. Offline tests use simulated motion and synthetic frames. They do not validate hardware, timing, pretrained model execution, or external NEC/GMRT dependencies. See [Doc/BUILD.md](Doc/BUILD.md).

## Run

Start the server and UI in separate terminals:

```sh
python3 scripts/run_server.py
python3 scripts/run_ui.py
```

The UI connects to `127.0.0.1:8097` by default. Focus and FAU datasets, runs, and checkpoints are independent. PyTorch/torchvision pretrained execution requires compatible dependencies and cached weights on the host. No packages or weights are installed by this project.

## Architecture documents

- [Unified architecture](Doc/UNIFIED_ARCHITECTURE.md)
- [UI page map](Doc/UI_PAGE_MAP.md)
- [Motion backend migration](Doc/MOTION_BACKEND_MIGRATION.md)
- [TCP compatibility](Doc/TCP_COMPATIBILITY.md)
- [Training to runtime](Doc/TRAINING_TO_RUNTIME.md)
- [Build and verification](Doc/BUILD.md)
