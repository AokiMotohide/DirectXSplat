from __future__ import annotations

import numpy as np
import torch
import gsplat

from common import load_gaussian_ply


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = int(np.ceil(max(0.0, min(1.0, fraction)) * len(ordered)) - 1)
    return float(ordered[max(0, min(index, len(ordered) - 1))])


def median(values: list[float]) -> float:
    return float(np.median(np.asarray(values, dtype=np.float64))) if values else 0.0


def rasterize_one(means, quats, scales, opacities, colors, viewmats, Ks, index: int, width: int, height: int,
                  degree: int | None, near: float, far: float, rasterize_mode: str, radius_clip: float):
    return gsplat.rasterization(
        means=means,
        quats=quats,
        scales=scales,
        opacities=opacities,
        colors=colors,
        viewmats=viewmats[index : index + 1],
        Ks=Ks[index : index + 1],
        width=width,
        height=height,
        sh_degree=degree,
        near_plane=near,
        far_plane=far,
        radius_clip=radius_clip,
        rasterize_mode=rasterize_mode,
    )


def load_resident_tensors(ply_path, draw_data):
    device = torch.device("cuda")
    ply_data = load_gaussian_ply(ply_path)

    return {
        "means": torch.from_numpy(ply_data["means"].astype(np.float32)).to(device),
        "quats": torch.from_numpy(ply_data["quats"].astype(np.float32)).to(device),
        "scales": torch.from_numpy(ply_data["scales"].astype(np.float32)).to(device),
        "opacities": torch.from_numpy(ply_data["opacities"].astype(np.float32)).to(device),
        "colors": torch.from_numpy(ply_data["colors"].astype(np.float32)).to(device),
        "viewmats": torch.from_numpy(draw_data["viewmats"].astype(np.float32)).to(device),
        "Ks": torch.from_numpy(draw_data["Ks"].astype(np.float32)).to(device),
    }


def run_hot_render(tensors, draw_data, near: float, far: float, sh_degree: int, antialiasing: bool,
                   warmup_sweeps: int, measurement_sweeps: int, radius_clip: float) -> dict:
    width = int(draw_data["width"])
    height = int(draw_data["height"])
    camera_count = int(len(tensors["viewmats"]))
    rasterize_mode = "antialiased" if antialiasing else "classic"
    degree = sh_degree if tensors["colors"].shape[1] > 1 else None

    with torch.inference_mode():
        for _ in range(max(2, warmup_sweeps)):
            for i in range(camera_count):
                rasterize_one(tensors["means"], tensors["quats"], tensors["scales"], tensors["opacities"], tensors["colors"],
                              tensors["viewmats"], tensors["Ks"], i, width, height, degree, near, far, rasterize_mode,
                              radius_clip)
    torch.cuda.synchronize()

    measured_frames = camera_count * measurement_sweeps
    starts = [torch.cuda.Event(enable_timing=True) for _ in range(measured_frames)]
    ends = [torch.cuda.Event(enable_timing=True) for _ in range(measured_frames)]

    with torch.inference_mode():
        frame = 0
        for _ in range(measurement_sweeps):
            for i in range(camera_count):
                starts[frame].record()
                rasterize_one(tensors["means"], tensors["quats"], tensors["scales"], tensors["opacities"], tensors["colors"],
                              tensors["viewmats"], tensors["Ks"], i, width, height, degree, near, far, rasterize_mode,
                              radius_clip)
                ends[frame].record()
                frame += 1
    torch.cuda.synchronize()

    frame_ms = [starts[i].elapsed_time(ends[i]) for i in range(measured_frames)]
    total_gpu_ms = float(sum(frame_ms))
    return {
        "label": "resident-scene single-camera GPU render throughput",
        "frames": measured_frames,
        "camera_count": camera_count,
        "warmup_sweeps": int(max(2, warmup_sweeps)),
        "measurement_sweeps": int(measurement_sweeps),
        "total_gpu_ms": total_gpu_ms,
        "gpu_fps": float(measured_frames * 1000.0 / total_gpu_ms) if total_gpu_ms > 0.0 else 0.0,
        "median_gpu_ms": median(frame_ms),
        "p95_gpu_ms": percentile(frame_ms, 0.95),
        "single_camera_calls": True,
        "radius_clip": float(radius_clip),
    }


def run_quality(tensors, draw_data, near: float, far: float, sh_degree: int, antialiasing: bool,
                radius_clip: float) -> dict:
    width = int(draw_data["width"])
    height = int(draw_data["height"])
    camera_count = int(len(tensors["viewmats"]))
    rasterize_mode = "antialiased" if antialiasing else "classic"
    degree = sh_degree if tensors["colors"].shape[1] > 1 else None

    images = []
    with torch.inference_mode():
        for i in range(camera_count):
            image, _, _ = rasterize_one(
                tensors["means"], tensors["quats"], tensors["scales"], tensors["opacities"], tensors["colors"],
                tensors["viewmats"], tensors["Ks"], i, width, height, degree, near, far, rasterize_mode, radius_clip
            )
            images.append(image[..., :3])
    torch.cuda.synchronize()

    colors = (torch.cat(images).clamp(0.0, 1.0) * 255.0).to(torch.uint8).cpu().numpy()
    return {
        "label": "quality/capture pass",
        "frames": camera_count,
        "colors": colors,
        "images": [],
        "radius_clip": float(radius_clip),
    }


def run_gsplat_benchmark(ply_path, draw_data, near: float, far: float, sh_degree: int, antialiasing: bool,
                         warmup_sweeps: int, measurement_sweeps: int, mode: str, radius_clip: float = 0.0):
    tensors = load_resident_tensors(ply_path, draw_data)
    hot_render = None
    quality = None

    if mode in {"hot-render", "both"}:
        hot_render = run_hot_render(tensors, draw_data, near, far, sh_degree, antialiasing, warmup_sweeps,
                                    measurement_sweeps, radius_clip)
    if mode in {"quality", "both"}:
        quality = run_quality(tensors, draw_data, near, far, sh_degree, antialiasing, radius_clip)

    return {
        "implementation": "gsplat",
        "timing_scope": (
            "Resident-scene, single-camera GPU render throughput. Scene loading, initial upload, warmup, CPU readback, "
            "UI, and presentation excluded. PSNR evaluated separately over the same camera set."
        ),
        "splats": int(tensors["means"].shape[0]),
        "width": int(draw_data["width"]),
        "height": int(draw_data["height"]),
        "frames": int(len(tensors["viewmats"])),
        "camera_count": int(len(tensors["viewmats"])),
        "hot_render": hot_render,
        "quality": quality,
        "chunk": 1,
        "radius_clip": float(radius_clip),
    }
