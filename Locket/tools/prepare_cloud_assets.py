#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct

from PIL import Image


ROOT = Path("/Users/matthewmorrone/Documents/Arduino/Waveshare/Locket")
INPUT_DIR = ROOT / "output" / "imagegen" / "cloud_batch"
HEADER_PATH = ROOT / "include" / "cloud_sprite_assets_generated.h"
SD_OUTPUT_DIR = ROOT / "output" / "sdcard" / "locket" / "clouds"


@dataclass(frozen=True)
class SpriteSpec:
    input_name: str
    output_name: str
    symbol_name: str
    max_width: int
    max_height: int


SPRITES = (
    SpriteSpec(
        "001-a-single-fluffy-semi-transparent-realistic-cloud-sprite-on-a.png",
        "01.cld",
        "kCloudSprite01Data",
        182,
        98,
    ),
    SpriteSpec(
        "002-a-single-wide-drifting-fluffy-semi-transparent-realistic-clo.png",
        "02.cld",
        "kCloudSprite02Data",
        238,
        88,
    ),
    SpriteSpec(
        "003-a-single-tall-billowy-fluffy-semi-transparent-realistic-clou.png",
        "03.cld",
        "kCloudSprite03Data",
        138,
        148,
    ),
)


def cloud_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    alpha = image.getchannel("A")
    thresholded = alpha.point(lambda value: 255 if value > 6 else 0)
    bbox = thresholded.getbbox()
    if bbox is None:
        raise ValueError("image has no opaque pixels")

    left, top, right, bottom = bbox
    padding = 12
    left = max(0, left - padding)
    top = max(0, top - padding)
    right = min(image.width, right + padding)
    bottom = min(image.height, bottom + padding)
    return left, top, right, bottom


def resize_cloud(image: Image.Image, max_width: int, max_height: int) -> Image.Image:
    scale = min(max_width / image.width, max_height / image.height)
    width = max(1, int(round(image.width * scale)))
    height = max(1, int(round(image.height * scale)))
    return image.resize((width, height), Image.Resampling.LANCZOS)


def pack_pixels(image: Image.Image) -> bytes:
    pixels = bytearray()
    for red, green, blue, alpha in image.getdata():
      if alpha < 4:
        pixels.append(0)
        continue

      luminance = int(round((red * 0.2126) + (green * 0.7152) + (blue * 0.0722)))
      alpha4 = max(0, min(15, int(round((alpha / 255.0) * 15.0))))
      light4 = max(0, min(15, int(round((luminance / 255.0) * 15.0))))
      pixels.append((alpha4 << 4) | light4)
    return bytes(pixels)


def format_array(data: bytes) -> str:
    lines: list[str] = []
    row: list[str] = []
    for index, value in enumerate(data, start=1):
        row.append(f"0x{value:02X}")
        if index % 16 == 0:
            lines.append("  " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("  " + ", ".join(row) + ",")
    return "\n".join(lines)


def write_header(sprite_rows: list[tuple[SpriteSpec, int, int, bytes]]) -> None:
    lines = [
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "struct EmbeddedCloudSpriteAsset",
        "{",
        "  uint16_t width;",
        "  uint16_t height;",
        "  const uint8_t *pixels;",
        "};",
        "",
    ]

    for spec, width, height, data in sprite_rows:
        lines.append(f"static const uint8_t {spec.symbol_name}[] = {{")
        lines.append(format_array(data))
        lines.append("};")
        lines.append("")

    lines.append(f"constexpr size_t kEmbeddedCloudSpriteCount = {len(sprite_rows)};")
    lines.append("static const EmbeddedCloudSpriteAsset kEmbeddedCloudSprites[kEmbeddedCloudSpriteCount] = {")
    for spec, width, height, _ in sprite_rows:
        lines.append(f"  {{{width}, {height}, {spec.symbol_name}}},")
    lines.append("};")
    lines.append("")

    HEADER_PATH.parent.mkdir(parents=True, exist_ok=True)
    HEADER_PATH.write_text("\n".join(lines), encoding="utf-8")


def write_sd_files(sprite_rows: list[tuple[SpriteSpec, int, int, bytes]]) -> None:
    SD_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for spec, width, height, data in sprite_rows:
        payload = struct.pack("<4sHH", b"CLD1", width, height) + data
        (SD_OUTPUT_DIR / spec.output_name).write_bytes(payload)


def main() -> None:
    sprite_rows: list[tuple[SpriteSpec, int, int, bytes]] = []
    for spec in SPRITES:
        source_path = INPUT_DIR / spec.input_name
        with Image.open(source_path).convert("RGBA") as source_image:
            cropped = source_image.crop(cloud_bbox(source_image))
            resized = resize_cloud(cropped, spec.max_width, spec.max_height)
            data = pack_pixels(resized)
            sprite_rows.append((spec, resized.width, resized.height, data))

    write_header(sprite_rows)
    write_sd_files(sprite_rows)

    for spec, width, height, _ in sprite_rows:
        print(f"{spec.output_name}: {width}x{height}")


if __name__ == "__main__":
    main()
