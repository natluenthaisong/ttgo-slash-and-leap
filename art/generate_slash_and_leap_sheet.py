#!/usr/bin/env python3
"""Build the strict 8x5 Slash & Leap pixel-art atlas without antialiasing."""

from __future__ import annotations

import binascii
import math
import struct
import zlib
from pathlib import Path


W, H, SCALE = 256, 160, 4
MAGENTA = "FF00FF"
OUTLINE = "28241E"
NAVY = "2E3458"
DARK_NAVY = "1E223C"
SKIN = "F4C8A0"
RED = "D83830"
WHITE = "FFFFFF"
GREEN = "6EBA46"
DARK_GREEN = "468228"
DEMON = "C83C32"
DARK_DEMON = "8C221E"
STEEL = "B0B4BE"
DARK_STEEL = "6E727E"
ROCK = "967654"
DARK_ROCK = "684E34"
GOLD = "FFD23C"
DARK_GOLD = "BE8C14"
HEART = "EB3C46"

PALETTE = {
    c: bytes.fromhex(c)
    for c in (
        MAGENTA, OUTLINE, NAVY, DARK_NAVY, SKIN, RED, WHITE, GREEN,
        DARK_GREEN, DEMON, DARK_DEMON, STEEL, DARK_STEEL, ROCK,
        DARK_ROCK, GOLD, DARK_GOLD, HEART,
    )
}

pixels = [[MAGENTA for _ in range(W)] for _ in range(H)]


def put(x: int, y: int, color: str) -> None:
    if 0 <= x < W and 0 <= y < H:
        pixels[y][x] = color


def mask_rect(x0: int, y0: int, x1: int, y1: int) -> set[tuple[int, int]]:
    return {(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)}


def mask_ellipse(cx: float, cy: float, rx: float, ry: float) -> set[tuple[int, int]]:
    pts: set[tuple[int, int]] = set()
    for y in range(math.floor(cy - ry), math.ceil(cy + ry) + 1):
        for x in range(math.floor(cx - rx), math.ceil(cx + rx) + 1):
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                pts.add((x, y))
    return pts


def mask_poly(points: list[tuple[int, int]]) -> set[tuple[int, int]]:
    min_y = min(y for _, y in points)
    max_y = max(y for _, y in points)
    pts: set[tuple[int, int]] = set()
    count = len(points)
    for y in range(min_y, max_y + 1):
        scan_y = y + 0.5
        xs: list[float] = []
        for i, (x1, y1) in enumerate(points):
            x2, y2 = points[(i + 1) % count]
            if (y1 <= scan_y < y2) or (y2 <= scan_y < y1):
                xs.append(x1 + (scan_y - y1) * (x2 - x1) / (y2 - y1))
        xs.sort()
        for i in range(0, len(xs) - 1, 2):
            for x in range(math.ceil(xs[i]), math.floor(xs[i + 1] - 1e-9) + 1):
                pts.add((x, y))
    return pts


def mask_line(x0: int, y0: int, x1: int, y1: int, width: int = 1) -> set[tuple[int, int]]:
    pts: set[tuple[int, int]] = set()
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    radius = (width - 1) // 2
    while True:
        for yy in range(y0 - radius, y0 + radius + 1):
            for xx in range(x0 - radius, x0 + radius + 1):
                pts.add((xx, yy))
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy
    return pts


def union(*parts: set[tuple[int, int]]) -> set[tuple[int, int]]:
    out: set[tuple[int, int]] = set()
    for part in parts:
        out.update(part)
    return out


def draw(mask: set[tuple[int, int]], fill: str, outline: str | None = None) -> None:
    if outline:
        border: set[tuple[int, int]] = set()
        for x, y in mask:
            for yy in range(y - 1, y + 2):
                for xx in range(x - 1, x + 2):
                    if (xx, yy) not in mask:
                        border.add((xx, yy))
        for x, y in border:
            put(x, y, outline)
    for x, y in mask:
        put(x, y, fill)


def detail(mask: set[tuple[int, int]], fill: str) -> None:
    for x, y in mask:
        put(x, y, fill)


