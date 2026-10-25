# Training to runtime

Focus and FAU are independent pipelines:

`Dataset → Annotation → Training → Evaluation → Checkpoint → Load Model → Runtime Inference`

Focus uses ImageNet-pretrained MobileNetV3-Small when PyTorch and torchvision are available, with an independent statistical offline baseline. FAU uses ImageNet-pretrained ResNet18 with a keypoint heatmap decoder, with an independent deterministic gradient-centroid baseline. Each model has its own dataset, worker command, result, and checkpoint directory.

The worker records model type/architecture, dataset reference, schema, preprocessing, validation metrics, and format/software metadata as supported by the existing training code. Runtime must validate model type, checkpoint format, camera role, label/keypoint schema, and preprocessing before loading an artifact. An incompatible checkpoint must produce an explicit error.

The Model Testing page selects an existing dataset image and checkpoint and uses the current worker-backed inference commands. Errors such as a missing PyTorch dependency or missing checkpoint are shown in the page.

The Model Runtime page is a mock lifecycle interface only. `MODEL_RUNTIME_MOCK_LOAD` and `MODEL_RUNTIME_MOCK_UNLOAD` transition UI-visible states; they do not open checkpoint files. Runtime inference, model loading, camera overlays, confidence, and timing remain unavailable. No retraining should be required by the future runtime: completed training artifacts should load directly after a versioned runtime loader is implemented and validated.

Camera training and inference should consume immutable snapshots from the shared server camera service. Current images are explicitly synthetic. Basler pylon integration, production preprocessing parity, and live inference are future work.
