# UI page map

The current Qt Designer file is `UI/qt-ui/mainwindow.ui`. Existing Focus and FAU widgets and their signal handlers were retained; pages are added or relabeled around those workflows.

- **Motion Control**: simulated pose and count status, server status, and physical machine controls. Initialize, Servo ON/OFF, fault reset, homing, and MoveL controls are visibly disabled. Motion limits and the explicit simulation operation remain under Settings and Logs.
- **Cameras and Dataset**: TOP/SIDE synthetic previews, capture/import, role display, ROI fields, and calibration status. Basler is unavailable.
- **Focus Training**: Focus data, labels, independent MobileNetV3-Small or statistical baseline training, progress, evaluation, and existing checkpoint inference controls.
- **FAU Training**: FAU datasets and keypoint annotations, ROI, independent ResNet18 or deterministic baseline training, progress, and existing checkpoint inference controls.
- **Model Testing**: separate Focus and FAU dataset, split, image, checkpoint, inference preview, and JSON result controls. Existing saved-run evaluation remains available.
- **Model Runtime**: checkpoint selection fields and server-backed mock load/unload transitions. Labels state that no checkpoint is read, inference remains disabled, and timing is unavailable.
- **Settings and Logs**: server-validated simulation limits and persistence controls. The persistent server/training log remains below the tab widget.
- **Diagnostics**: concise status explanation.

The original motion UI has not been copied or integrated. Existing widgets adapt its pose/count grouping and camera role concepts only. Exact UI reuse requires resolving redistribution rights and supplying the external source tree to the optional local build helper. The helper builds the reference as a separate application and is not an integrated runtime.
