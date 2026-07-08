# Benchmark

## Requirements
- Build `DirectXSplatBench`.
  ```bash
  $ cmake -S . -B build/bench -DDIRECTXSPLAT_BUILD_BENCHES=ON -DDIRECTXSPLAT_BUILD_TESTS=OFF
  $ cmake --build build/bench --config Release --target DirectXSplatBench
  ```
- Install `torch` with a CUDA version matching your local setup.
- Then install `gsplat` and other dependencies.
  ```bash
  $ pip install -r ./benches/requirements.txt
  ```

## Usage
```bash
$ python ./benches/bench.py --target all --mode both --ply_path models/bicycle/point_cloud/iteration_30000/point_cloud.ply --colmap_path datasets/mipnerf360/bicycle --scale 4
```

## Examples
```bash
$ python .\benches\bench.py --ply_path models\train\point_cloud\iteration_30000\point_cloud.ply --camera_json models\train\cameras.json --scale 1 --target directxsplat --mode hot-render --no_psnr --build_dir build\bench
...
GPU FPS: 399.50

$ python .\benches\bench.py --ply_path models\train\point_cloud\iteration_30000\point_cloud.ply --camera_json models\train\cameras.json --scale 1 --target gsplat --mode hot-render --no_psnr
...
GPU FPS: 177.70

$ python .\benches\bench.py --ply_path models\bicycle\point_cloud\iteration_30000\point_cloud.ply --colmap_path datasets\mipnerf360\bicycle --scale 4 --target gsplat-radius-sweep --target_psnr 19.17 --radius_clips 0.0,0.25,0.5,1.0,1.5,2.0,3.0,4.0
...
radius_clip 1.00, PSNR: 19.17 +/- 1.60, FPS: 167.22
```

## Results
- Test environment:
  - NVIDIA GeForce RTX 4070 SUPER, 12282 MiB VRAM
  - NVIDIA driver 610.62
  - CUDA UMD 13.3
  - AMD Ryzen 7 7800X3D 8-Core Processor
  - CPU memory 17GB (total 32GB, used 15GB constantly on background)
  - Windows 11 Home 25H2, build 26200
  - Python 3.10.20
  - CUDA toolkit compiler not on PATH
  - CMake 4.2.0
  - torch 2.4.0+cu124
  - torch CUDA 12.4
  - gsplat 1.5.3+pt24cu124
- Notes:
  - FPS is resident-scene, single-camera GPU render throughput.
  - Scene loading, initial upload, warmup, CPU readback, UI, and presentation are excluded from FPS.
  - Quality/PSNR is evaluated separately over the same camera set when reference images are available.
  - DirectXSplat creates one uploaded scene before warmup and destroys it after timing and quality finish.
  - DirectXSplat uses `PrepareSceneForRender(...)` and `Render(...)` for each timed camera.
  - gsplat keeps Gaussian tensors and cameras resident on CUDA before timing.
  - gsplat is run with one camera per `gsplat.rasterization()` call; DirectXSplat has no chunk-size parameter in this benchmark.
  - Matched-quality gsplat results start with the listed `radius_clip` values, then refine the bracket and report the closest tuned PSNR row.
  - Matched-quality delta is `gsplat PSNR - DirectXSplat PSNR`.
  - Warmup sweeps = 2.
  - Measurement sweeps = 5.
  - near = 0.1.
  - far = 1000.

## Dataset
  | Dataset  | #imgs | scale | resolution | #points |
  |:---------|:-----:|:-----:|:----------:|--------:|
  | bicycle  | 194   |     4 |   1237x822 | 6131954 |
  | bonsai   | 292   |     2 |  1559x1039 | 1244819 |
  | counter  | 240   |     2 |  1558x1038 | 1222956 |
  | drjohnson| 263   |     1 |   1332x876 | 3405153 |
  | flowers  | 173   |     4 |   1256x828 | 3636448 |
  | garden   | 185   |     4 |   1297x840 | 5834784 |
  | kitchen  | 279   |     2 |  1558x1039 | 1852335 |
  | playroom | 225   |     1 |   1264x832 | 2546116 |
  | room     | 311   |     2 |  1557x1038 | 1593376 |
  | stump    | 125   |     4 |   1245x825 | 4961797 |
  | train    | 301   |     1 |    980x545 | 1026508 |
  | treehill | 141   |     4 |   1267x832 | 3783761 |
  | truck    | 251   |     1 |    979x546 | 2541226 |

