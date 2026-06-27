from __future__ import annotations

from dataclasses import dataclass
import json
import math
from pathlib import Path
import struct
from typing import Iterable

import numpy as np
from PIL import Image
from plyfile import PlyData


CAMERA_MODELS = {
    0: ("SIMPLE_PINHOLE", 3),
    1: ("PINHOLE", 4),
    2: ("SIMPLE_RADIAL", 4),
    3: ("RADIAL", 5),
    4: ("OPENCV", 8),
    5: ("OPENCV_FISHEYE", 8),
    6: ("FULL_OPENCV", 12),
    7: ("FOV", 5),
    8: ("SIMPLE_RADIAL_FISHEYE", 4),
    9: ("RADIAL_FISHEYE", 5),
    10: ("THIN_PRISM_FISHEYE", 12),
}

CAMERA_MODEL_IDS = {name: (model_id, count) for model_id, (name, count) in CAMERA_MODELS.items()}


@dataclass
class ColmapCamera:
    camera_id: int
    model: str
    width: int
    height: int
    params: list[float]


@dataclass
class ColmapImage:
    image_id: int
    qvec: np.ndarray
    tvec: np.ndarray
    camera_id: int
    name: str


def qvec_to_rotmat(qvec: np.ndarray) -> np.ndarray:
    w, x, y, z = qvec
    return np.array(
        [
            [1.0 - 2.0 * y * y - 2.0 * z * z, 2.0 * x * y - 2.0 * w * z, 2.0 * z * x + 2.0 * w * y],
            [2.0 * x * y + 2.0 * w * z, 1.0 - 2.0 * x * x - 2.0 * z * z, 2.0 * y * z - 2.0 * w * x],
            [2.0 * z * x - 2.0 * w * y, 2.0 * y * z + 2.0 * w * x, 1.0 - 2.0 * x * x - 2.0 * y * y],
        ],
        dtype=np.float32,
    )


def read_next_bytes(handle, count: int, fmt: str):
    data = handle.read(count)
    if len(data) != count:
        raise ValueError("Unexpected end of COLMAP file")
    return struct.unpack(fmt, data)


def read_cameras_binary(path: Path) -> dict[int, ColmapCamera]:
    cameras: dict[int, ColmapCamera] = {}
    with path.open("rb") as handle:
        (count,) = read_next_bytes(handle, 8, "<Q")
        for _ in range(count):
            camera_id, model_id, width, height = read_next_bytes(handle, 24, "<iiQQ")
            model, param_count = CAMERA_MODELS[model_id]
            params = list(read_next_bytes(handle, 8 * param_count, "<" + "d" * param_count))
            cameras[camera_id] = ColmapCamera(camera_id, model, width, height, params)
    return cameras


def read_images_binary(path: Path) -> list[ColmapImage]:
    images: list[ColmapImage] = []
    with path.open("rb") as handle:
        (count,) = read_next_bytes(handle, 8, "<Q")
        for _ in range(count):
            image_id = read_next_bytes(handle, 4, "<i")[0]
            qvec = np.array(read_next_bytes(handle, 32, "<dddd"), dtype=np.float32)
            tvec = np.array(read_next_bytes(handle, 24, "<ddd"), dtype=np.float32)
            camera_id = read_next_bytes(handle, 4, "<i")[0]
            name_bytes = bytearray()
            while True:
                ch = handle.read(1)
                if ch == b"\x00":
                    break
                if not ch:
                    raise ValueError("Unexpected end of COLMAP image name")
                name_bytes.extend(ch)
            (point_count,) = read_next_bytes(handle, 8, "<Q")
            handle.seek(point_count * 24, 1)
            images.append(ColmapImage(image_id, qvec, tvec, camera_id, name_bytes.decode("utf-8")))
    return images