def mirror_x(mask: set[tuple[int, int]], cx: int) -> set[tuple[int, int]]:
    return {(2 * cx - x, y) for x, y in mask}


def ninja_frame(col: int, pose: str) -> None:
    x0 = col * 32
    cx = x0 + 16

    if pose == "defeated":
        # Seated silhouette, head bowed; all pixels remain inside the cell.
        draw(mask_ellipse(cx + 1, 20, 7, 5), NAVY, OUTLINE)
        draw(mask_ellipse(cx - 2, 13, 5, 5), DARK_NAVY, OUTLINE)
        draw(mask_poly([(cx - 8, 24), (cx - 2, 22), (cx + 2, 26), (cx + 8, 27), (cx + 9, 30), (cx - 9, 30)]), NAVY, OUTLINE)
        draw(mask_line(cx - 6, 11, cx - 10, 9, 2), RED, OUTLINE)
        draw(mask_line(cx - 6, 13, cx - 10, 14, 2), RED, OUTLINE)
        detail(mask_rect(cx - 6, 13, cx + 1, 14), RED)
        draw(mask_line(cx + 1, 24, cx + 9, 15, 2), DARK_STEEL, OUTLINE)
        detail(mask_rect(cx + 6, 17, cx + 8, 18), GOLD)
        return

    if pose == "leap":
        draw(mask_poly([(cx - 5, 15), (cx - 8, 17), (cx - 6, 24), (cx, 26), (cx + 6, 24), (cx + 8, 17), (cx + 5, 15)]), NAVY, OUTLINE)
        draw(mask_line(cx - 6, 18, cx - 11, 15, 3), NAVY, OUTLINE)
        draw(mask_line(cx + 6, 18, cx + 11, 15, 3), NAVY, OUTLINE)
        draw(mask_rect(cx - 12, 14, cx - 10, 16), SKIN, OUTLINE)
        draw(mask_rect(cx + 10, 14, cx + 12, 16), SKIN, OUTLINE)
        draw(mask_line(cx - 3, 24, cx - 7, 28, 3), DARK_NAVY, OUTLINE)
        draw(mask_line(cx + 3, 24, cx + 7, 28, 3), DARK_NAVY, OUTLINE)
        draw(mask_ellipse(cx, 10, 5, 5), DARK_NAVY, OUTLINE)
    elif pose == "windup":
        draw(mask_poly([(cx - 5, 15), (cx - 8, 19), (cx - 6, 28), (cx + 6, 28), (cx + 8, 19), (cx + 5, 15)]), NAVY, OUTLINE)
        draw(mask_line(cx - 4, 18, cx - 7, 26, 3), NAVY, OUTLINE)
        draw(mask_line(cx + 4, 18, cx + 8, 11, 3), NAVY, OUTLINE)
        draw(mask_rect(cx + 7, 8, cx + 9, 11), SKIN, OUTLINE)
        draw(mask_line(cx + 8, 9, cx + 5, 2, 2), STEEL, OUTLINE)
        detail(mask_rect(cx + 5, 7, cx + 10, 8), GOLD)
        draw(mask_line(cx - 3, 28, cx - 4, 29, 4), DARK_NAVY, OUTLINE)
        draw(mask_line(cx + 3, 28, cx + 5, 29, 4), DARK_NAVY, OUTLINE)
        draw(mask_ellipse(cx, 10, 5, 5), DARK_NAVY, OUTLINE)
    elif pose == "slash":
        draw(mask_poly([(cx - 5, 15), (cx - 8, 20), (cx - 6, 28), (cx + 6, 28), (cx + 8, 20), (cx + 5, 15)]), NAVY, OUTLINE)
        draw(mask_line(cx - 4, 19, cx - 8, 23, 3), NAVY, OUTLINE)
        draw(mask_line(cx + 4, 18, cx + 8, 14, 3), NAVY, OUTLINE)
        draw(mask_rect(cx + 7, 12, cx + 9, 15), SKIN, OUTLINE)
        draw(mask_line(cx - 4, 28, cx - 5, 29, 4), DARK_NAVY, OUTLINE)
        draw(mask_line(cx + 4, 28, cx + 5, 29, 4), DARK_NAVY, OUTLINE)
        draw(mask_ellipse(cx, 10, 5, 5), DARK_NAVY, OUTLINE)
        # Chunky overhead crescent arc, fully confined to c5.
        arc = union(
            mask_line(cx + 8, 13, cx + 10, 6, 2),
            mask_line(cx + 10, 6, cx + 6, 2, 2),
            mask_line(cx + 6, 2, cx - 2, 1, 2),
        )
        draw(arc, WHITE, OUTLINE)
        detail(mask_rect(cx + 5, 11, cx + 10, 12), GOLD)
    elif pose == "hurt":
        draw(mask_poly([(cx - 5, 15), (cx - 8, 19), (cx - 5, 28), (cx + 7, 28), (cx + 8, 20), (cx + 4, 15)]), NAVY, OUTLINE)
        draw(mask_line(cx - 6, 19, cx - 10, 23, 3), NAVY, OUTLINE)
        draw(mask_line(cx + 5, 19, cx + 9, 16, 3), NAVY, OUTLINE)
        draw(mask_line(cx - 3, 28, cx - 5, 29, 4), DARK_NAVY, OUTLINE)
        draw(mask_line(cx + 4, 28, cx + 6, 29, 4), DARK_NAVY, OUTLINE)
        draw(mask_ellipse(cx - 1, 10, 5, 5), DARK_NAVY, OUTLINE)
        for sx, sy in ((cx - 9, 7), (cx + 8, 5), (cx + 10, 12)):
            star = {(sx, sy - 2), (sx, sy - 1), (sx - 2, sy), (sx - 1, sy), (sx, sy), (sx + 1, sy), (sx + 2, sy), (sx, sy + 1), (sx, sy + 2)}
            draw(star, GOLD, OUTLINE)
    else:
        # Run A/B share torso and head; legs swap cleanly around the same registration.
        flip = pose == "run_b"
        draw(mask_poly([(cx - 5, 15), (cx - 8, 19), (cx - 6, 26), (cx, 28), (cx + 6, 26), (cx + 8, 19), (cx + 5, 15)]), NAVY, OUTLINE)
        draw(mask_line(cx - 6, 18, cx - 8, 24, 3), NAVY, OUTLINE)
        draw(mask_line(cx + 6, 18, cx + 8, 23, 3), NAVY, OUTLINE)
        left_leg = mask_line(cx - 3, 25, cx - (2 if flip else 5), 29, 4)
        right_leg = mask_line(cx + 3, 25, cx + (5 if flip else 2), 29, 4)
        draw(left_leg, DARK_NAVY, OUTLINE)
        draw(right_leg, NAVY, OUTLINE)
        draw(mask_ellipse(cx, 10, 5, 5), DARK_NAVY, OUTLINE)

    # Shared rear-view headband, scarf tails, and diagonal sheathed katana.
    detail(mask_rect(cx - 5, 11, cx + 5, 13), RED)
    draw(mask_line(cx - 5, 11, cx - 10, 9, 2), RED, OUTLINE)
    draw(mask_line(cx - 5, 13, cx - 10, 15, 2), RED, OUTLINE)
    detail(mask_rect(cx - 4, 15, cx + 4, 16), RED)
    if pose not in {"windup", "slash"}:
        draw(mask_line(cx - 5, 25, cx + 7, 13, 2), DARK_STEEL, OUTLINE)
        detail(mask_rect(cx + 4, 14, cx + 8, 15), GOLD)


