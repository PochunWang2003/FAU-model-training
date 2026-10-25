# Qt reference hierarchy comparison

## Source identity and limitation

The requested `qt-server(20261022-005017)(2).zip` was not present in the accessible Desktop, Downloads, `/home/user` upload search, or `/mnt/data` (the latter path does not exist). The existing ignored `.reference/qt-server(20261022-005017)/` extraction is from the earlier archive without the `(2)` suffix. That earlier extraction contains both complete-looking `qt-server/qt-server` and `qt-ui(20261022-005118)/qt-ui` source trees, including all the named Qt UI forms and C++ source. It is read-only and is used below only as a provisional hierarchy reference. Its contents are not asserted to match the newly requested archive revision. No legacy source is copied into this project.

The comparison below was completed before editing the FAU Designer UI in this branch. The current `mainwindow.ui` already had an uncommitted formatting/XML expansion when work began; it is preserved.

## Observed original hierarchy

In the earlier extracted reference, the main window contains 705 widgets and 71 group boxes. Its top-level motion workspace places `Pose` (`groupPoseBox`) and `Count (Axis)` (`groupCountBox`) together. Pose uses a fixed six-axis X/Y/Z/RX/RY/RZ grid and `comboLivePoseMode` for Machine Pose or Tool Pose. Raw count uses a separate six-field row (`lineCountAxis1` through `lineCountAxis6`). `Camera Status`, `Camera Control`, `Visual Servoing Control`, `Visual Servoing Parameters`, `Camera to Machine Calibration`, `Initial Position inside Camera ROI`, and `Logging` are separate groups. Additional tabs and nested groups contain machine-specific motion parameters.

The reference `UpdateLivePose` stores the received six-value pose in either simulation or real arrays, then refreshes the selected pose fields. Raw counts are stored and rendered through separate simulation/real caches. This separation is useful as a display and ownership pattern. The FAU prototype will not import those legacy motion commands or label synthetic values as real telemetry.

The reference client uses `QTcpSocket`, `readyRead`, and newline-delimited legacy text commands. The FAU application retains its versioned JSON training protocol; only the client/server separation and camera/status presentation pattern are reused.

## Current FAU hierarchy

Before this update, the FAU UI had 92 widgets and six group boxes: `System status`, `Recent result and project notes`, `Dataset management`, the focus training group, FAU annotation, and simulated move request. The tabs were Dashboard, Live Camera, Focus Training, FAU Training, Training Results, Motion Limits, and Logs. Pose and raw-count displays were absent. Focus and FAU operations were already separate, but the focus model was the offline logistic baseline and the FAU run was a deterministic baseline check.

## Reuse and adaptation map

- Pose group: adapt as a fixed-alignment six-axis feedback panel. Its source badge must say `SIMULATED`; a future provider may report `REAL` only when supplied by GMRT. The prototype has no GMRT provider.
- Count (Axis): preserve as a separate six-axis raw-count panel. Until a provider supplies counts, display `UNAVAILABLE`; never derive or label invented encoder counts as real.
- Camera Status and Camera Control: adapt to explicit TOP/SIDE synthetic or file source state and separate operational start/stop/capture controls.
- Visual Servoing Control and Visual Servoing Parameters: reuse the visual separation between operations and parameter editing, replacing servo actions with independent focus and FAU training workflows.
- Camera to Machine Calibration: adapt to a calibration status/configuration group that remains `UNCONFIGURED` until measured calibration data is supplied. Do not provide invented mm/degree scale constants.
- Camera ROI limits: adapt to editable pixel-space ROI fields and ROI validation for annotations.
- Logging: retain a persistent central event log for network, dataset, camera, worker, and validation messages.
- Machine-specific movement and servo groups: omit. Only server-validated simulated position requests remain.

The requested `(2)` archive should still be checked when it becomes available. If it differs from the earlier extracted revision, this document and the visual mapping should be reconciled against that exact source before treating the comparison as revision-accurate.
