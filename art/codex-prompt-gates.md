# Codex image-generation prompt for the Slash & Leap ZONE GATES sheet

This is a second sheet (the main sprite sheet lives at `art/spritesheet.png`).
Paste everything below the line into Codex. When the image comes back, save it as
`art/gates.png` — integration slices the four 96x64 gate blocks + row-5 extras
into `src/sprites_gates.h` and replaces the procedural drawGate() with the themed
gate for the zone being entered.

Key constraint encoded in the prompt: the path width varies per zone (60-76 px),
so every gate must keep an open transparent passage >= 78 px between pillar inner
edges — pillars land on the grass regardless of lane width.

---

Generate ONE pixel-art sprite sheet image for "Slash & Leap", a tiny vertical ninja
runner on an ESP32 LCD (135x240 portrait, top-down view, world scrolls down). This
sheet contains the ZONE GATES: big ceremonial gateways spanning the mountain path
that the ninja runs through at each stage boundary.

HARD TECHNICAL REQUIREMENTS (these matter more than beauty):
- Output exactly 1024x640 px: a logical 256x160 pixel-art canvas scaled 4x with
  NEAREST-NEIGHBOR only. Every art pixel is a crisp 4x4 block. No anti-aliasing,
  no gradients, no blur, no soft shadows, no dithering.
- Whole-sheet background: solid pure magenta #FF00FF (transparency key). No grid
  lines, labels, watermarks, or border art.
- The canvas is a strict grid of 32x32-pixel cells: 8 columns x 5 rows. Each GATE
  occupies a 3-column x 2-row block (96x64 logical px), pillars resting on the
  block's bottom edge.
- CRITICAL for every gate: it is an archway seen from the front. Two pillars at the
  far left and far right (each at most 9 px wide), crossbeams across the TOP third
  only. The open passage between the inner edges of the pillars must be at least
  78 px wide and fully MAGENTA (transparent) below the beams — the road, and the
  ninja, show through the opening.
- Flat colors, consistent 1-art-pixel dark outline (#28241E), light from above,
  NO baked-in ground shadows (the game draws its own).
- Use ONLY this palette (+ magenta bg): #28241E outline, #C83C32 torii red,
  #8C221E dark red, #6EBA46 bamboo green, #468228 dark green, #F0D25A rope tan,
  #FFD23C gold, #B0B4BE stone grey, #6E727E dark stone, #F8F8F8 white,
  #A8D8E8 ice blue, #967654 wood brown, #684E34 dark brown, #FFF6E0 paper cream.

CONTENT — four gates in 3x2-cell blocks, plus small extras:
Block A (cols 1-3, rows 1-2) — BAMBOO GROVE GATE: an arch lashed together from
  thick green bamboo poles (visible segment rings), rope-tan bindings at the
  joints, a few leafy sprigs sprouting from the top corners. Rustic and friendly.
Block B (cols 4-6, rows 1-2) — TORII VILLAGE GATE: a grand classic red torii —
  double top beam with upswept ends, dark caps, a small gold plaque hanging at the
  center of the lower beam, one tiny paper lantern hanging from each side.
Block C (cols 1-3, rows 3-4) — ROCKY PASS GATE: a rough megalithic stone arch —
  two weathered grey pillars of stacked stone with cracks and a little moss,
  one massive lintel stone across the top. Ancient and heavy.
Block D (cols 4-6, rows 3-4) — SNOWY SUMMIT GATE: a red torii like Block B but
  winter-worn: thick white snow caps along the top beams, three small icicles
  hanging from the lower beam, snow dusting the pillar tops.
Row 5 small extras (one per 32x32 cell):
  c1 gold heal-glow ring (circular sparkle burst, ~24px, for the heart restored
     when passing a gate)
  c2 small vertical banner flag on a pole (red with cream stripe, ~12x28)
  c3 the same banner flag fluttering (frame B, cloth bent)
  c4-c8 empty

Style: same family as a cozy 16-bit ninja runner — chunky, readable at 25%,
Japanese mountain-trail ceremony vibes. The four gates should feel like one set:
same proportions and outline weight, different materials.