def demon_frame(col: int, hop: bool) -> None:
    cx = col * 32 + 16
    body_cy = 54 if not hop else 52
    draw(mask_ellipse(cx, body_cy, 7, 6), DEMON, OUTLINE)
    detail(mask_ellipse(cx, body_cy + 3, 6, 2), DARK_DEMON)
    # Horns.
    draw(mask_poly([(cx - 5, body_cy - 4), (cx - 6, body_cy - 8), (cx - 3, body_cy - 6)]), WHITE, OUTLINE)
    draw(mask_poly([(cx + 5, body_cy - 4), (cx + 6, body_cy - 8), (cx + 3, body_cy - 6)]), WHITE, OUTLINE)
    # Angry eyes and brows.
    detail(mask_line(cx - 5, body_cy - 2, cx - 2, body_cy, 1), OUTLINE)
    detail(mask_line(cx + 5, body_cy - 2, cx + 2, body_cy, 1), OUTLINE)
    put(cx - 3, body_cy, WHITE)
    put(cx + 3, body_cy, WHITE)
    if hop:
        draw(mask_line(cx - 6, body_cy + 1, cx - 9, body_cy - 2, 3), DEMON, OUTLINE)
        draw(mask_line(cx + 6, body_cy + 1, cx + 9, body_cy - 2, 3), DEMON, OUTLINE)
        draw(mask_line(cx - 3, body_cy + 6, cx - 6, 61, 3), DARK_DEMON, OUTLINE)
        draw(mask_line(cx + 3, body_cy + 6, cx + 6, 61, 3), DARK_DEMON, OUTLINE)
    else:
        draw(mask_line(cx - 6, body_cy + 1, cx - 9, body_cy + 4, 3), DEMON, OUTLINE)
        draw(mask_line(cx + 6, body_cy + 1, cx + 9, body_cy + 4, 3), DEMON, OUTLINE)
        draw(mask_line(cx - 3, body_cy + 6, cx - 4, 61, 3), DARK_DEMON, OUTLINE)
        draw(mask_line(cx + 3, body_cy + 6, cx + 4, 61, 3), DARK_DEMON, OUTLINE)
    detail(mask_rect(cx - 4, body_cy + 3, cx + 4, body_cy + 4), DARK_DEMON)


