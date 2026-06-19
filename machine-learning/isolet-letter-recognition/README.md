# ISOLET Letter Recognition

CUDA/C implementation for English-letter recognition using ISOLET speech-feature data. The public folder keeps the source code and headers needed to study the implementation while excluding the dataset, generated binaries, model files, logs, and original compressed archives.

## Contents

- `src/main.c`: training and evaluation entry point
- `src/neural_net.cu`: CUDA neural-network implementation
- `src/isolet_data.c`: ISOLET data loading and preprocessing helpers
- `src/utils.c`: utility functions
- `include/`: public headers
- `Makefile`: build entry point

## Public Boundary

The original ISOLET data files and generated model artifacts are not included because they are large runtime assets rather than source code.
