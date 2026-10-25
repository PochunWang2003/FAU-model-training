# TCP compatibility boundaries

## Training protocol

The public unified server preserves the existing version 1 newline-delimited JSON protocol on configurable training port 8097. Each command uses a request ID and JSON payload; responses and asynchronous events retain the existing framing. The Qt UI continues to use this connection for datasets, camera previews/capture, training, evaluation, inference, motion-limit validation, simulated feedback, and mock runtime status.

The server is the single owner of the training listener, simulated motion backend, camera service, and workers. Both TOP and SIDE camera roles are served through the same service instance.

## Original motion protocol

The external reference motion server uses a separate legacy line-oriented text command protocol and defaults to port 8087 in its source. It is not currently hosted by this server, and no command translation is implemented. `config/app.example.json` reserves a disabled compatibility port setting. Never send JSON training commands to the legacy motion listener or reinterpret legacy commands without paired client/server tests.

When a local authorized integration is available, the intended transitional design is for one server process to own both listeners: existing legacy motion compatibility on 8087 and training JSON on 8097. The optional build script only builds the external Qt project out-of-tree; it does not create this integrated two-listener process. This remains an integration blocker.

Physical motion commands exposed on the training JSON port (`MOTION_SERVO_ON`, `MOTION_HOME`, `MOTION_MOVEL`, and related operations) are dispatched only to the selected backend. The current simulator rejects drive operations; NEC and GMRT are not selectable. This does not provide compatibility with the original motion protocol.