## Default Benchmark Result
  | Implementation | Dataset  | PSNR | FPS |
  |:---------------|:---------|:----:|---:|
  | DirectXSplat | bicycle  | 19.17 +/- 1.71 | **382.51** |
  | gsplat       | bicycle  | **19.18 +/- 1.60** | 158.24 |
  | DirectXSplat | bonsai   | 30.11 +/- 1.90 | **681.10** |
  | gsplat       | bonsai   | **30.91 +/- 2.35** | 275.38 |
  | DirectXSplat | counter  | 28.63 +/- 1.43 | **485.58** |
  | gsplat       | counter  | **29.35 +/- 1.40** | 179.61 |
  | DirectXSplat | drjohnson| 33.28 +/- 2.66 | **524.46** |
  | gsplat       | drjohnson| **34.35 +/- 3.01** | 232.66 |
  | DirectXSplat | flowers  | 19.27 +/- 0.87 | **453.45** |
  | gsplat       | flowers  | **19.80 +/- 0.75** | 170.80 |
  | DirectXSplat | garden   | **18.99 +/- 0.75** | **319.14** |
  | gsplat       | garden   | 18.96 +/- 0.74 | 124.06 |
  | DirectXSplat | kitchen  | 29.22 +/- 1.77 | **348.41** |
  | gsplat       | kitchen  | **29.69 +/- 1.88** | 123.02 |
  | DirectXSplat | playroom | 33.54 +/- 2.68 | **676.91** |
  | gsplat       | playroom | **34.56 +/- 3.14** | 291.03 |
  | DirectXSplat | room     | 30.80 +/- 1.56 | **571.45** |
  | gsplat       | room     | **32.37 +/- 1.88** | 249.13 |
  | DirectXSplat | stump    | 23.91 +/- 1.85 | **468.19** |
  | gsplat       | stump    | **24.06 +/- 1.83** | 192.80 |
  | DirectXSplat | train    | 20.84 +/- 2.14 | **766.11** |
  | gsplat       | train    | **22.29 +/- 2.41** | 282.22 |
  | DirectXSplat | treehill | 20.24 +/- 2.64 | **491.86** |
  | gsplat       | treehill | **20.30 +/- 2.70** | 194.31 |
  | DirectXSplat | truck    | 19.67 +/- 1.58 | **651.56** |
  | gsplat       | truck    | **20.59 +/- 1.66** | 263.62 |

## Tuned Benchmark Result
  | Dataset | DirectXSplat PSNR | DirectXSplat FPS | gsplat radius_clip | gsplat PSNR | delta | gsplat FPS |
  |:--------|:-----------------:|-----------------:|:------------------:|:-----------:|------:|-----------:|
  | bicycle   | 19.17 +/- 1.71 | 382.51 | 1.0000 | 19.17 +/- 1.60 | +0.002 | 165.82 |
  | bonsai    | 30.11 +/- 1.90 | 681.10 | 3.0000 | 29.87 +/- 2.16 | -0.242 | 278.57 |
  | counter   | 28.63 +/- 1.43 | 485.58 | 4.9898 | 28.79 +/- 1.32 | +0.161 | 219.75 |
  | drjohnson | 33.28 +/- 2.66 | 524.46 | 5.0943 | 33.09 +/- 3.21 | -0.184 | 281.51 |
  | flowers   | 19.27 +/- 0.87 | 453.45 | 3.0000 | 18.98 +/- 0.74 | -0.292 | 278.35 |
  | garden    | 18.99 +/- 0.75 | 319.14 | 0.0000 | 18.96 +/- 0.74 | -0.032 | 131.68 |
  | kitchen   | 29.22 +/- 1.77 | 348.41 | 3.0000 | 28.93 +/- 2.02 | -0.286 | 147.79 |
  | playroom  | 33.54 +/- 2.68 | 676.91 | 4.0000 | 33.42 +/- 3.38 | -0.122 | 321.89 |
  | room      | 30.80 +/- 1.56 | 571.45 | 5.3515 | 30.72 +/- 2.18 | -0.075 | 283.55 |
  | stump     | 23.91 +/- 1.85 | 468.19 | 2.0000 | 23.86 +/- 1.87 | -0.055 | 220.39 |
  | train     | 20.84 +/- 2.14 | 766.11 | 5.5564 | 20.98 +/- 2.24 | +0.147 | 498.69 |
  | treehill  | 20.24 +/- 2.64 | 491.86 | 2.0000 | 20.20 +/- 2.69 | -0.044 | 224.86 |
  | truck     | 19.67 +/- 1.58 | 651.56 | 4.0000 | 19.40 +/- 1.78 | -0.274 | 384.74 |
