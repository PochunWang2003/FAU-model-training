# Dataset format

Dataset roots are `data/focus/<id>/` and `data/fau/<id>/`. IDs are restricted to ASCII letters, digits, underscore, and hyphen. Images are immutable grayscale PGM files. `manifest.jsonl` has one JSON record per image with image filename, logical camera role, timestamp, label (nullable), split (`train`, `validation`, or `test`), schema version, and a synthetic-demo marker when applicable.

Focus labels are `sharp` and `blurred`. Dataset creation makes a demonstration-only synthetic set with explicit train/validation assignments. Imported images begin unlabeled in the train split. `DATASET_LABEL` changes one focus label and `DATASET_SPLIT` changes one split.

FAU keypoints are stored in append-only `annotations.jsonl` records with image, role, ROI, feature identifiers, pixel coordinates, validity/confidence, timestamp, and schema version. Physical mm/degree calibration is deliberately not part of these annotations.

Near-duplicate grouping and leakage-resistant sequence split tooling are not yet implemented. Review splits before drawing conclusions from evaluation metrics.
