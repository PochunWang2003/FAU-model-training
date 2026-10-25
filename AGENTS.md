# Project guidance

- This repository is a standalone visual training prototype. Never add EtherCAT, servo, or physical motion control.
- Keep camera acquisition, datasets, training workers, UI, and transport in separate modules.
- The only supported movement implementation is simulation, with server-side validation.
- Training runs must be started by a TCP command originating from the UI and supervised by the server.
- Keep generated images, datasets, runs, checkpoints, and reference sources out of Git.
- Use Python's standard library for the offline baseline. PyTorch and pylon are optional future backends.
