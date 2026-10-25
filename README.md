# FAU Model Training

Standalone Qt client/server prototype for camera datasets, focus training, FAU annotations, baseline detection, and simulated motion-limit validation. It has no EtherCAT or physical motion interface.

## Current implementation status

The application uses a versioned newline-delimited JSON protocol on configurable port 8097. The Qt server supervises one Python worker at a time, and all training and inference requests originate in the Qt UI. The UI provides a synthetic camera preview, dataset management, training controls, progress, evaluation, checkpoint selection, and pixel-space inference overlays.

Focus training supports an ImageNet-pretrained torchvision MobileNetV3-Small sharp/blurred classifier and an explicitly separate statistical CPU baseline. FAU training supports an ImageNet-pretrained ResNet18 encoder with a trainable keypoint heatmap decoder and a separate deterministic gradient-centroid baseline. Focus and FAU data, runs, and checkpoints use independent directories. Motion limits are validated by the server and only feed an in-memory simulator; pose is labeled simulated and raw encoder counts are unavailable.

The requested `qt-server(20261022-005017)(2).zip` was not available during this implementation. The earlier ignored `.reference/` extraction contains both server and UI sources and was used only as a provisional design reference. The exact comparison and its limits are documented in `Doc/UI_REFERENCE_COMPARISON.md`. No legacy production motion source is linked into this application.

## Dependencies

- Qt 6 Core, Network, and Widgets development tools (`qmake6`, `uic`, C++17 compiler)
- Python 3.8 or newer
- Python standard library for the offline baselines
- Compatible PyTorch and torchvision for pretrained training/inference; these were unavailable during this build, and the UI/server report that dependency error explicitly
- Basler pylon is optional; no Basler backend is implemented yet

This host has Qt 6.2.4 and qmake. PyTorch, torchvision, OpenCV Python bindings, and pylon were not present during verification. The server and UI build instructions use qmake.

## Build

From the repository root:

```sh
python3 scripts/build.py
```

The generated binaries are placed under `build/`.

## Run

Terminal 1, start the server:

```sh
python3 scripts/run_server.py
```

Terminal 2, launch the UI:

```sh
python3 scripts/run_ui.py
```

The UI connects to `127.0.0.1:8097`. Use the Focus tab to create/import data, select MobileNetV3-Small or the offline baseline, configure training, and start a run. The UI sends `FOCUS_TRAIN_START`; the server launches and monitors the worker and relays structured progress. FAU has an independent annotation and ResNet18 keypoint workflow. Pretrained runs require PyTorch/torchvision. The server binds to loopback only.

## Tests

```sh
python3 scripts/test.py
```

The test script builds both Qt targets and exercises JSON framing, synthetic camera, dataset labeling, focus score and baseline training, FAU annotations and baseline evaluation, inference, cancellation/reconnection, missing-PyTorch error handling, and server-side motion-limit validation. It launches the Qt Widgets application offscreen as a startup smoke test. It does not prove pretrained model execution, camera hardware behavior, or real-time performance.
