"""ImageNet-pretrained ResNet18 encoder with a trainable keypoint heatmap decoder."""


def build_model(keypoint_count, pretrained=True):
    import torch.nn as nn
    import torch.nn.functional as functional
    from torchvision.models import ResNet18_Weights, resnet18

    class ResNet18KeypointNet(nn.Module):
        def __init__(self):
            super().__init__()
            encoder = resnet18(weights=ResNet18_Weights.DEFAULT if pretrained else None)
            self.encoder = nn.Sequential(encoder.conv1, encoder.bn1, encoder.relu, encoder.maxpool,
                                         encoder.layer1, encoder.layer2, encoder.layer3, encoder.layer4)
            self.decoder = nn.Sequential(
                nn.Conv2d(512, 256, 3, padding=1), nn.ReLU(inplace=True),
                nn.Conv2d(256, 128, 3, padding=1), nn.ReLU(inplace=True),
                nn.Conv2d(128, keypoint_count, 1))

        def forward(self, images):
            features = self.encoder(images)
            heatmaps = self.decoder(features)
            return functional.interpolate(heatmaps, size=(56, 56), mode="bilinear", align_corners=False)

    return ResNet18KeypointNet()
