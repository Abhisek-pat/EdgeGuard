from __future__ import annotations

import argparse
import io
import time
from datetime import datetime
from pathlib import Path

import requests
from PIL import Image


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Capture labeled images from EdgeGuard over HTTP.")
    parser.add_argument("--base-url", required=True, help="Base URL of the ESP32 dashboard, e.g. http://192.168.1.207")
    parser.add_argument("--label", required=True, choices=["person", "empty"], help="Dataset label")
    parser.add_argument("--count", type=int, default=100, help="Number of images to capture")
    parser.add_argument("--delay", type=float, default=1.0, help="Delay between captures in seconds")
    parser.add_argument("--out-dir", default="ml/dataset", help="Dataset root directory")
    parser.add_argument("--resize", type=int, default=96, help="Resize output to NxN")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout in seconds")
    return parser.parse_args()


def ensure_dirs(root: Path, label: str) -> tuple[Path, Path]:
    raw_dir = root / "raw" / label
    processed_dir = root / "processed" / label
    raw_dir.mkdir(parents=True, exist_ok=True)
    processed_dir.mkdir(parents=True, exist_ok=True)
    return raw_dir, processed_dir


def capture_one(session: requests.Session, url: str, timeout: float) -> bytes:
    response = session.get(url, timeout=timeout)
    response.raise_for_status()
    return response.content


def save_pair(raw_bytes: bytes, raw_dir: Path, processed_dir: Path, resize: int) -> tuple[Path, Path]:
    ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    raw_path = raw_dir / f"{ts}.jpg"
    processed_path = processed_dir / f"{ts}.jpg"

    raw_path.write_bytes(raw_bytes)

    image = Image.open(io.BytesIO(raw_bytes)).convert("RGB")
    image = image.resize((resize, resize))
    image.save(processed_path, format="JPEG", quality=95)

    return raw_path, processed_path


def main() -> None:
    args = parse_args()
    root = Path(args.out_dir)
    raw_dir, processed_dir = ensure_dirs(root, args.label)

    capture_url = args.base_url.rstrip("/") + "/capture.jpg"
    session = requests.Session()

    print(f"Capturing {args.count} images for label={args.label}")
    print(f"Source: {capture_url}")
    print(f"Raw dir: {raw_dir}")
    print(f"Processed dir: {processed_dir}")

    saved = 0
    for i in range(args.count):
        try:
            raw_bytes = capture_one(session, capture_url, args.timeout)
            raw_path, processed_path = save_pair(raw_bytes, raw_dir, processed_dir, args.resize)
            saved += 1
            print(f"[{i+1}/{args.count}] saved raw={raw_path.name} processed={processed_path.name}")
        except Exception as exc:
            print(f"[{i+1}/{args.count}] failed: {exc}")

        time.sleep(args.delay)

    print(f"Done. Saved {saved}/{args.count} images.")


if __name__ == "__main__":
    main()