# Focus model workflow

The primary focus model is torchvision's ImageNet-pretrained MobileNetV3-Small. Its final classifier layer is replaced with a two-class sharp/blurred head. Training records explicit train, validation, and test membership; displays training/validation cross-entropy and validation accuracy; saves the best validation checkpoint as a PyTorch `.pt` file; and supports image-level inference from the UI. The checkpoint, run record, and dataset live in the independent `models/focus`, `runs`, and `data/focus` locations.

The Focus tab also offers an explicitly named statistical offline baseline. It trains CPU logistic regression over log variance-of-Laplacian, log Tenengrad, and normalized intensity variance, and writes a small JSON checkpoint. The separate `FOCUS_SCORE` command remains the traditional non-model variance-of-Laplacian score. The demo dataset is synthetic and cannot establish real camera focus performance.

Pretrained MobileNet training/inference requires a compatible PyTorch and torchvision installation. The worker requests `MobileNet_V3_Small_Weights.DEFAULT`; it never labels random initialization as pretrained. CUDA is checked by the worker when selected. This development environment did not contain PyTorch or torchvision, so the pretrained workflow was built but not executed here; its missing-dependency error path is tested.
