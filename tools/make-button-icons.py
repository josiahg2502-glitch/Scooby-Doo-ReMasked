"""Builds the per-controller button prompt textures (Play/UserData/button_icons/<xbox|playstation|nintendo>).

Re-Masked's prompts ("A Select", "B Back") are GameCube button pictures packed into the game's font texture. Each set
is that texture, upscaled 4x, with the A, B, X and Y buttons redrawn from Kenney's Input Prompts (CC0, tools/button-art)
in their own spots, saved under the game texture's name. Aurora loads the set for the controller in use
(texture_replacement.cpp, hle/input/pad.cpp). GameCube buttons map by position: A south, B east, X west, Y north.

    python tools/make-button-icons.py --src <texture dump of tex1_512x128_1eb582c7b05a3277_6> [--out Play/UserData/button_icons]
Get the source with [video] texture_dumps = true (it lands in UserData/Cache/texture_dumps). Needs ImageMagick (magick).
"""
import argparse
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "tools", "button-art")
FONT = "tex1_512x128_1eb582c7b05a3277_6.png"
SCALE = 4
SEEDS = {"A": (32, 57), "B": (105, 62), "X": (325, 58), "Y": (395, 65)}  # a pixel inside each button, font texture coords
STYLES = {
    "xbox": {"A": "xbox_button_color_a", "B": "xbox_button_color_b", "X": "xbox_button_color_x", "Y": "xbox_button_color_y"},
    "playstation": {"A": "playstation_button_color_cross", "B": "playstation_button_color_circle",
                    "X": "playstation_button_color_square", "Y": "playstation_button_color_triangle"},
    "nintendo": {"A": "switch_button_b", "B": "switch_button_a", "X": "switch_button_y", "Y": "switch_button_x"},
}


def button_mask(font, seed):
    """The button's pixels: everything opaque and not white (the letters around it are white) connected to the seed."""
    w, h = font.size
    px = font.load()
    inside = lambda p: px[p][3] > 64 and min(px[p][:3]) < 200
    mask = Image.new("L", font.size, 0)
    m = mask.load()
    stack = [seed]
    while stack:
        x, y = stack.pop()
        if 0 <= x < w and 0 <= y < h and not m[x, y] and inside((x, y)):
            m[x, y] = 255
            stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    assert mask.getbbox(), f"no button at {seed}"
    return mask


def render_svg(svg, size, tmp, dark_symbol):
    png = os.path.join(tmp, os.path.basename(svg) + f".{size}.png")
    subprocess.run(["magick", "-background", "none", "-density", "600", svg, "-resize", f"{size}x{size}", png], check=True)
    icon = Image.open(png).convert("RGBA")
    # The symbol is cut out of the button: fill what the outside can't reach (white, dark on Nintendo's white buttons).
    outside = Image.new("L", (icon.width + 2, icon.height + 2), 0)
    outside.paste(icon.getchannel("A").point(lambda v: 255 if v > 32 else 0), (1, 1))
    ImageDraw.floodfill(outside, (0, 0), 128)
    holes = outside.crop((1, 1, icon.width + 1, icon.height + 1)).point(lambda v: 255 if v == 0 else 0)
    fill = Image.new("RGBA", icon.size, (40, 40, 40, 255) if dark_symbol else (255, 255, 255, 255))
    return Image.alpha_composite(Image.composite(fill, Image.new("RGBA", icon.size), holes), icon)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", default=os.path.join(ROOT, "Play", "UserData", "button_icons"))
    a = ap.parse_args()
    font = Image.open(a.src).convert("RGBA")
    assert font.size == (512, 128), font.size
    masks = {name: button_mask(font, seed) for name, seed in SEEDS.items()}
    big = font.resize((512 * SCALE, 128 * SCALE), Image.LANCZOS)
    with tempfile.TemporaryDirectory() as tmp:
        for style, buttons in STYLES.items():
            atlas = big.copy()
            for name, mask in masks.items():
                # Clear the old button (grown a pixel for its antialiased rim), then draw the new one in its box.
                grown = mask.resize(big.size, Image.NEAREST).filter(ImageFilterMax)
                atlas.paste((0, 0, 0, 0), mask=grown)
                x0, y0, x1, y1 = (v * SCALE for v in mask.getbbox())
                size = min(x1 - x0, y1 - y0)
                icon = render_svg(os.path.join(ART, style, buttons[name] + ".svg"), size, tmp, style == "nintendo")
                atlas.alpha_composite(icon, ((x0 + x1 - icon.width) // 2, (y0 + y1 - icon.height) // 2))
            os.makedirs(os.path.join(a.out, style), exist_ok=True)
            atlas.save(os.path.join(a.out, style, FONT))
            print("wrote", os.path.join(a.out, style, FONT))


if __name__ == "__main__":
    from PIL import ImageFilter
    ImageFilterMax = ImageFilter.MaxFilter(SCALE + 1)
    main()