def rock_frame(col: int, frame: int) -> None:
    cx, cy = col * 32 + 16, 52
    draw(mask_ellipse(cx, cy, 10, 10), ROCK, OUTLINE)
    detail(mask_ellipse(cx - 3, cy - 4, 4, 3), "967654")
    detail(mask_line(cx - 7, cy + 5, cx - 4, cy + 7, 2), DARK_ROCK)
    cracks = [
        [(cx - 6, cy - 6, cx - 1, cy - 1), (cx - 1, cy - 1, cx - 4, cy + 4), (cx - 1, cy - 1, cx + 5, cy + 2)],
        [(cx + 6, cy - 6, cx + 1, cy - 1), (cx + 1, cy - 1, cx + 4, cy + 5), (cx + 1, cy - 1, cx - 5, cy + 2)],
        [(cx - 6, cy + 5, cx, cy), (cx, cy, cx + 5, cy - 5), (cx, cy, cx + 5, cy + 5)],
    ][frame]
    for x0, y0, x1, y1 in cracks:
        detail(mask_line(x0, y0, x1, y1, 1), DARK_ROCK)


def bamboo() -> None:
    # c1-c3, 88 px wide, centered in its 96x32 band.
    draw(mask_rect(4, 77, 91, 84), GREEN, OUTLINE)
    detail(mask_rect(6, 78, 89, 79), "6EBA46")
    detail(mask_rect(5, 82, 90, 84), DARK_GREEN)
    for x in (18, 36, 54, 72):
        draw(mask_rect(x, 76, x + 2, 85), DARK_GREEN, OUTLINE)
        detail(mask_rect(x + 1, 77, x + 1, 84), GREEN)
    # Cut end and leaf sprig.
    draw(mask_ellipse(6, 80.5, 3, 4), DARK_GREEN, OUTLINE)
    detail(mask_ellipse(6, 80.5, 1, 2), GOLD)
    draw(mask_line(10, 76, 8, 70, 1), DARK_GREEN, OUTLINE)
    draw(mask_poly([(8, 72), (3, 69), (4, 75), (8, 76)]), GREEN, OUTLINE)
    draw(mask_poly([(9, 71), (12, 66), (14, 72), (10, 75)]), GREEN, OUTLINE)


def spikes() -> None:
    # c4-c6, 88 px wide, exactly eight spikes.
    draw(mask_rect(100, 88, 187, 94), DARK_STEEL, OUTLINE)
    detail(mask_rect(101, 89, 186, 90), STEEL)
    for i in range(8):
        left = 101 + i * 11
        tri = mask_poly([(left, 87), (left + 5, 68), (left + 10, 87)])
        draw(tri, STEEL, OUTLINE)
        detail(mask_poly([(left + 5, 70), (left + 5, 85), (left + 8, 85)]), WHITE)
        detail(mask_line(left + 1, 86, left + 4, 73, 1), DARK_STEEL)
    for x in (106, 128, 150, 172):
        detail(mask_rect(x, 91, x + 2, 93), OUTLINE)


