# Focus workflow

The UI can create a synthetic demonstration dataset, import a folder, capture a synthetic sample, change sharp/blurred labels, request a traditional focus score, and start/cancel a CPU training run.

The traditional score is variance of the Laplacian and is clearly reported as a non-model score. The trainable prototype is standard-library logistic regression over three features: log variance of Laplacian, log Tenengrad, and normalized intensity variance. It reports binary cross-entropy losses and validation accuracy. Its checkpoint is JSON (`fau-focus-model-v1`), not PyTorch. Check cancellation is performed between epochs. Resume, CUDA, regression, inference, and image preview of arbitrary imported files are not implemented.

Synthetic examples are demonstration-only. Accuracy on them says nothing about production focus performance.
