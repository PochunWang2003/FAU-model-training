#!/usr/bin/env python3
"""Train a pretrained ResNet18 keypoint heatmap model for a selected camera role."""

import argparse
import json
import math
import os
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, os.path.join(ROOT, "ML/common"))
sys.path.insert(0, os.path.dirname(__file__))


def emit(event, **values):
    print(json.dumps({"event": event, "timestamp": time.time(), **values}, separators=(",", ":")), flush=True)


def train(args):
    emit("TRAINING_STARTED", run_id=args.run_id, task="fau", architecture="resnet18_keypoint", role=args.role, pretrained=True)
    try:
        import torch
        from torch.utils.data import DataLoader, Dataset
        from model import build_model
        from torch_images import load_rgb_tensor
    except Exception as error:
        raise RuntimeError("ResNet18 FAU training requires compatible PyTorch and torchvision: " + str(error)) from error

    with open(os.path.join(args.dataset, "annotations.jsonl"), encoding="utf-8") as stream:
        annotations = [json.loads(line) for line in stream if line.strip()]
    annotations = [row for row in annotations if row.get("role") == args.role]
    keypoint_ids = sorted({str(feature["id"]) for row in annotations for feature in row.get("features", [])
                           if feature.get("valid", True) and str(feature.get("id", "")).strip()})
    if not annotations or not keypoint_ids:
        raise ValueError("selected FAU camera role has no valid annotated keypoints")
    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA was selected but is unavailable; select CPU in the UI")
    keypoint_index = {name: index for index, name in enumerate(keypoint_ids)}
    split_by_image = {}
    manifest_path = os.path.join(args.dataset, "manifest.jsonl")
    if os.path.isfile(manifest_path):
        with open(manifest_path, encoding="utf-8") as stream:
            split_by_image = {row["image"]: row.get("split", "train") for row in map(json.loads, stream) if row.get("image")}
    train_rows = [row for row in annotations if split_by_image.get(row["image"], "train") == "train"]
    validation_rows = [row for row in annotations if split_by_image.get(row["image"], "train") == "validation"]
    if not train_rows or not validation_rows:
        raise ValueError("FAU training requires at least one annotated train image and one validation image for the selected role")

    class KeypointDataset(Dataset):
        def __init__(self, rows):
            self.rows = rows

        def __len__(self):
            return len(self.rows)

        def __getitem__(self, index):
            row = self.rows[index]
            image_path = os.path.join(args.dataset, row["image"])
            image_tensor = load_rgb_tensor(image_path)[0]
            width, height = 224, 224
            target = torch.zeros((len(keypoint_ids), 56, 56), dtype=torch.float32)
            mask = torch.zeros((len(keypoint_ids), 1, 1), dtype=torch.float32)
            for feature in row.get("features", []):
                name = str(feature.get("id", ""))
                if name not in keypoint_index or not feature.get("valid", True):
                    continue
                x = float(feature["x"]) * 224.0 / int(row.get("image_width", 160))
                y = float(feature["y"]) * 224.0 / int(row.get("image_height", 120))
                gx, gy = x * 56.0 / width, y * 56.0 / height
                channel = keypoint_index[name]
                for iy in range(max(0, int(gy) - 6), min(56, int(gy) + 7)):
                    for ix in range(max(0, int(gx) - 6), min(56, int(gx) + 7)):
                        target[channel, iy, ix] = max(float(target[channel, iy, ix]),
                            math.exp(-((ix-gx)**2 + (iy-gy)**2) / (2.0 * 2.0**2)))
                mask[channel, 0, 0] = 1.0
            return image_tensor, target, mask, row

    def collate_samples(batch):
        return (torch.stack([item[0] for item in batch]), torch.stack([item[1] for item in batch]),
                torch.stack([item[2] for item in batch]), [item[3] for item in batch])

    train_loader = DataLoader(KeypointDataset(train_rows), batch_size=args.batch_size, shuffle=True, collate_fn=collate_samples)
    validation_loader = DataLoader(KeypointDataset(validation_rows), batch_size=args.batch_size, shuffle=False, collate_fn=collate_samples)
    device = torch.device(args.device)
    model = build_model(len(keypoint_ids)).to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.learning_rate)
    best_loss = float("inf")
    best_state = None
    started = time.monotonic()
    for epoch in range(1, args.epochs + 1):
        model.train()
        loss_sum = 0.0
        count = 0
        for images, targets, masks, _ in train_loader:
            images, targets, masks = images.to(device), targets.to(device), masks.to(device)
            optimizer.zero_grad(set_to_none=True)
            heatmaps = model(images).sigmoid()
            loss = (((heatmaps - targets) ** 2) * masks).sum() / (masks.sum() * 56 * 56).clamp_min(1.0)
            loss.backward()
            optimizer.step()
            loss_sum += loss.item()
            count += 1
        model.eval()
        val_loss = 0.0
        pixel_error_sum = 0.0
        point_count = 0
        with torch.no_grad():
            for images, targets, masks, rows in validation_loader:
                images, targets, masks = images.to(device), targets.to(device), masks.to(device)
                heatmaps = model(images).sigmoid()
                val_loss += ((((heatmaps - targets) ** 2) * masks).sum() / (masks.sum() * 56 * 56).clamp_min(1.0)).item()
                for batch_index, row in enumerate(rows):
                    for feature in row.get("features", []):
                        name = str(feature.get("id", ""))
                        if name not in keypoint_index or not feature.get("valid", True):
                            continue
                        channel = keypoint_index[name]
                        position = int(heatmaps[batch_index, channel].argmax().item())
                        predicted_x = (position % 56) * 224.0 / 56
                        predicted_y = (position // 56) * 224.0 / 56
                        expected_x = float(feature["x"]) * 224.0 / int(row.get("image_width", 160))
                        expected_y = float(feature["y"]) * 224.0 / int(row.get("image_height", 120))
                        pixel_error_sum += math.hypot(predicted_x - expected_x, predicted_y - expected_y)
                        point_count += 1
        val_loss /= max(1, len(validation_loader))
        mean_error = pixel_error_sum / max(1, point_count)
        if val_loss < best_loss:
            best_loss = val_loss
            best_state = {key: value.detach().cpu().clone() for key, value in model.state_dict().items()}
        emit("TRAINING_PROGRESS", run_id=args.run_id, epoch=epoch, epochs=args.epochs,
             train_loss=loss_sum / max(1,count), validation_loss=val_loss,
             mean_pixel_error=mean_error, valid_keypoints=point_count)
        if os.path.exists(args.cancel_file):
            emit("TRAINING_CANCELLED", run_id=args.run_id, epoch=epoch)
            return 2
    os.makedirs(os.path.dirname(args.checkpoint), exist_ok=True)
    torch.save({"architecture":"resnet18_keypoint", "pretrained":"ImageNet ResNet18_Weights.DEFAULT",
                "camera_role":args.role, "keypoint_ids":keypoint_ids, "input_size":224, "heatmap_size":56,
                "calibration":"none", "state_dict":best_state}, args.checkpoint)
    result = {"run_id":args.run_id,"task":"fau","architecture":"resnet18_keypoint","pretrained":True,
              "camera_role":args.role,"status":"COMPLETED","device":args.device,
              "duration_seconds":time.monotonic()-started,"checkpoint":args.checkpoint,
              "metrics":{"validation_loss":best_loss,"mean_pixel_error":mean_error,"valid_keypoints":point_count}}
    with open(args.result,"w",encoding="utf-8") as stream:
        json.dump(result,stream,indent=2)
    emit("TRAINING_COMPLETED", **result)
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset",required=True)
    parser.add_argument("--run-id",required=True)
    parser.add_argument("--role",choices=("top","side"),default="top")
    parser.add_argument("--epochs",type=int,default=30)
    parser.add_argument("--batch-size",type=int,default=4)
    parser.add_argument("--learning-rate",type=float,default=0.0001)
    parser.add_argument("--device",choices=("cpu","cuda"),default="cpu")
    parser.add_argument("--checkpoint",required=True)
    parser.add_argument("--result",required=True)
    parser.add_argument("--cancel-file",required=True)
    args=parser.parse_args()
    try:
        return train(args)
    except Exception as error:
        emit("TRAINING_FAILED",run_id=args.run_id,error=str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