def tree(col: int, tall: bool) -> None:
    cx = col * 32 + 16
    base = 126
    trunk_top = 111 if not tall else 106
    draw(mask_poly([(cx - 3, base), (cx - 2, trunk_top), (cx + 3, trunk_top), (cx + 5, base)]), DARK_ROCK, OUTLINE)
    detail(mask_rect(cx, trunk_top + 3, cx + 2, base - 1), ROCK)
    roots = union(mask_line(cx, base - 2, cx - 7, base, 2), mask_line(cx + 1, base - 2, cx + 7, base, 2))
    draw(roots, DARK_ROCK, OUTLINE)
    if tall:
        draw(mask_ellipse(cx, 103, 8, 7), DARK_GREEN, OUTLINE)
        draw(mask_ellipse(cx - 4, 110, 7, 6), DARK_GREEN, OUTLINE)
        draw(mask_ellipse(cx + 5, 111, 8, 6), DARK_GREEN, OUTLINE)
        detail(mask_ellipse(cx - 2, 101, 4, 3), GREEN)
        detail(mask_ellipse(cx + 4, 108, 4, 3), GREEN)
    else:
        draw(mask_ellipse(cx, 107, 10, 7), DARK_GREEN, OUTLINE)
        draw(mask_ellipse(cx - 6, 112, 7, 5), DARK_GREEN, OUTLINE)
        draw(mask_ellipse(cx + 6, 112, 7, 5), DARK_GREEN, OUTLINE)
        detail(mask_ellipse(cx - 3, 105, 5, 3), GREEN)
        detail(mask_ellipse(cx + 5, 109, 3, 2), GREEN)


def torii() -> None:
    cx = 80
    draw(mask_rect(cx - 12, 101, cx + 12, 104), RED, OUTLINE)
    draw(mask_rect(cx - 9, 106, cx + 9, 108), RED, OUTLINE)
    draw(mask_rect(cx - 8, 104, cx - 5, 126), RED, OUTLINE)
    draw(mask_rect(cx + 5, 104, cx + 8, 126), RED, OUTLINE)
    detail(mask_rect(cx - 7, 106, cx - 6, 125), DEMON)
    detail(mask_rect(cx + 6, 106, cx + 7, 125), DEMON)
    detail(mask_rect(cx - 11, 102, cx + 11, 102), DARK_DEMON)
    draw(mask_rect(cx - 10, 125, cx - 3, 126), DARK_ROCK, OUTLINE)
    draw(mask_rect(cx + 3, 125, cx + 10, 126), DARK_ROCK, OUTLINE)


def lantern() -> None:
    cx = 112
    draw(mask_rect(cx - 3, 120, cx + 3, 126), ROCK, OUTLINE)
    draw(mask_rect(cx - 5, 117, cx + 5, 121), DARK_ROCK, OUTLINE)
    draw(mask_rect(cx - 5, 108, cx + 5, 116), ROCK, OUTLINE)
    detail(mask_rect(cx - 3, 110, cx - 1, 114), GOLD)
    detail(mask_rect(cx + 1, 110, cx + 3, 114), GOLD)
    detail(mask_rect(cx, 110, cx, 114), OUTLINE)
    draw(mask_poly([(cx - 7, 108), (cx - 4, 104), (cx + 4, 104), (cx + 7, 108)]), DARK_STEEL, OUTLINE)
    draw(mask_rect(cx - 2, 102, cx + 2, 104), STEEL, OUTLINE)
    draw(mask_rect(cx - 1, 100, cx + 1, 102), DARK_STEEL, OUTLINE)


