# Build and run

The build helper uses `qmake6` (falling back to `qmake`) and `make`; it does not install packages. It builds independent server and UI targets into `build/server` and `build/ui`.

```sh
python3 scripts/build.py
```

Run the Qt server in one terminal:

```sh
python3 scripts/run_server.py
```

The optional first argument changes the port, for example `python3 scripts/run_server.py 8098`. The server binds to loopback. Run the UI in another terminal:

```sh
python3 scripts/run_ui.py
```

Run build plus offline integration tests:

```sh
python3 scripts/test.py
```

Use the Focus Training tab: connect, create `focus_demo`, then select Start focus training. The server creates and supervises the worker; epoch loss and accuracy arrive over TCP. The FAU tab can create `fau_demo`, save the default example keypoint, and run the deterministic baseline. A display server is needed for interactive UI use; tests use Qt's offscreen platform plugin for startup only.
