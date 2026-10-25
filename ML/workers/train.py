#!/usr/bin/env python3
"""Supervised CPU worker. Progress is one JSON object per stdout line."""

import argparse
import json
import math
import os
import random
import sys
import time
import uuid

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
from image_features import focus_features, read_pgm


def emit(event, **payload):
    message = {"event": event, "timestamp": time.time(), **payload}
    print(json.dumps(message, separators=(",", ":")), flush=True)


def load_focus_samples(dataset_path):
    manifest = os.path.join(dataset_path, "manifest.jsonl")
    samples = []
    with open(manifest, encoding="utf-8") as stream:
        for line in stream:
            record = json.loads(line)
            label = record.get("label")
            split = record.get("split", "train")
            if label not in ("sharp", "blurred"):
                continue
            samples.append((focus_features(os.path.join(dataset_path, record["image"])),
                            1.0 if label == "sharp" else 0.0, split))
    return samples


def train_focus(args):
    samples = load_focus_samples(args.dataset)
    train = [item for item in samples if item[2] == "train"]
    validation = [item for item in samples if item[2] == "validation"]
    if len(train) < 2 or not validation or len({item[1] for item in train}) < 2:
        raise ValueError("focus classification needs both labels in train and at least one validation sample")
    dimensions = len(train[0][0])
    means = [sum(row[0][i] for row in train) / len(train) for i in range(dimensions)]
    scales = [math.sqrt(sum((row[0][i] - means[i]) ** 2 for row in train) / len(train)) or 1.0
              for i in range(dimensions)]
    weights = [0.0] * dimensions
    bias = 0.0
    best_loss = float("inf")
    best = None
    started = time.monotonic()
    emit("TRAINING_STARTED", run_id=args.run_id, task="focus", samples=len(samples), epochs=args.epochs)
    for epoch in range(1, args.epochs + 1):
        random.Random(epoch).shuffle(train)
        loss_sum = 0.0
        for features, target, _ in train:
            vector = [(features[i] - means[i]) / scales[i] for i in range(dimensions)]
            logit = max(-30.0, min(30.0, sum(weights[i] * vector[i] for i in range(dimensions)) + bias))
            prediction = 1.0 / (1.0 + math.exp(-logit))
            error = prediction - target
            loss_sum += -(target * math.log(max(prediction, 1e-12)) + (1.0 - target) * math.log(max(1.0 - prediction, 1e-12)))
            for i in range(dimensions):
                weights[i] -= args.learning_rate * error * vector[i] / len(train)
            bias -= args.learning_rate * error / len(train)
        val_loss = 0.0
        correct = 0
        for features, target, _ in validation:
            vector = [(features[i] - means[i]) / scales[i] for i in range(dimensions)]
            logit = max(-30.0, min(30.0, sum(weights[i] * vector[i] for i in range(dimensions)) + bias))
            prediction = 1.0 / (1.0 + math.exp(-logit))
            val_loss += -(target * math.log(max(prediction, 1e-12)) + (1.0 - target) * math.log(max(1.0 - prediction, 1e-12)))
            correct += int((prediction >= 0.5) == bool(target))
        val_loss /= len(validation)
        train_loss = loss_sum / len(train)
        if val_loss < best_loss:
            best_loss = val_loss
            best = {"weights": weights[:], "bias": bias, "means": means, "scales": scales}
        emit("TRAINING_PROGRESS", run_id=args.run_id, epoch=epoch, epochs=args.epochs,
             train_loss=train_loss, validation_loss=val_loss,
             accuracy=correct / len(validation), best_validation_loss=best_loss)
        if os.path.exists(args.cancel_file):
            emit("TRAINING_CANCELLED", run_id=args.run_id, epoch=epoch)
            return 2
        time.sleep(0.025)
    os.makedirs(os.path.dirname(args.checkpoint), exist_ok=True)
    with open(args.checkpoint, "w", encoding="utf-8") as stream:
        json.dump({"format":"fau-focus-model-v1", "task":"sharp-blurred-classification",
                   "features":["log_variance_of_laplacian", "log_tenengrad", "intensity_variance"],
                   **best}, stream, indent=2)
    result = {"run_id":args.run_id, "task":"focus", "status":"COMPLETED",
              "duration_seconds":time.monotonic()-started, "checkpoint":args.checkpoint,
              "metrics":{"validation_loss":best_loss, "accuracy":correct/len(validation),
                         "train_samples":len(train), "validation_samples":len(validation)}}
    with open(args.result, "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
    emit("TRAINING_COMPLETED", **result)
    return 0


def train_fau(args):
    annotations_path = os.path.join(args.dataset, "annotations.jsonl")
    annotations = []
    with open(annotations_path, encoding="utf-8") as stream:
        annotations = [json.loads(line) for line in stream if line.strip()]
    if not annotations:
        raise ValueError("FAU baseline needs at least one saved annotation")
    valid = 0
    errors = []
    for item in annotations:
        image_path = os.path.join(args.dataset, item["image"])
        width, height, pixels = read_pgm(image_path)
        roi = item.get("roi", {"x":0,"y":0,"width":width,"height":height})
        x0 = max(1, int(roi["x"]))
        y0 = max(1, int(roi["y"]))
        x1 = min(width - 1, int(roi["x"] + roi["width"]))
        y1 = min(height - 1, int(roi["y"] + roi["height"]))
        weight_sum = 0.0
        weighted_x = 0.0
        weighted_y = 0.0
        for y in range(y0, y1):
            for x in range(x0, x1):
                weight = abs(pixels[y * width + x + 1] - pixels[y * width + x - 1])
                weight += abs(pixels[(y + 1) * width + x] - pixels[(y - 1) * width + x])
                weight_sum += weight
                weighted_x += x * weight
                weighted_y += y * weight
        for point in item.get("features", []):
            x = float(point["x"])
            y = float(point["y"])
            if 0 <= x < width and 0 <= y < height and roi["x"] <= x < roi["x"] + roi["width"] and roi["y"] <= y < roi["y"] + roi["height"]:
                valid += 1
                if weight_sum > 0.0:
                    predicted_x = weighted_x / weight_sum
                    predicted_y = weighted_y / weight_sum
                    errors.append(math.hypot(predicted_x - x, predicted_y - y))
    if valid == 0:
        raise ValueError("FAU annotations have no valid keypoints within image and ROI bounds")
    emit("TRAINING_STARTED", run_id=args.run_id, task="fau-baseline", annotations=len(annotations))
    for epoch in range(1, max(1, args.epochs) + 1):
        if os.path.exists(args.cancel_file):
            emit("TRAINING_CANCELLED", run_id=args.run_id, epoch=epoch)
            return 2
        emit("TRAINING_PROGRESS", run_id=args.run_id, epoch=epoch, epochs=max(1,args.epochs),
             valid_keypoints=valid, mean_pixel_error=sum(errors) / len(errors) if errors else None,
             note="deterministic gradient-centroid baseline evaluation; no neural training")
        time.sleep(0.025)
    checkpoint = {"format":"fau-landmark-baseline-v1", "algorithm":"gradient-centroid",
                  "calibration":"none", "annotations":len(annotations), "valid_keypoints":valid,
                  "mean_pixel_error":sum(errors) / len(errors) if errors else None}
    os.makedirs(os.path.dirname(args.checkpoint), exist_ok=True)
    with open(args.checkpoint, "w", encoding="utf-8") as stream:
        json.dump(checkpoint, stream, indent=2)
    result = {"run_id":args.run_id,"task":"fau-baseline","status":"COMPLETED",
             "checkpoint":args.checkpoint,"metrics":{"annotations":len(annotations),"valid_keypoints":valid,
             "mean_pixel_error":sum(errors) / len(errors) if errors else None},
             "warning":"This validates annotation persistence and a deterministic baseline, not a trained detector."}
    with open(args.result, "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
    emit("TRAINING_COMPLETED", **result)
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--task", choices=("focus", "fau"), required=True)
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--run-id", default=lambda: str(uuid.uuid4()))
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--learning-rate", type=float, default=0.05)
    parser.add_argument("--checkpoint", required=True)
    parser.add_argument("--result", required=True)
    parser.add_argument("--cancel-file", required=True)
    args = parser.parse_args()
    try:
        return train_focus(args) if args.task == "focus" else train_fau(args)
    except Exception as error:
        emit("TRAINING_FAILED", run_id=args.run_id, error=str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
