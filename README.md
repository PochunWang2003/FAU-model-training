# FAU Model Training

Standalone Qt client/server prototype for camera datasets, focus training, FAU annotations, baseline detection, and simulated motion-limit validation. It has no EtherCAT or physical motion interface.

## Current implementation status

The first vertical slice is designed around a versioned newline-delimited JSON protocol on port 8097. The Qt server supervises one Python worker at a time. Focus training is a small CPU logistic-regression baseline using image statistics; it is not a PyTorch model. FAU uses annotation persistence and a deterministic OpenCV-style image-gradient baseline; it is not a trained deep detector. The offline camera is synthetic. The Basler adapter is not yet implemented.

The reference Qt server and matching UI archive is extracted locally in `.reference/` for read-only inspection. It is ignored by Git and none of its production motion code or binaries are linked into this project.

## Dependencies

- Qt 6 Core, Network, and Widgets development tools (`qmake6`, `uic`, C++17 compiler)
- Python 3.8 or newer
- No Python package installation is required for the offline baseline
- Optional future backends: PyTorch, OpenCV, and Basler pylon SDK

This host has Qt 6.2.4 and qmake. CMake, PyTorch, OpenCV Python bindings, and pylon were not present during initial inspection. The server and UI build instructions use qmake.

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

The UI connects to `127.0.0.1:8097`. Use the Focus tab to create a demo dataset and start a run. All training starts by sending `FOCUS_TRAIN_START` to the Qt server; the server launches and monitors the Python worker and relays structured progress events. The server binds to loopback only.

## Tests

```sh
python3 scripts/test.py
```

The test script builds both Qt targets and exercises JSON framing, synthetic camera, dataset labeling, focus score and training, FAU annotations and baseline evaluation, cancellation/reconnection, and server-side motion-limit validation. It launches the Qt Widgets application offscreen as a startup smoke test. It does not prove camera hardware behavior or real-time performance.
