# TCP protocol version 1

Transport: TCP, newline-delimited UTF-8 JSON. A TCP read may contain a fragment, one message, or several messages. The receiver buffers bytes until LF. Each command line is limited to 1 MiB. Malformed JSON receives a structured error response. Oversized partial input disconnects the client. If queued output exceeds 2 MiB, the server disconnects that client instead of growing an unbounded queue.

Requests contain `protocol_version` (integer `1`), `request_id` (unique string per connection), `type` (`command`), `command`, `payload` (object), and `timestamp` (ISO string recommended). Responses contain `type=response`, matching `request_id`, `event`, `ok`, `payload`, and optional `error={code,message}`. Server events contain `type=event`, `event`, `payload`, and `timestamp`. Duplicate request IDs are rejected without re-running the command. Unknown commands are rejected. Image preview uses Base64 PNG only for the small synthetic demo frame.

Example request:

```json
{"protocol_version":1,"request_id":"17","type":"command","command":"FOCUS_TRAIN_START","payload":{"dataset":"focus_demo","epochs":20,"learning_rate":0.05},"timestamp":"2026-10-25T07:00:00Z"}
```

Implemented commands:

- Server/status: `HELLO`, `GET_STATUS`, `TRAINING_STATUS`
- Camera: `CAMERA_LIST`, `CAMERA_OPEN`, `CAMERA_CLOSE`, `CAMERA_START`, `CAMERA_STOP`, `CAMERA_CAPTURE`
- Dataset: `DATASET_CREATE`, `DATASET_LIST`, `DATASET_IMPORT`, `DATASET_LABEL`, `DATASET_SPLIT`
- Focus: `FOCUS_SCORE`, `FOCUS_TRAIN_START`, `FOCUS_TRAIN_CANCEL`, `FOCUS_EVALUATE`, `FOCUS_INFER`
- FAU: `FAU_ANNOTATION_SAVE`, `FAU_TRAIN_START`, `FAU_TRAIN_CANCEL`, `FAU_EVALUATE`, `FAU_INFER`
- Runs: `TRAINING_STATUS`, `TRAINING_RUNS`, `TRAINING_RESULT`
- Simulation/feedback: `MOTION_LIMIT_GET`, `MOTION_LIMIT_VALIDATE`, `MOTION_LIMIT_SAVE`, `MOTION_SIMULATE`, `MOTION_FEEDBACK_GET`

Events/responses: `CAMERA_FRAME`, `TRAINING_STARTED`, `TRAINING_PROGRESS`, `TRAINING_COMPLETED`, `TRAINING_FAILED`, `TRAINING_CANCELLED`, `FOCUS_INFER`, `FAU_INFER`, and `SERVER_LOG`.

`CAMERA_SET_PARAMETER` remains unavailable. Inference requires an existing checkpoint and image in the selected dataset. The statistical/gradient baselines work without PyTorch; `.pt` inference needs compatible PyTorch/torchvision. Exposure/gain are not silently simulated. The server binds only to loopback by default. Training events/results are sent only to the requesting connection; reconnecting clients can query active status and attach to a running job without replaying old events.
