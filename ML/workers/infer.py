#!/usr/bin/env python3
"""One-image inference worker for trained checkpoints and explicit demo baselines."""

import argparse
import json
import math
import os
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, os.path.join(ROOT, "ML/common"))


def emit(event, **values):
    print(json.dumps({"event": event, "timestamp": time.time(), **values}, separators=(",", ":")), flush=True)


def infer_focus(args):
    if args.checkpoint.endswith(".json"):
        from image_features import focus_features
        with open(args.checkpoint, encoding="utf-8") as stream:
            model = json.load(stream)
        features = focus_features(args.image)
        vector = [(features[i] - model["means"][i]) / model["scales"][i] for i in range(len(features))]
        logit = max(-30.0, min(30.0, sum(model["weights"][i] * vector[i] for i in range(len(vector))) + model["bias"]))
        probability = 1.0 / (1.0 + math.exp(-logit))
        label = "sharp" if probability >= 0.5 else "blurred"
        return {"architecture":"statistical-logistic-regression", "label":label,
                "confidence":probability if label == "sharp" else 1.0 - probability,
                "sharp_probability":probability, "traditional_focus_score":math.expm1(features[0])}
    try:
        import torch
        from torch import nn
        from torchvision.models import mobilenet_v3_small
        from torch_images import load_rgb_tensor
    except Exception as error:
        raise RuntimeError("MobileNet inference requires compatible PyTorch and torchvision: " + str(error)) from error
    state = torch.load(args.checkpoint, map_location="cpu", weights_only=True)
    model = mobilenet_v3_small(weights=None)
    final_layer = model.classifier[-1]
    model.classifier[-1] = nn.Linear(final_layer.in_features, 2)
    model.load_state_dict(state["state_dict"])
    model.eval()
    with torch.no_grad():
        probabilities = model(load_rgb_tensor(args.image)).softmax(dim=1)[0]
    index = int(probabilities.argmax().item())
    label = "blurred" if index == 0 else "sharp"
    return {"architecture":"mobilenet_v3_small", "label":label, "confidence":float(probabilities[index]),
            "sharp_probability":float(probabilities[1])}


def infer_fau(args):
    if args.checkpoint.endswith(".json"):
        from image_features import read_pgm
        width, height, pixels = read_pgm(args.image)
        sum_weight = sum_x = sum_y = 0.0
        for y in range(1, height - 1):
            for x in range(1, width - 1):
                weight = abs(pixels[y * width + x + 1] - pixels[y * width + x - 1])
                weight += abs(pixels[(y + 1) * width + x] - pixels[(y - 1) * width + x])
                sum_weight += weight
                sum_x += x * weight
                sum_y += y * weight
        if sum_weight <= 0:
            raise ValueError("image contains no gradient signal for the baseline detector")
        return {"architecture":"gradient-centroid", "camera_role":args.role, "calibration":"none",
                "predictions":[{"id":"gradient_centroid", "x":sum_x/sum_weight, "y":sum_y/sum_weight,
                                "confidence":min(1.0, sum_weight/(width*height*255.0))}]}
    try:
        import torch
        from model import build_model
        from torch_images import load_rgb_tensor
    except Exception as error:
        raise RuntimeError("ResNet18 inference requires compatible PyTorch and torchvision: " + str(error)) from error
    state = torch.load(args.checkpoint, map_location="cpu", weights_only=True)
    if state.get("camera_role") != args.role:
        raise ValueError("checkpoint camera role does not match selected image role")
    keypoint_ids = state["keypoint_ids"]
    model = build_model(len(keypoint_ids), pretrained=False)
    model.load_state_dict(state["state_dict"])
    model.eval()
    from image_features import read_pgm
    width, height, _ = read_pgm(args.image)
    with torch.no_grad():
        heatmaps = model(load_rgb_tensor(args.image)).sigmoid()[0]
    predictions = []
    for index, name in enumerate(keypoint_ids):
        position = int(heatmaps[index].argmax().item())
        confidence = float(heatmaps[index].flatten()[position].item())
        predictions.append({"id":name,"x":(position % 56) * width / 56.0,
                            "y":(position // 56) * height / 56.0,"confidence":confidence})
    return {"architecture":"resnet18_keypoint", "camera_role":args.role, "calibration":"none",
            "predictions":predictions}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--task", choices=("focus","fau"), required=True)
    parser.add_argument("--checkpoint", required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--role", choices=("top","side"), default="top")
    args = parser.parse_args()
    try:
        result = infer_focus(args) if args.task == "focus" else infer_fau(args)
        emit("INFERENCE_RESULT", **result)
        return 0
    except Exception as error:
        emit("INFERENCE_FAILED", error=str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
