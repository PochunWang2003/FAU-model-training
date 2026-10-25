"""ImageNet-pretrained MobileNetV3-Small focus classifier."""


def build_model():
    import torch.nn as nn
    from torchvision.models import MobileNet_V3_Small_Weights, mobilenet_v3_small

    model = mobilenet_v3_small(weights=MobileNet_V3_Small_Weights.DEFAULT)
    final_layer = model.classifier[-1]
    model.classifier[-1] = nn.Linear(final_layer.in_features, 2)
    return model