def read_cameras_text(path: Path) -> dict[int, ColmapCamera]:
    cameras: dict[int, ColmapCamera] = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        camera_id = int(parts[0])
        model = parts[1]
        width = int(parts[2])
        height = int(parts[3])
        params = [float(v) for v in parts[4:]]
        cameras[camera_id] = ColmapCamera(camera_id, model, width, height, params)
    return cameras


def read_images_text(path: Path) -> list[ColmapImage]:
    images: list[ColmapImage] = []
    lines = [line.strip() for line in path.read_text().splitlines() if line.strip() and not line.startswith("#")]
    for i in range(0, len(lines), 2):
        parts = lines[i].split()
        image_id = int(parts[0])
        qvec = np.array([float(v) for v in parts[1:5]], dtype=np.float32)
        tvec = np.array([float(v) for v in parts[5:8]], dtype=np.float32)
        camera_id = int(parts[8])
        name = " ".join(parts[9:])
        images.append(ColmapImage(image_id, qvec, tvec, camera_id, name))
    return images


def resolve_sparse_dir(path: Path) -> Path:
    if (path / "cameras.bin").exists() or (path / "cameras.txt").exists():
        return path
    sparse = path / "sparse" / "0"
    if (sparse / "cameras.bin").exists() or (sparse / "cameras.txt").exists():
        return sparse
    raise FileNotFoundError(f"Could not find COLMAP sparse files under {path}")


def resolve_image_dir(path: Path, scale: int) -> Path:
    name = "images" if scale == 1 else f"images_{scale}"
    candidates = [path / name, path.parent / name, path.parent.parent / name]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"Could not find {name} next to {path}")


def load_colmap_sparse(path: Path) -> tuple[dict[int, ColmapCamera], list[ColmapImage]]:
    sparse = resolve_sparse_dir(path)
    if (sparse / "cameras.bin").exists() and (sparse / "images.bin").exists():
        return read_cameras_binary(sparse / "cameras.bin"), read_images_binary(sparse / "images.bin")
    return read_cameras_text(sparse / "cameras.txt"), read_images_text(sparse / "images.txt")


def camera_intrinsics(camera: ColmapCamera) -> tuple[float, float, float, float]:
    p = camera.params
    if camera.model in {"SIMPLE_PINHOLE", "SIMPLE_RADIAL", "RADIAL", "SIMPLE_RADIAL_FISHEYE", "RADIAL_FISHEYE"}:
        return p[0], p[0], p[1], p[2]
    if camera.model in {"PINHOLE", "OPENCV", "OPENCV_FISHEYE", "FULL_OPENCV", "FOV", "THIN_PRISM_FISHEYE"}:
        return p[0], p[1], p[2], p[3]
    raise ValueError(f"Unsupported camera model {camera.model}")


def make_camera_record(image: ColmapImage, camera: ColmapCamera, image_path: Path, center_principal_point: bool) -> dict:
    with Image.open(image_path) as image_file:
        width, height = image_file.size
    fx, fy, cx, cy = camera_intrinsics(camera)
    sx = width / camera.width
    sy = height / camera.height
    if center_principal_point:
        cx = width * 0.5
        cy = height * 0.5
    else:
        cx *= sx
        cy *= sy
    extrinsic = np.eye(4, dtype=np.float32)
    extrinsic[:3, :3] = qvec_to_rotmat(image.qvec)
    extrinsic[:3, 3] = image.tvec
    intrinsic = np.array(
        [
            [fx * sx, 0.0, cx],
            [0.0, fy * sy, cy],
            [0.0, 0.0, 1.0],
        ],
        dtype=np.float32,
    )
    return {
        "name": image.name,
        "width": width,
        "height": height,
        "extrinsic": extrinsic.tolist(),
        "intrinsic": intrinsic.tolist(),
    }


