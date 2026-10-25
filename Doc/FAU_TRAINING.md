# FAU annotation and baseline

The UI saves a role, image, ROI, feature ID, pixel coordinates, validity, and confidence. The current baseline computes a gradient-weighted centroid inside each ROI and reports mean Euclidean pixel error against saved keypoints. The run artifact identifies the `gradient-centroid` algorithm and `calibration: none`.

This is a deterministic baseline evaluation, not deep model training. It does not implement editable image overlays, automatic ROI drawing, top/side image import management, multi-keypoint-specific detectors, validation splits, or calibrated TOP/SIDE mm/degree outputs. Feature channels such as `TOP_DX`, `TOP_DY`, `TOP_DANG`, `TOP_ANG`, `SIDE_DX`, `SIDE_DY`, `SIDE_DANG`, and `SIDE_ANG` are annotation IDs/reference concepts only.
