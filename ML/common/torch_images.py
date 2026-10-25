"""Image decoding for the PGM datasets used by the optional PyTorch workers."""

from image_features import read_pgm


def load_rgb_tensor(path, image_size=224):
    import torch
    import torch.nn.functional as functional

    width, height, pixels = read_pgm(path)
    image = torch.tensor(pixels, dtype=torch.float32).view(1, 1, height, width) / 255.0
    image = image.repeat(1, 3, 1, 1)
    image = functional.interpolate(image, size=(image_size, image_size), mode="bilinear", align_corners=False)
    mean = torch.tensor([0.485, 0.456, 0.406], dtype=image.dtype).view(1, 3, 1, 1)
    std = torch.tensor([0.229, 0.224, 0.225], dtype=image.dtype).view(1, 3, 1, 1)
    return (image - mean) / std