def load_colmap_draw_data(colmap_path: Path, scale: int, first: int | None, raw_intrinsics: bool = False):
    cameras, images = load_colmap_sparse(colmap_path)
    image_dir = resolve_image_dir(colmap_path, scale)
    images = sorted(images, key=lambda item: item.name)
    if first is not None:
        images = images[:first]

    camera_records = []
    image_paths = []
    for image in images:
        image_path = image_dir / image.name
        if not image_path.exists():
            raise FileNotFoundError(image_path)
        camera_records.append(make_camera_record(image, cameras[image.camera_id], image_path, not raw_intrinsics))
        image_paths.append(image_path)

    return {
        "cameras": camera_records,
        "viewmats": np.stack([np.asarray(camera["extrinsic"], dtype=np.float32) for camera in camera_records]),
        "Ks": np.stack([np.asarray(camera["intrinsic"], dtype=np.float32) for camera in camera_records]),
        "image_paths": image_paths,
        "width": camera_records[0]["width"],
        "height": camera_records[0]["height"],
    }


def scale_matrix_camera(camera: dict, scale: int) -> dict:
    if scale <= 1:
        return dict(camera)
    out = dict(camera)
    width = int(camera["width"])
    height = int(camera["height"])
    out["width"] = max(1, math.floor(width / scale + 0.5))
    out["height"] = max(1, math.floor(height / scale + 0.5))
    if "intrinsic" in out:
        intrinsic = np.asarray(out["intrinsic"], dtype=np.float32).copy()
        intrinsic[0, :] /= scale
        intrinsic[1, :] /= scale
        out["intrinsic"] = intrinsic.tolist()
    else:
        for name in ("fx", "fy", "cx", "cy"):
            if name in out:
                out[name] = float(out[name]) / scale
    return out


def normalize_camera_json_records(records: Iterable[dict], first: int | None, scale: int):
    cameras = list(records)
    if first is not None:
        cameras = cameras[:first]
    return [scale_matrix_camera(camera, scale) for camera in cameras]


def camera_record_matrices(camera: dict) -> tuple[np.ndarray, np.ndarray]:
    if "extrinsic" in camera and "intrinsic" in camera:
        return np.asarray(camera["extrinsic"], dtype=np.float32), np.asarray(camera["intrinsic"], dtype=np.float32)

    rotation = np.asarray(camera["rotation"], dtype=np.float32).reshape(3, 3)
    position = np.asarray(camera["position"], dtype=np.float32)
    extrinsic = np.eye(4, dtype=np.float32)
    extrinsic[0, :3] = [rotation[0, 0], rotation[1, 0], rotation[2, 0]]
    extrinsic[1, :3] = [rotation[0, 1], rotation[1, 1], rotation[2, 1]]
    extrinsic[2, :3] = [rotation[0, 2], rotation[1, 2], rotation[2, 2]]
    extrinsic[:3, 3] = -(extrinsic[:3, :3] @ position)
    width = int(camera["width"])
    height = int(camera["height"])
    cx = float(camera.get("cx", width * 0.5))
    cy = float(camera.get("cy", height * 0.5))
    intrinsic = np.array(
        [
            [float(camera["fx"]), 0.0, cx],
            [0.0, float(camera["fy"]), cy],
            [0.0, 0.0, 1.0],
        ],
        dtype=np.float32,
    )
    return extrinsic, intrinsic


def load_camera_json_draw_data(camera_json_path: Path, image_dir: Path | None, first: int | None, scale: int):
    root = json.loads(camera_json_path.read_text())
    if isinstance(root, dict):
        root = root.get("cameras", root.get("frames", []))
    cameras = normalize_camera_json_records(root, first, scale)
    image_paths = []
    if image_dir is not None:
        for camera in cameras:
            name = camera.get("img_name", camera.get("name", camera.get("file_path", "")))
            path = resolve_named_image(image_dir, name)
            image_paths.append(path)
    viewmats = []
    Ks = []
    for camera in cameras:
        viewmat, K = camera_record_matrices(camera)
        viewmats.append(viewmat)
        Ks.append(K)
    return {
        "cameras": cameras,
        "viewmats": np.stack(viewmats),
        "Ks": np.stack(Ks),
        "image_paths": image_paths,
        "width": cameras[0]["width"],
        "height": cameras[0]["height"],
    }


