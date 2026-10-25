# FAU keypoint model workflow

FAU data and checkpoints are separate from focus. Annotation records include image, camera role (`top` or `side`), image dimensions, pixel ROI, feature identifiers, point coordinates, validity/confidence, timestamp, and schema version. The UI can assign each annotated image to train, validation, or test. Missing camera roles and partially labeled keypoint sets are supported by role-specific training and per-keypoint heatmap masks.

The primary model uses a ResNet18 encoder initialized with torchvision `ResNet18_Weights.DEFAULT` and a trainable convolutional heatmap decoder. It trains heatmaps for the labeled keypoint IDs, reports validation loss and mean pixel localization error, saves a role-specific `.pt` checkpoint, and can draw pixel-space detections over the selected image. The checkpoint records `calibration: none`. Pixel coordinates are never converted to millimeters or degrees.

An explicitly named gradient-centroid offline baseline remains available without PyTorch. It is deterministic image processing, not neural-network training. It validates the saved annotations, reports pixel error, writes a baseline JSON artifact, and supports a pixel-space overlay inference preview.

ResNet18 pretrained training/inference requires compatible PyTorch and torchvision. The worker uses ImageNet weights explicitly and reports a dependency error if unavailable. This environment lacked PyTorch and torchvision, so only the baseline was executed here. No trained ResNet18 checkpoint or accuracy claim is included.
