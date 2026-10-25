# Qt reference hierarchy comparison

## Reference source inspected

The reference archive is `/home/user/Desktop/qt-server(20261022-005017).zip`. The Qt UI files are nested under:

`.reference/qt-server(20261022-005017)/qt-ui(20261022-005118)/qt-ui/`

Inspected sources include `mainwindow.ui`, `mainwindow.cpp`, `mainwindow.h`, and the forms `ui/camerawindow.ui`, `ui/graphform.ui`, `ui/plotwindow.ui`, and `ui/settingsform.ui`. The archive and original source tree were not modified. The extracted reference is ignored by Git.

## Original layout concepts

The original `mainwindow.ui` is a large motion-control workspace. Its left telemetry area places the six-axis pose fields and the separate `Count (Axis)` fields in aligned groups. The motion pages use stacked pages for machine parameters and camera/visual-servoing setup. Camera settings are separated into camera status, camera controls, role-specific previews, camera calibration, and initial pose inside the camera ROI. Logging and plot panels are distinct side panels. The separate camera form displays TOP and SIDE previews together; the settings form organizes exposure and gain by camera role; the graph and plot forms provide reusable graph presentation.

The original `mainwindow.cpp` and `.h` confirm that these widgets are connected to physical machine, servo, camera, calibration, and motion-server operations. Those operations and legacy text protocol are not copied into this project.

## FAU application adaptation

- Dashboard adapts the aligned six-axis pose pattern and separate six-field `Count (Axis)` layout. Pose values come from the server's simulated motion provider; validity and source are shown. Raw counts remain `UNAVAILABLE`.
- Live Camera adapts the paired TOP/SIDE preview layout, role mapping, status, and calibration separation. Only the synthetic backend is operational. Basler and physical camera controls are unavailable.
- Focus and FAU tabs separate dataset preparation, model training, testing/inference, and metrics. The focus tab selects between the statistical baseline and pretrained MobileNetV3-Small configuration. The FAU tab keeps the deterministic gradient-centroid baseline separate from the ResNet18 heatmap model configuration.
- Motion Limits keeps editable simulation envelopes and sends validation to the server. It is not a set of approved physical safety limits.
- Training Results, responsive loss charts, and the persistent log panel adapt the reference's separate plot and logging panels.
- Dataset workflows use versioned JSON over the training TCP protocol. No legacy motion command is used.

## Current limitations

The project has no GMRT, EtherCAT, servo, physical movement, encoder feedback, Basler pylon, or camera-to-machine calibration integration. FAU errors are pixel-coordinate metrics unless valid calibration is supplied by a future provider. Pretrained PyTorch workflows report missing dependencies or weights and must not be represented as successful inference when unavailable. Synthetic frames and simulated pose are explicitly identified as such.