def resize_camera_record(camera: dict, width: int, height: int) -> dict:
    out = dict(camera)
    old_width = max(1, int(out["width"]))
    old_height = max(1, int(out["height"]))
    sx = width / old_width
    sy = height / old_height
    out["width"] = width
    out["height"] = height
    if "intrinsic" in out:
        intrinsic = np.asarray(out["intrinsic"], dtype=np.float32).copy()
        intrinsic[0, :] *= sx
        intrinsic[1, :] *= sy
        out["intrinsic"] = intrinsic.tolist()
    else:
        for name in ("fx", "cx"):
            if name in out:
                out[name] = float(out[name]) * sx
        for name in ("fy", "cy"):
            if name in out:
                out[name] = float(out[name]) * sy
    return out


def resize_draw_data(draw_data: dict, width: int | None, height: int | None) -> dict:
    if width is None and height is None:
        return draw_data
    if width is None or height is None:
        raise ValueError("--width and --height must be provided together")
    if width <= 0 or height <= 0:
        raise ValueError("benchmark resolution must be positive")

    cameras = [resize_camera_record(camera, width, height) for camera in draw_data["cameras"]]
    viewmats = []
    Ks = []
    for camera in cameras:
        viewmat, K = camera_record_matrices(camera)
        viewmats.append(viewmat)
        Ks.append(K)
    out = dict(draw_data)
    out["cameras"] = cameras
    out["viewmats"] = np.stack(viewmats)
    out["Ks"] = np.stack(Ks)
    out["width"] = width
    out["height"] = height
    return out


def resolve_named_image(image_dir: Path, name: str) -> Path:
    raw = Path(name)
    candidates = [image_dir / raw]
    if raw.suffix == "":
        candidates.extend(image_dir / f"{name}{suffix}" for suffix in (".png", ".jpg", ".jpeg"))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"Could not resolve image {name} in {image_dir}")


def write_camera_json(path: Path, cameras: list[dict]) -> None:
    path.write_text(json.dumps(cameras, indent=2))


def sigmoid(x: np.ndarray) -> np.ndarray:
    return 1.0 / (1.0 + np.exp(-x))


def load_gaussian_ply(path: Path) -> dict[str, np.ndarray]:
    ply = PlyData.read(path)
    vertex = ply["vertex"].data

    def stack(fields: Iterable[str]) -> np.ndarray:
        return np.stack([vertex[field] for field in fields], axis=1)

    means = stack(["x", "y", "z"]).astype(np.float32)
    quats = stack(["rot_0", "rot_1", "rot_2", "rot_3"]).astype(np.float32)
    scales = np.exp(stack(["scale_0", "scale_1", "scale_2"]).astype(np.float32))
    opacities = sigmoid(np.asarray(vertex["opacity"], dtype=np.float32))
    dc = stack(["f_dc_0", "f_dc_1", "f_dc_2"])
    rest_fields = [f"f_rest_{i}" for i in range(45)]
    if all(field in vertex.dtype.names for field in rest_fields):
        rest = stack(rest_fields)
        colors = np.concatenate((dc[:, None, :], rest.reshape(-1, 3, 15).swapaxes(-1, -2)), axis=-2)
    else:
        colors = dc[:, None, :]
    return {
        "means": means.astype(np.float32),
        "quats": quats.astype(np.float32),
        "scales": scales.astype(np.float32),
        "opacities": opacities.astype(np.float32),
        "colors": colors.astype(np.float32),
    }


def calculate_psnr(img_a: np.ndarray, img_b: np.ndarray) -> float:
    mse = np.mean((img_a.astype(np.float32) - img_b.astype(np.float32)) ** 2)
    if mse == 0.0:
        return 100.0
    return 20.0 * math.log10(255.0 / math.sqrt(mse))
