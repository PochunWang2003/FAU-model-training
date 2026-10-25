# Architecture and scope

The application currently has one Qt UI process, one Qt server process, and supervised ML child processes:

- `UI/qt-ui`: Qt Widgets client. Designer owns layouts in `mainwindow.ui`. The client sends commands over `QTcpSocket`, renders synthetic PNG preview events, and displays worker progress.
- `Src/server`: Qt Core/Network server on loopback port 8097 by default. It frames JSON messages, validates commands, stores datasets/configuration, owns a synthetic camera service and simulated motion backend, and supervises one worker process.
- `ML/workers`: standard-library baseline and inference workers plus separate optional PyTorch MobileNetV3-Small and ResNet18 training workers. They consume PGM/JSONL datasets, emit structured JSON events, write checkpoints/results, and check cancellation between epochs.

Server event-loop operations are short file/dataset operations or nonblocking socket/process operations. Training runs in a child process, never in the UI or Qt server event loop. There is no real-time cyclic path.

The server binds to `127.0.0.1`; remote UI clients are not enabled. One training process is allowed at a time. Progress/results go only to the requesting client. If that client disconnects, a reconnecting client that calls `GET_STATUS` attaches to the live run and receives future events; earlier events are not replayed.

The original Qt reference archive remains external and read-only. The inspected source has no copyright or license notice, so redistribution rights remain unresolved and no source was copied into this repository. `Doc/UI_REFERENCE_COMPARISON.md` records what was inspected. The optional local build helper builds a separately supplied Qt project out-of-tree; it does not integrate the original server.

## Implemented flow

1. UI sends `DATASET_CREATE`, `DATASET_IMPORT`, or `CAMERA_CAPTURE`.
2. Server creates grayscale PGM samples and JSONL metadata.
3. User labels samples or saves FAU keypoint annotations in the UI.
4. UI sends `FOCUS_TRAIN_START` or `FAU_TRAIN_START`.
5. Server creates a run record and starts `ML/workers/train.py` with a run ID.
6. Worker emits line-delimited JSON progress; server forwards events to the requesting UI.
7. Worker persists a JSON checkpoint and result record. Cancel writes a marker consumed by the worker.

## Boundaries

Implemented camera source: server-owned synthetic preview/capture and file-folder import converted to grayscale PGM. `BaslerCameraService` is an unavailable placeholder; no pylon device is opened. The pretrained model implementations run only when compatible PyTorch/torchvision dependencies and weights exist. Offline alternatives are focus logistic regression and the FAU gradient-centroid detector. Physical calibration is absent. `IMotionBackend` provides the backend seam; simulation is selected, NEC/GMRT are unavailable, raw counts report unavailable, and no motion hardware library is linked.
