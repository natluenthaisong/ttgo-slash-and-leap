# Codex image-generation prompt for the Slash & Leap sprite sheet

Paste everything below the line into Codex. When you get the image back, save it as
`art/spritesheet.png` — the integration pipeline (slice → quantize to the palette →
RGB565 C arrays → pushImage with magenta color key) takes it from there.

---

Generate ONE pixel-art sprite sheet image for a tiny ESP32 LCD game called "Slash & Leap"
(a vertical ninja runner on a 135x240 screen, viewed from behind/above as the ninja runs
UP a mountain path).

HARD TECHNICAL REQUIREMENTS (must all hold, they matter more than beauty):
- Output exactly 1024x640 px: a logical 256x160 pixel-art canvas scaled 4x with
  NEAREST-NEIGHBOR only. Every "art pixel" is a crisp 4x4 block. No anti-aliasing,
  no gradients, no blur, no soft shadows, no dithering noise.
- Background of the whole sheet: solid pure magenta #FF00FF (it is the transparency key).
  No grid lines, no labels, no watermark, no border art.
- The canvas is a strict grid of 32x32-pixel cells: 8 columns x 5 rows. Each sprite is
  centered in its cell (feet/base on the cell's bottom edge). Wide sprites span 3 cells
  where noted and are vertically centered in that 96x32 band.
- Flat colors with a consistent 1-art-pixel dark outline (#28241E). Single light source
  from the top. NO baked-in drop shadows (the game draws its own).
- Use ONLY this palette (+ magenta bg): #28241E outline, #2E3458 navy, #1E223C dark navy,
  #F4C8A0 skin, #D83830 red, #FFFFFF white, #6EBA46 bamboo green, #468228 dark green,
  #C83C32 demon red, #8C221E dark demon red, #B0B4BE steel, #6E727E dark steel,
  #967654 rock brown, #684E34 dark brown, #FFD23C gold, #BE8C14 dark gold, #EB3C46 heart red.
- Sprites must read clearly when shrunk to 25%: chunky silhouettes, big shapes, no fine detail.

CONTENT — row by row (col 1 = leftmost):
Row 1 — NINJA, seen from BEHIND (back view, he faces away/up), ~16 wide x 24 tall art px,
dark navy gi, red headband with two tails, red scarf, katana sheathed diagonally on back:
  c1 run frame A (left leg forward)   c2 run frame B (right leg forward)
  c3 leap (tucked legs, arms out)     c4 slash wind-up (katana raised high)
  c5 slash swing (blade sweeping overhead, white arc)  c6 hurt (flinch, dizzy stars)
  c7 defeated (sitting, head down)    c8 empty
Row 2 — ENEMIES/OBSTACLES:
  c1 demon imp frame A (round red oni, white horns, angry eyes, ~18x16)
  c2 demon imp frame B (mid-hop)      c3 rolling rock frame A (round boulder ~22x22)
  c4 rock frame B (rotated cracks)    c5 rock frame C (rotated again)
  c6-c8 empty
Row 3 — WIDE OBSTACLES (each spans 3 cells = 96x32 band, drawn ~88 art px wide):
  c1-c3 bamboo stalk lying horizontally across the path: segmented green cane with joint
        rings and a leaf sprig at one end
  c4-c6 spike strip: steel base bar with a row of 8 menacing triangular spikes pointing up
  c7-c8 empty
Row 4 — SCENERY:
  c1 pine-ish tree A (round canopy)   c2 tree B (taller, two-lobed canopy)
  c3 red torii gate (~24x26)          c4 stone lantern (~14x22)
  c5-c8 empty
Row 5 — PICKUPS & FX (small, centered):
  c1 coin spin frame A (full circle, 12x12)  c2 coin spin B (3/4 ellipse)
  c3 coin spin C (thin edge-on)              c4 coin spin D (3/4, other side)
  c5 heart icon full (10x10)                 c6 heart icon empty outline
  c7 white slash crescent FX (curved blade trail, opening downward)
  c8 poof/burst FX (6-8 flecks radiating)

Style: cozy 16-bit handheld vibe (Game Boy Color / Kirby-ish charm), cute but readable,
Japanese mountain-trail theme. Consistent proportions between all frames of the same
character so they animate cleanly.
