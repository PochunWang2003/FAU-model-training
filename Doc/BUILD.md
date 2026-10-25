# Build and offline verification

## Offline build

The normal public build uses only the Qt development tools already installed on the system and Python's standard library for offline baselines. It does not require the external original Qt source, NEC dependencies, GMRT, Basler pylon, PyTorch, or CUDA.

From the repository root:

```sh
python3 scripts/build.py
python3 scripts/test.py
```

Build outputs are placed under ignored `build/server` and `build/ui` directories. Tests use synthetic images and simulated motion only. A passing offline suite does not validate machine motion, Basler acquisition, PREEMPT_RT timing, pretrained model execution, or GMRT hardware behavior.

## Optional external Qt reference build

Keep the original archive/source outside the repository. If an authorized local source tree is available, build its `.pro` file separately:

```sh
python3 scripts/build_original_qt_reference.py --project-file /path/to/external/qt-ui/qt-ui.pro
```

The helper rejects a project path inside this repository and writes generated files under ignored `build/original-motion-ui` by default. It does not copy source or binaries into Git. It may fail if Qt modules, proprietary libraries, or other local dependencies are unavailable. It builds a separate reference application, not the unified server.

## External blockers

- Original Qt source ownership/redistribution rights have not been confirmed.
- NEC proprietary headers/libraries are not included and no physical NEC adapter is enabled.
- GMRT hardware integration has not been authorized or validated.
- Basler pylon is not integrated.
- PyTorch/torchvision and pretrained model execution depend on the host environment and cached weights.
