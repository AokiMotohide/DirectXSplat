from __future__ import annotations

import argparse
import copy
from datetime import datetime
import hashlib
import importlib.metadata
import json
from pathlib import Path
import platform
import subprocess
import sys


TIMING_SCOPE = (
    "Resident-scene, single-camera GPU render throughput. Scene loading, initial upload, warmup, CPU readback, "
    "UI, and presentation excluded. PSNR evaluated separately over the same camera set."
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def find_runner(build_dir: Path | None) -> Path:
    search_root = build_dir if build_dir is not None else repo_root() / "build"
    candidates = sorted(search_root.rglob("DirectXSplatBench.exe"), key=lambda path: path.stat().st_mtime, reverse=True)
    if not candidates:
        raise FileNotFoundError(
            "DirectXSplatBench.exe was not found. Build with -DDIRECTXSPLAT_BUILD_BENCHES=ON and target DirectXSplatBench."
        )
    return candidates[0]


def default_output_dir(ply_path: Path) -> Path:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return repo_root() / "benches" / "outputs" / f"{ply_path.stem}_{stamp}"


def load_draw_data(args):
    from common import load_camera_json_draw_data, load_colmap_draw_data, resize_draw_data

    first = args.first if args.first and args.first > 0 else None
    if args.colmap_path is not None:
        data = load_colmap_draw_data(args.colmap_path, args.scale, first, args.raw_colmap_intrinsics)
        return resize_draw_data(data, args.width, args.height)
    if args.camera_json is not None:
        data = load_camera_json_draw_data(args.camera_json, args.image_dir, first, args.scale)
        return resize_draw_data(data, args.width, args.height)
    raise ValueError("Either --colmap-path or --camera-json is required")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(16 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_git(args: list[str]) -> str | None:
    completed = subprocess.run(["git", *args], cwd=repo_root(), text=True, capture_output=True)
    if completed.returncode != 0:
        return None
    return completed.stdout.strip()


def directxsplat_commit() -> str | None:
    commit = run_git(["rev-parse", "--short", "HEAD"])
    if not commit:
        return None
    dirty = subprocess.run(["git", "diff", "--quiet"], cwd=repo_root(), capture_output=True).returncode != 0
    staged = subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=repo_root(), capture_output=True).returncode != 0
    return f"{commit}-dirty" if dirty or staged else commit


def package_version(package: str) -> str | None:
    try:
        return importlib.metadata.version(package)
    except importlib.metadata.PackageNotFoundError:
        return None


def gsplat_version() -> str | None:
    version = package_version("gsplat")
    if version:
        return version
    try:
        import gsplat

        return getattr(gsplat, "__version__", None)
    except Exception:
        return None


def cuda_device_name() -> str | None:
    try:
        import torch

        if torch.cuda.is_available():
            return torch.cuda.get_device_name(0)
    except Exception:
        return None
    return None


def nvidia_smi_gpu() -> tuple[str | None, str | None]:
    command = ["nvidia-smi", "--query-gpu=name,driver_version", "--format=csv,noheader"]
    try:
        completed = subprocess.run(command, text=True, capture_output=True)
    except FileNotFoundError:
        return None, None
    if completed.returncode != 0 or not completed.stdout.strip():
        return None, None
    first = completed.stdout.strip().splitlines()[0]
    pieces = [piece.strip() for piece in first.split(",", 1)]
    if len(pieces) == 2:
        return pieces[0], pieces[1]
    return pieces[0], None


def system_info(render_summary: dict) -> dict:
    native_gpu = render_summary.get("adapter_name")
    smi_gpu, smi_driver = nvidia_smi_gpu()
    return {
        "gpu": native_gpu or cuda_device_name() or smi_gpu,
        "driver": smi_driver,
        "os": platform.platform(),
    }


def dataset_name(args) -> str:
    if args.dataset:
        return args.dataset
    if args.colmap_path is not None:
        return args.colmap_path.name
    if args.camera_json is not None:
        return args.camera_json.stem
    return args.ply_path.parent.name


def wants_quality(mode: str) -> bool:
    return mode in {"quality", "both"}


def parse_radius_clips(text: str) -> list[float]:
    values = []
    for piece in text.replace(";", ",").split(","):
        piece = piece.strip()
        if piece:
            values.append(float(piece))
    if not values:
        raise ValueError("At least one radius clip value is required")
    return sorted(set(values))


def run_renderer(args, camera_json: Path, output_dir: Path, write_frames: bool) -> dict:
    runner = args.runner if args.runner is not None else find_runner(args.build_dir)
    frames_dir = output_dir / "frames"
    summary_path = output_dir / "render.json"

    command = [
        str(runner),
        "--scene",
        str(args.ply_path),
        "--cameras",
        str(camera_json),
        "--json",
        str(summary_path),
        "--mode",
        args.mode,
        "--near",
        str(args.near),
        "--far",
        str(args.far),
        "--warmup-sweeps",
        str(args.warmup_sweeps),
        "--measurement-sweeps",
        str(args.measurement_sweeps),
        "--render-type",
        args.render_type,
        "--sh-degree",
        str(args.sh_degree),
    ]
    if write_frames:
        command.extend(["--out", str(frames_dir)])
    if not args.aa:
        command.append("--no-aa")
    if args.gamma:
        command.append("--gamma")
    if args.no_fast_culling:
        command.append("--no-fast-culling")
    if args.splat_budget is not None:
        command.extend(["--splat-budget", str(args.splat_budget)])
    if args.scale_modifier != 1.0:
        command.extend(["--scale-modifier", str(args.scale_modifier)])

    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.stdout:
        print(completed.stdout, end="")
    if completed.stderr:
        print(completed.stderr, end="", file=sys.stderr)
    if completed.returncode != 0:
        raise RuntimeError(f"Renderer benchmark failed with exit code {completed.returncode}")
    return json.loads(summary_path.read_text())


def run_gsplat(args, draw_data: dict, output_dir: Path, write_frames: bool) -> dict:
    from draw_gsplat import run_gsplat_benchmark
    from PIL import Image

    result = run_gsplat_benchmark(
        args.ply_path,
        draw_data,
        near=args.near,
        far=args.far,
        sh_degree=args.sh_degree,
        antialiasing=args.aa,
        warmup_sweeps=args.warmup_sweeps,
        measurement_sweeps=args.measurement_sweeps,
        mode=args.mode,
        radius_clip=args.radius_clip,
    )
    quality = result.get("quality")
    if quality is not None:
        image_paths = []
        colors = quality.pop("colors", None)
        if write_frames and colors is not None:
            frames_dir = output_dir / "frames"
            frames_dir.mkdir(parents=True, exist_ok=True)
            for index, image in enumerate(colors):
                path = frames_dir / f"{index:06d}.png"
                Image.fromarray(image, mode="RGB").save(path)
                image_paths.append(str(path))
        quality["images"] = image_paths
    (output_dir / "render.json").write_text(json.dumps(result, indent=2))
    return result


def rendered_image_paths(render_summary: dict) -> list[Path]:
    quality = render_summary.get("quality")
    if quality is None:
        return []
    return [Path(path) for path in quality.get("images", [])]


def evaluate_psnr(render_summary: dict, image_paths: list[Path], resize_reference: bool) -> tuple[float, float]:
    import numpy as np
    from PIL import Image

    from common import calculate_psnr

    psnrs = []
    rendered_paths = rendered_image_paths(render_summary)
    if len(rendered_paths) != len(image_paths):
        raise ValueError("Rendered image count does not match reference image count")
    resample = getattr(getattr(Image, "Resampling", Image), "BILINEAR")

    for rendered_path, image_path in zip(rendered_paths, image_paths):
        rendered_image = Image.open(rendered_path).convert("RGB")
        reference_image = Image.open(image_path).convert("RGB")
        if rendered_image.size != reference_image.size and resize_reference:
            reference_image = reference_image.resize(rendered_image.size, resample)
        rendered = np.array(rendered_image)
        reference = np.array(reference_image)
        if rendered.shape != reference.shape:
            raise ValueError(f"Image size mismatch: {rendered_path} vs {image_path}")
        psnrs.append(calculate_psnr(reference, rendered))

    return float(np.mean(psnrs)), float(np.std(psnrs))


def evaluate_psnr_colors(colors, image_paths: list[Path], resize_reference: bool) -> tuple[float, float]:
    import numpy as np
    from PIL import Image

    from common import calculate_psnr

    if len(colors) != len(image_paths):
        raise ValueError("Rendered image count does not match reference image count")
    psnrs = []
    resample = getattr(getattr(Image, "Resampling", Image), "BILINEAR")

    for rendered, image_path in zip(colors, image_paths):
        with Image.open(image_path) as image_file:
            reference_image = image_file.convert("RGB")
        if (rendered.shape[1], rendered.shape[0]) != reference_image.size and resize_reference:
            reference_image = reference_image.resize((rendered.shape[1], rendered.shape[0]), resample)
        reference = np.array(reference_image)
        if rendered.shape != reference.shape:
            raise ValueError(f"Image size mismatch: rendered frame vs {image_path}")
        psnrs.append(calculate_psnr(reference, rendered))

    return float(np.mean(psnrs)), float(np.std(psnrs))


def evaluate_gsplat_quality_psnr(tensors, draw_data: dict, args, radius_clip: float) -> tuple[tuple[float, float], dict]:
    import numpy as np
    from PIL import Image
    import torch

    from common import calculate_psnr
    from draw_gsplat import rasterize_one

    width = int(draw_data["width"])
    height = int(draw_data["height"])
    camera_count = int(len(tensors["viewmats"]))
    rasterize_mode = "antialiased" if args.aa else "classic"
    degree = args.sh_degree if tensors["colors"].shape[1] > 1 else None
    psnrs = []
    resample = getattr(getattr(Image, "Resampling", Image), "BILINEAR")

    with torch.inference_mode():
        for i, image_path in enumerate(draw_data["image_paths"]):
            image, _, _ = rasterize_one(
                tensors["means"],
                tensors["quats"],
                tensors["scales"],
                tensors["opacities"],
                tensors["colors"],
                tensors["viewmats"],
                tensors["Ks"],
                i,
                width,
                height,
                degree,
                args.near,
                args.far,
                rasterize_mode,
                radius_clip,
            )
            rendered = (image[0, ..., :3].clamp(0.0, 1.0) * 255.0).to(torch.uint8).cpu().numpy()
            with Image.open(image_path) as image_file:
                reference_image = image_file.convert("RGB")
            if (rendered.shape[1], rendered.shape[0]) != reference_image.size and args.resize_reference:
                reference_image = reference_image.resize((rendered.shape[1], rendered.shape[0]), resample)
            reference = np.array(reference_image)
            if rendered.shape != reference.shape:
                raise ValueError(f"Image size mismatch: rendered frame vs {image_path}")
            psnrs.append(calculate_psnr(reference, rendered))
    torch.cuda.synchronize()

    return (
        float(np.mean(psnrs)),
        float(np.std(psnrs)),
    ), {
        "label": "quality/capture pass",
        "frames": camera_count,
        "radius_clip": float(radius_clip),
    }


def compact_quality(quality: dict | None) -> dict | None:
    if quality is None:
        return None
    return {
        "label": quality.get("label"),
        "frames": quality.get("frames"),
        "image_count": len(quality.get("images", [])),
    }


def build_summary(render_summary: dict, psnr: tuple[float, float] | None, args, target: str, scene_hash: str) -> dict:
    hot = render_summary.get("hot_render")
    width = int(render_summary["width"])
    height = int(render_summary["height"])
    return {
        "implementation": render_summary.get("implementation", target),
        "dataset": dataset_name(args),
        "scene_hash": scene_hash,
        "camera_count": int(render_summary.get("camera_count", render_summary["frames"])),
        "resolution": [width, height],
        "splats": int(render_summary["splats"]),
        "system": system_info(render_summary),
        "directxsplat_commit": directxsplat_commit(),
        "gsplat_commit_version": gsplat_version(),
        "timing_scope": render_summary.get("timing_scope", TIMING_SCOPE),
        "warmup_sweeps": int(args.warmup_sweeps),
        "measurement_sweeps": int(args.measurement_sweeps),
        "psnr": None if psnr is None else {"mean": psnr[0], "std": psnr[1]},
        "gpu_fps": None if hot is None else hot.get("gpu_fps"),
        "median_gpu_frame_ms": None if hot is None else hot.get("median_gpu_ms"),
        "p95_gpu_frame_ms": None if hot is None else hot.get("p95_gpu_ms"),
        "hot_render": hot,
        "quality": compact_quality(render_summary.get("quality")),
        "render": render_summary,
        "radius_clip": render_summary.get("radius_clip"),
    }


def write_summary(path: Path, summary: dict) -> None:
    path.write_text(json.dumps(summary, indent=2))


def table_value(value, digits: int = 2) -> str:
    if value is None:
        return "N/A"
    if isinstance(value, float):
        return f"{value:.{digits}f}"
    return str(value)


def psnr_value_text(psnr: dict, digits: int = 2) -> str:
    return f"{psnr['mean']:.{digits}f} +/- {psnr['std']:.{digits}f}"


def psnr_text(summary: dict) -> str:
    psnr = summary.get("psnr")
    if psnr is None:
        return "N/A"
    return f"{psnr['mean']:.2f} +/- {psnr['std']:.2f}"


def print_table(summaries: list[dict]) -> None:
    rows = []
    for summary in summaries:
        width, height = summary["resolution"]
        rows.append(
            [
                str(summary["implementation"]),
                str(summary["dataset"]),
                str(summary["camera_count"]),
                f"{width}x{height}",
                str(summary["splats"]),
                psnr_text(summary),
                table_value(summary.get("gpu_fps")),
                table_value(summary.get("median_gpu_frame_ms"), 4),
                table_value(summary.get("p95_gpu_frame_ms"), 4),
            ]
        )

    headers = [
        "Implementation",
        "Dataset",
        "#imgs",
        "resolution",
        "#splats",
        "PSNR",
        "GPU FPS",
        "median GPU ms",
        "p95 GPU ms",
    ]
    widths = [len(header) for header in headers]
    for row in rows:
        for index, value in enumerate(row):
            widths[index] = max(widths[index], len(value))

    print(" | ".join(header.ljust(widths[index]) for index, header in enumerate(headers)))
    print(" | ".join("-" * width for width in widths))
    for row in rows:
        print(" | ".join(value.ljust(widths[index]) for index, value in enumerate(row)))


def print_radius_sweep_table(summary: dict) -> None:
    rows = []
    target = summary.get("target_psnr")
    matched_radius = None
    if summary.get("matched") is not None:
        matched_radius = summary["matched"]["radius_clip"]
    for row in summary["results"]:
        psnr = row["psnr"]
        delta = None if target is None else psnr["mean"] - target
        rows.append(
            [
                f"{row['radius_clip']:.4f}",
                psnr_value_text(psnr, 3),
                table_value(delta, 3) if delta is not None else "N/A",
                table_value(row.get("gpu_fps")),
                table_value(row.get("median_gpu_frame_ms"), 4),
                table_value(row.get("p95_gpu_frame_ms"), 4),
                "*" if matched_radius == row["radius_clip"] else "",
            ]
        )

    headers = ["radius_clip", "PSNR", "delta", "GPU FPS", "median GPU ms", "p95 GPU ms", "matched"]
    widths = [len(header) for header in headers]
    for row in rows:
        for index, value in enumerate(row):
            widths[index] = max(widths[index], len(value))
    print(" | ".join(header.ljust(widths[index]) for index, header in enumerate(headers)))
    print(" | ".join("-" * width for width in widths))
    for row in rows:
        print(" | ".join(value.ljust(widths[index]) for index, value in enumerate(row)))


def psnr_delta(row: dict, target_psnr: float) -> float:
    return float(row["psnr"]["mean"] - target_psnr)


def nearest_psnr_row(results: list[dict], target_psnr: float) -> dict:
    return min(results, key=lambda row: (abs(psnr_delta(row, target_psnr)), -float(row.get("gpu_fps") or 0.0)))


def find_psnr_bracket(results: list[dict], target_psnr: float) -> tuple[dict, dict] | None:
    sorted_rows = sorted(results, key=lambda row: row["radius_clip"])
    candidates = []
    for left, right in zip(sorted_rows, sorted_rows[1:]):
        left_delta = psnr_delta(left, target_psnr)
        right_delta = psnr_delta(right, target_psnr)
        if left_delta == 0.0:
            return left, left
        if right_delta == 0.0:
            return right, right
        if left_delta * right_delta < 0.0:
            candidates.append((abs(right["radius_clip"] - left["radius_clip"]), left, right))
    if not candidates:
        return None
    _, left, right = min(candidates, key=lambda item: item[0])
    return left, right


def next_radius_from_bracket(left: dict, right: dict, target_psnr: float, seen: set[float]) -> float | None:
    left_radius = float(left["radius_clip"])
    right_radius = float(right["radius_clip"])
    if left_radius == right_radius:
        return None

    left_psnr = float(left["psnr"]["mean"])
    right_psnr = float(right["psnr"]["mean"])
    if left_psnr != right_psnr:
        fraction = (target_psnr - left_psnr) / (right_psnr - left_psnr)
        fraction = max(0.1, min(0.9, fraction))
        radius = left_radius + (right_radius - left_radius) * fraction
    else:
        radius = (left_radius + right_radius) * 0.5

    rounded = round(radius, 4)
    if rounded in seen:
        midpoint = round((left_radius + right_radius) * 0.5, 4)
        if midpoint in seen:
            return None
        return midpoint
    return rounded


def next_radius_without_bracket(results: list[dict], target_psnr: float, max_radius: float, seen: set[float]) -> float | None:
    sorted_rows = sorted(results, key=lambda row: row["radius_clip"])
    deltas = [psnr_delta(row, target_psnr) for row in sorted_rows]
    if all(delta > 0.0 for delta in deltas):
        highest = float(sorted_rows[-1]["radius_clip"])
        if highest >= max_radius:
            return None
        radius = min(max_radius, highest * 1.5 if highest > 0.0 else 0.25)
        while round(radius, 4) in seen and radius < max_radius:
            radius = min(max_radius, radius + max(0.25, radius * 0.25))
        return None if round(radius, 4) in seen else round(radius, 4)

    if all(delta < 0.0 for delta in deltas):
        lowest = float(sorted_rows[0]["radius_clip"])
        if lowest <= 0.0:
            return None
        radius = max(0.0, lowest * 0.5)
        return None if round(radius, 4) in seen else round(radius, 4)

    return None


def next_radius_candidate(results: list[dict], target_psnr: float, max_radius: float, seen: set[float]) -> float | None:
    bracket = find_psnr_bracket(results, target_psnr)
    if bracket is not None:
        return next_radius_from_bracket(bracket[0], bracket[1], target_psnr, seen)
    return next_radius_without_bracket(results, target_psnr, max_radius, seen)


def run_gsplat_radius_row(tensors, draw_data: dict, args, radius_clip: float) -> dict:
    from draw_gsplat import run_hot_render

    print(f"radius_clip: {radius_clip:.4f}")
    hot = run_hot_render(
        tensors,
        draw_data,
        args.near,
        args.far,
        args.sh_degree,
        args.aa,
        args.warmup_sweeps,
        args.measurement_sweeps,
        radius_clip,
    )
    psnr, quality = evaluate_gsplat_quality_psnr(tensors, draw_data, args, radius_clip)
    return {
        "implementation": "gsplat",
        "dataset": dataset_name(args),
        "radius_clip": float(radius_clip),
        "psnr": {"mean": psnr[0], "std": psnr[1]},
        "gpu_fps": hot.get("gpu_fps"),
        "median_gpu_frame_ms": hot.get("median_gpu_ms"),
        "p95_gpu_frame_ms": hot.get("p95_gpu_ms"),
        "hot_render": hot,
        "quality": {
            "label": quality.get("label"),
            "frames": quality.get("frames"),
            "radius_clip": quality.get("radius_clip"),
        },
    }


def run_gsplat_radius_sweep(args, draw_data: dict, output_dir: Path, scene_hash: str) -> dict:
    from draw_gsplat import load_resident_tensors

    if not draw_data["image_paths"]:
        raise ValueError("gsplat radius sweep requires reference images for PSNR")
    output_dir.mkdir(parents=True, exist_ok=True)

    radius_clips = parse_radius_clips(args.radius_clips)
    tensors = load_resident_tensors(args.ply_path, draw_data)
    results = []
    seen = set()

    for radius_clip in radius_clips:
        rounded = round(radius_clip, 4)
        seen.add(rounded)
        results.append(run_gsplat_radius_row(tensors, draw_data, args, rounded))

    matched = None
    if args.target_psnr is not None:
        matched = nearest_psnr_row(results, args.target_psnr)
        for _ in range(args.psnr_match_iterations):
            if abs(psnr_delta(matched, args.target_psnr)) <= args.psnr_match_tolerance:
                break
            if not args.psnr_match_refine:
                break
            candidate = next_radius_candidate(results, args.target_psnr, args.psnr_match_max_radius, seen)
            if candidate is None:
                break
            seen.add(candidate)
            results.append(run_gsplat_radius_row(tensors, draw_data, args, candidate))
            matched = nearest_psnr_row(results, args.target_psnr)

    summary = {
        "implementation": "gsplat",
        "benchmark": "radius_clip_quality_speed_sweep",
        "dataset": dataset_name(args),
        "scene_hash": scene_hash,
        "camera_count": int(len(tensors["viewmats"])),
        "resolution": [int(draw_data["width"]), int(draw_data["height"])],
        "splats": int(tensors["means"].shape[0]),
        "system": system_info({"adapter_name": None}),
        "directxsplat_commit": directxsplat_commit(),
        "gsplat_commit_version": gsplat_version(),
        "timing_scope": TIMING_SCOPE,
        "warmup_sweeps": int(args.warmup_sweeps),
        "measurement_sweeps": int(args.measurement_sweeps),
        "target_psnr": args.target_psnr,
        "psnr_match_tolerance": float(args.psnr_match_tolerance),
        "psnr_match_refine": bool(args.psnr_match_refine),
        "psnr_match_iterations": int(args.psnr_match_iterations),
        "psnr_match_max_radius": float(args.psnr_match_max_radius),
        "radius_clips": sorted(row["radius_clip"] for row in results),
        "matched": matched,
        "matched_within_tolerance": None if matched is None or args.target_psnr is None else (
            abs(psnr_delta(matched, args.target_psnr)) <= args.psnr_match_tolerance
        ),
        "results": sorted(results, key=lambda row: row["radius_clip"]),
    }
    write_summary(output_dir / "summary.json", summary)
    print_radius_sweep_table(summary)
    print(f"summary: {output_dir / 'summary.json'}")
    return summary


def run_target(args, target: str, camera_json: Path, draw_data: dict, output_dir: Path, scene_hash: str) -> dict:
    target_args = copy.copy(args)
    target_args.target = target
    output_dir.mkdir(parents=True, exist_ok=True)

    has_reference_images = bool(draw_data["image_paths"])
    write_frames = target_args.keep_frames or (wants_quality(target_args.mode) and has_reference_images and not target_args.no_psnr)

    if target == "directxsplat":
        render_summary = run_renderer(target_args, camera_json, output_dir, write_frames)
    elif target == "gsplat":
        render_summary = run_gsplat(target_args, draw_data, output_dir, write_frames)
    else:
        raise ValueError(f"Unsupported target {target}")

    psnr = None
    if has_reference_images and not target_args.no_psnr and wants_quality(target_args.mode):
        psnr = evaluate_psnr(render_summary, draw_data["image_paths"], target_args.resize_reference)

    summary = build_summary(render_summary, psnr, target_args, target, scene_hash)
    write_summary(output_dir / "summary.json", summary)
    return summary


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=["directxsplat", "gsplat", "gsplat-radius-sweep", "all"], default="directxsplat")
    parser.add_argument("--mode", choices=["hot-render", "quality", "both"], default="both")
    parser.add_argument("--dataset")
    parser.add_argument("--ply-path", "--ply_path", dest="ply_path", type=Path, required=True)
    parser.add_argument("--colmap-path", "--colmap_path", dest="colmap_path", type=Path)
    parser.add_argument("--camera-json", "--camera_json", dest="camera_json", type=Path)
    parser.add_argument("--image-dir", "--image_dir", dest="image_dir", type=Path)
    parser.add_argument("--scale", type=int, default=1, help="COLMAP image folder scale, such as 1, 2, 4, or 8")
    parser.add_argument("--width", type=int)
    parser.add_argument("--height", type=int)
    parser.add_argument("--raw-colmap-intrinsics", "--raw_colmap_intrinsics", dest="raw_colmap_intrinsics", action="store_true")
    parser.add_argument("--first", type=int)
    parser.add_argument("--runner", type=Path)
    parser.add_argument("--build-dir", "--build_dir", dest="build_dir", type=Path)
    parser.add_argument("--output-dir", "--output_dir", dest="output_dir", type=Path)
    parser.add_argument("--warmup-sweeps", "--warmup_sweeps", "--warmup", dest="warmup_sweeps", type=int, default=2)
    parser.add_argument("--measurement-sweeps", "--measurement_sweeps", dest="measurement_sweeps", type=int, default=5)
    parser.add_argument("--near", type=float, default=0.1)
    parser.add_argument("--far", type=float, default=1000.0)
    parser.add_argument("--render-type", "--render_type", dest="render_type", choices=["color", "alpha", "depth"], default="color")
    parser.add_argument("--sh-degree", "--sh_degree", dest="sh_degree", type=int, default=3, choices=[0, 1, 2, 3])
    parser.add_argument("--splat-budget", "--splat_budget", dest="splat_budget", type=int)
    parser.add_argument("--scale-modifier", "--scale_modifier", dest="scale_modifier", type=float, default=1.0)
    parser.add_argument("--chunk-size", "--chunk_size", dest="chunk_size", type=int, default=1)
    parser.add_argument("--radius-clip", "--radius_clip", dest="radius_clip", type=float, default=0.0)
    parser.add_argument("--radius-clips", "--radius_clips", dest="radius_clips",
                        default="0.0,0.25,0.5,1.0,1.5,2.0,3.0,4.0")
    parser.add_argument("--target-psnr", "--target_psnr", dest="target_psnr", type=float)
    parser.add_argument("--psnr-match-tolerance", "--psnr_match_tolerance", dest="psnr_match_tolerance",
                        type=float, default=0.05)
    parser.add_argument("--psnr-match-iterations", "--psnr_match_iterations", dest="psnr_match_iterations",
                        type=int, default=8)
    parser.add_argument("--psnr-match-max-radius", "--psnr_match_max_radius", dest="psnr_match_max_radius",
                        type=float, default=16.0)
    parser.add_argument("--no-psnr-match-refine", "--no_psnr_match_refine", dest="psnr_match_refine",
                        action="store_false")
    parser.add_argument("--no-aa", "--no_aa", dest="aa", action="store_false")
    parser.add_argument("--gamma", action="store_true")
    parser.add_argument("--no-fast-culling", "--no_fast_culling", dest="no_fast_culling", action="store_true")
    parser.add_argument("--no-psnr", "--no_psnr", dest="no_psnr", action="store_true")
    parser.add_argument("--resize-reference", "--resize_reference", dest="resize_reference", action="store_true")
    parser.add_argument("--keep-frames", "--keep_frames", dest="keep_frames", action="store_true")
    parser.set_defaults(aa=True, psnr_match_refine=True)
    args = parser.parse_args()

    args.warmup_sweeps = max(2, args.warmup_sweeps)
    args.measurement_sweeps = max(1, args.measurement_sweeps)
    args.psnr_match_tolerance = max(0.0, args.psnr_match_tolerance)
    args.psnr_match_iterations = max(0, args.psnr_match_iterations)
    args.psnr_match_max_radius = max(0.0, args.psnr_match_max_radius)

    output_dir = args.output_dir if args.output_dir is not None else default_output_dir(args.ply_path)
    output_dir.mkdir(parents=True, exist_ok=True)

    draw_data = load_draw_data(args)
    camera_json = output_dir / "cameras.json"
    from common import write_camera_json

    write_camera_json(camera_json, draw_data["cameras"])

    scene_hash = file_sha256(args.ply_path)
    if args.target == "gsplat-radius-sweep":
        run_gsplat_radius_sweep(args, draw_data, output_dir, scene_hash)
        return 0

    targets = ["directxsplat", "gsplat"] if args.target == "all" else [args.target]
    summaries = []
    for target in targets:
        target_output = output_dir / target if len(targets) > 1 else output_dir
        summaries.append(run_target(args, target, camera_json, draw_data, target_output, scene_hash))

    if len(summaries) > 1:
        write_summary(output_dir / "summary.json", {"timing_scope": TIMING_SCOPE, "results": summaries})

    print(f"timing scope: {TIMING_SCOPE}")
    print_table(summaries)
    print(f"summary: {output_dir / 'summary.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
