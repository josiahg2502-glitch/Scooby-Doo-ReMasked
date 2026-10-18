"""Replaces the HUD/pause Scooby portrait (a 32x32 picture in the game) with the launcher's Scooby picture.

    python tools/make-hud-portrait.py [--out Play/UserData/texture_replacements/ReMasked]
"""
import argparse
import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORTRAIT = "tex1_32x32_558883f9f635a36c_6.png"

ap = argparse.ArgumentParser()
ap.add_argument("--out", default=os.path.join(ROOT, "Play", "UserData", "texture_replacements", "ReMasked"))
a = ap.parse_args()
pic = Image.open(os.path.join(ROOT, "launcher", "scooby.png")).convert("RGBA")
side = max(pic.size)
square = Image.new("RGBA", (side, side))
square.alpha_composite(pic, ((side - pic.width) // 2, side - pic.height))
# The game stores the portrait upside down.
square = square.resize((256, 256), Image.LANCZOS).transpose(Image.FLIP_TOP_BOTTOM)
os.makedirs(a.out, exist_ok=True)
square.save(os.path.join(a.out, PORTRAIT))
print("wrote", os.path.join(a.out, PORTRAIT))
