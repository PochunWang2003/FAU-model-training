#!/usr/bin/env python3
"""Train ImageNet-pretrained MobileNetV3-Small from a versioned focus dataset."""

import argparse
import json
import os
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, os.path.join(ROOT, "ML/common"))
sys.path.insert(0, os.path.dirname(__file__))


def emit(event, **values):
    print(json.dumps({"event": event, "timestamp": time.time(), **values}, separators=(",", ":")), flush=True)


def train(args):
    emit("TRAINING_STARTED", run_id=args.run_id, task="focus", architecture="mobilenet_v3_small", pretrained=True)
    try:
        import torch
        from torch.utils.data import DataLoader, Dataset
        from model import build_model
        from torch_images import load_rgb_tensor
    except Exception as error:
        raise RuntimeError("MobileNetV3-Small pretrained training requires compatible PyTorch and torchvision: " + str(error)) from error

    with open(os.path.join(args.dataset, "manifest.jsonl"), encoding="utf-8") as stream:
        records = [json.loads(line) for line in stream if line.strip()]
    label_index = {"blurred": 0, "sharp": 1}
    records = [row for row in records if row.get("label") in label_index and row.get("split") in ("train", "validation", "test")]
    train_rows = [row for row in records if row["split"] == "train"]
    validation_rows = [row for row in records if row["split"] == "validation"]
    if len(train_rows) < 2 or not validation_rows or len({row["label"] for row in train_rows}) < 2:
        raise ValueError("MobileNet focus training needs both labels in train and labeled validation samples")
    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA was selected but is unavailable; select CPU in the UI")

    class FocusDataset(Dataset):
        def __init__(self, rows):
            self.rows = rows

        def __len__(self):
            return len(self.rows)

        def __getitem__(self, index):
            row = self.rows[index]
            image = load_rgb_tensor(os.path.join(args.dataset, row["image"]))[0]
            return image, label_index[row["label"]]

    train_loader = DataLoader(FocusDataset(train_rows), batch_size=args.batch_size, shuffle=True)
    validation_loader = DataLoader(FocusDataset(validation_rows), batch_size=args.batch_size, shuffle=False)
    device = torch.device(args.device)
    try:
        model = build_model().to(device)
    except Exception as error:
        raise RuntimeError("Pretrained weights not installed or could not be loaded: " + str(error)) from error
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.learning_rate)
    loss_function = torch.nn.CrossEntropyLoss()
    best_loss = float("inf")
    best_state = None
    started = time.monotonic()
    for epoch in range(1, args.epochs + 1):
        model.train()
        train_loss = 0.0
        train_count = 0
        for images, labels in train_loader:
            images, labels = images.to(device), labels.to(device)
            optimizer.zero_grad(set_to_none=True)
            loss = loss_function(model(images), labels)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * labels.size(0)
            train_count += labels.size(0)
        model.eval()
        validation_loss = 0.0
        correct = 0
        validation_count = 0
        with torch.no_grad():
            for images, labels in validation_loader:
                images, labels = images.to(device), labels.to(device)
                logits = model(images)
                validation_loss += loss_function(logits, labels).item() * labels.size(0)
                correct += (logits.argmax(1) == labels).sum().item()
                validation_count += labels.size(0)
        validation_loss /= validation_count
        accuracy = correct / validation_count
        if validation_loss < best_loss:
            best_loss = validation_loss
            best_state = {key: value.detach().cpu().clone() for key, value in model.state_dict().items()}
        emit("TRAINING_PROGRESS", run_id=args.run_id, epoch=epoch, epochs=args.epochs,
             train_loss=train_loss / train_count, validation_loss=validation_loss, accuracy=accuracy,
             best_validation_loss=best_loss)
        if os.path.exists(args.cancel_file):
            emit("TRAINING_CANCELLED", run_id=args.run_id, epoch=epoch)
            return 2
    os.makedirs(os.path.dirname(args.checkpoint), exist_ok=True)
    torch.save({"architecture":"mobilenet_v3_small", "pretrained":"ImageNet MobileNet_V3_Small_Weights.DEFAULT",
                "labels":label_index, "state_dict":best_state}, args.checkpoint)
    result = {"run_id":args.run_id, "task":"focus", "architecture":"mobilenet_v3_small", "pretrained":True,
              "device":args.device, "status":"COMPLETED", "duration_seconds":time.monotonic()-started,
              "checkpoint":args.checkpoint, "metrics":{"validation_loss":best_loss,"accuracy":accuracy,
              "train_samples":len(train_rows),"validation_samples":len(validation_rows)}}
    with open(args.result, "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
    emit("TRAINING_COMPLETED", **result)
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--learning-rate", type=float, default=0.0001)
    parser.add_argument("--device", choices=("cpu", "cuda"), default="cpu")
    parser.add_argument("--checkpoint", required=True)
    parser.add_argument("--result", required=True)
    parser.add_argument("--cancel-file", required=True)
    args = parser.parse_args()
    try:
        return train(args)
    except Exception as error:
        emit("TRAINING_FAILED", run_id=args.run_id, error=str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