def coin(col: int, frame: int) -> None:
    cx, cy = col * 32 + 16, 144
    radii = [(5, 5), (4, 5), (1, 5), (4, 5)][frame]
    rx, ry = radii
    draw(mask_ellipse(cx, cy, rx, ry), GOLD, OUTLINE)
    if frame != 2:
        detail(mask_line(cx - max(1, rx - 2), cy - 2, cx - max(1, rx - 2), cy + 2, 1), DARK_GOLD)
        detail(mask_rect(cx, cy - 3, cx + 1, cy - 2), WHITE)
    else:
        detail(mask_line(cx, cy - 3, cx, cy + 3, 1), WHITE)


def heart(col: int, empty: bool) -> None:
    cx, cy = col * 32 + 16, 144
    mask = mask_poly([
        (cx, cy + 4), (cx - 4, cy), (cx - 4, cy - 2),
        (cx - 2, cy - 4), (cx, cy - 2), (cx + 2, cy - 4),
        (cx + 4, cy - 2), (cx + 4, cy),
    ])
    draw(mask, MAGENTA if empty else HEART, OUTLINE)
    if empty:
        # Repaint interior magenta after outlining, keeping a clean 1px key-color cavity.
        inner = mask_poly([(cx, cy + 1), (cx - 2, cy - 1), (cx - 1, cy - 2), (cx, cy - 1), (cx + 1, cy - 2), (cx + 2, cy - 1)])
        detail(inner, MAGENTA)
    else:
        detail(mask_rect(cx - 2, cy - 2, cx - 1, cy - 1), WHITE)


def slash_fx() -> None:
    cx, cy = 208, 143
    arc = union(
        mask_line(cx - 8, cy + 5, cx - 5, cy - 1, 2),
        mask_line(cx - 5, cy - 1, cx, cy - 4, 2),
        mask_line(cx, cy - 4, cx + 5, cy - 1, 2),
        mask_line(cx + 5, cy - 1, cx + 8, cy + 5, 2),
    )
    draw(arc, WHITE, OUTLINE)
    detail(mask_line(cx - 5, cy - 1, cx, cy - 2, 1), STEEL)


def poof_fx() -> None:
    cx, cy = 240, 144
    flecks = [
        mask_rect(cx - 1, cy - 9, cx + 1, cy - 6),
        mask_rect(cx - 1, cy + 6, cx + 1, cy + 9),
        mask_rect(cx - 9, cy - 1, cx - 6, cy + 1),
        mask_rect(cx + 6, cy - 1, cx + 9, cy + 1),
        mask_rect(cx - 7, cy - 7, cx - 5, cy - 5),
        mask_rect(cx + 5, cy - 7, cx + 7, cy - 5),
        mask_rect(cx - 7, cy + 5, cx - 5, cy + 7),
        mask_rect(cx + 5, cy + 5, cx + 7, cy + 7),
    ]
    for i, fleck in enumerate(flecks):
        draw(fleck, WHITE if i % 2 == 0 else STEEL, OUTLINE)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)


def write_png(path: Path) -> None:
    out_w, out_h = W * SCALE, H * SCALE
    raw = bytearray()
    for y in range(H):
        expanded_row = b"".join(PALETTE[pixels[y][x]] * SCALE for x in range(W))
        for _ in range(SCALE):
            raw.append(0)
            raw.extend(expanded_row)
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(png_chunk(b"IHDR", struct.pack(">IIBBBBB", out_w, out_h, 8, 2, 0, 0, 0)))
    png.extend(png_chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(png_chunk(b"IEND", b""))
    path.write_bytes(png)


def main() -> None:
    # Row 1.
    for col, pose in enumerate(("run_a", "run_b", "leap", "windup", "slash", "hurt", "defeated")):
        ninja_frame(col, pose)

    # Row 2.
    demon_frame(0, False)
    demon_frame(1, True)
    for col, frame in ((2, 0), (3, 1), (4, 2)):
        rock_frame(col, frame)

    # Row 3.
    bamboo()
    spikes()

    # Row 4.
    tree(0, False)
    tree(1, True)
    torii()
    lantern()

    # Row 5.
    for col in range(4):
        coin(col, col)
    heart(4, False)
    heart(5, True)
    slash_fx()
    poof_fx()

    destination = Path(__file__).with_name("slash_and_leap_sprite_sheet.png")
    write_png(destination)
    print(destination)


if __name__ == "__main__":
    main()
