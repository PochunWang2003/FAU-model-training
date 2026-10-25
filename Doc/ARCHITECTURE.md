# Architecture and scope

The prototype has three isolated components:

- `UI/qt-ui`: Qt Widgets client. Designer owns layouts in `mainwindow.ui`. The client sends commands over `QTcpSocket`, renders synthetic PNG preview events, and displays worker progress.
- `Src/server`: Qt Core/Network server on loopback port 8097 by default. It frames JSON messages, validates commands, stores datasets/configuration, runs the synthetic camera service, enforces simulation-only limits, and supervises one worker process.
- `ML/workers`: standard-library Python worker. It consumes versioned PGM/JSONL datasets, emits JSON progress lines, writes JSON checkpoints/results, and checks a cancellation marker between epochs.

Server event-loop operations are short file/dataset operations or nonblocking socket/process operations. Training runs in a child process, never in the UI or Qt server event loop. There is no real-time cyclic path.

The server binds to `127.0.0.1`; remote UI clients are not enabled. One training process is allowed at a time. Progress/results go only to the requesting client. If that client disconnects, a reconnecting client that calls `GET_STATUS` attaches to the live run and receives future events; earlier events are not replayed.

The reference archive is extracted into `.reference/` only for read-only architecture review. It includes production motion sources and third-party binary/log files, but those files are Git-ignored and not linked or copied into the new implementation. The archive did not provide a license manifest, so this project does not redistribute its code.

## Implemented flow

1. UI sends `DATASET_CREATE`, `DATASET_IMPORT`, or `CAMERA_CAPTURE`.
2. Server creates grayscale PGM samples and JSONL metadata.
3. User labels samples or saves FAU keypoint annotations in the UI.
4. UI sends `FOCUS_TRAIN_START` or `FAU_TRAIN_START`.
5. Server creates a run record and starts `ML/workers/train.py` with a run ID.
6. Worker emits line-delimited JSON progress; server forwards events to the requesting UI.
7. Worker persists a JSON checkpoint and result record. Cancel writes a marker consumed by the worker.

## Boundaries

Implemented image backend: synthetic grayscale only. File import converts supported image formats through Qt image plugins to PGM. No Basler pylon backend exists yet. Focus uses a hand-coded CPU logistic regression over variance-of-Laplacian, Tenengrad, and intensity-variance features. FAU baseline computes a gradient-weighted centroid and reports pixel error against saved keypoints; it is not a trained neural detector. Physical calibration is absent. Movement is simulation-only and no motion hardware libraries are linked.
