#!/usr/bin/env python3
"""Generates the OBSOLETE textures and the night-sky cube map faces.
Everything here is made by the group (licence: own work / CC0).  Run from the repo root:
    python tools/make_textures.py
Needs: pip install pillow numpy
Output goes to  Skybox Activity/ActivityJ_Skybox/SkyboxLab/Textures/
"""
import os, sys
import numpy as np
from PIL import Image

OUT = os.path.join("Skybox Activity", "ActivityJ_Skybox", "SkyboxLab", "Textures")
os.makedirs(os.path.join(OUT, "Skybox"), exist_ok=True)
rng = np.random.default_rng(20261031)
N = 256

def periodic_noise(cx, cy, size=N):
    """smooth value noise on a cx x cy periodic grid, so the texture tiles seamlessly"""
    g = rng.random((cy, cx))
    ys = np.linspace(0, cy, size, endpoint=False)
    xs = np.linspace(0, cx, size, endpoint=False)
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    fy = ys - y0; fx = xs - x0
    fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
    y1 = (y0 + 1) % cy; x1 = (x0 + 1) % cx
    a = g[np.ix_(y0, x0)]; b = g[np.ix_(y0, x1)]; c = g[np.ix_(y1, x0)]; d = g[np.ix_(y1, x1)]
    top = a * (1 - fx)[None, :] + b * fx[None, :]
    bot = c * (1 - fx)[None, :] + d * fx[None, :]
    return top * (1 - fy)[:, None] + bot * fy[:, None]

def save(name, arr, folder=""):
    arr = np.clip(arr, 0, 1)
    Image.fromarray((arr * 255).astype(np.uint8), "RGB").save(os.path.join(OUT, folder, name))
    print("wrote", os.path.join(folder, name))

yy, xx = np.mgrid[0:N, 0:N]
u = xx / N; v = yy / N

# ---- concrete: blotchy grey with speckle and a few darker stains
n = 0.55 * periodic_noise(4, 4) + 0.3 * periodic_noise(16, 16) + 0.15 * periodic_noise(64, 64)
g = 0.30 + 0.22 * n + 0.04 * rng.random((N, N))
concrete = np.stack([g, g * 1.0, g * 1.05], -1)
save("concrete.png", concrete)

# ---- brushed metal: horizontal streaks, panel seams, rivets
streak = periodic_noise(2, 96)
g = 0.50 + 0.18 * streak + 0.04 * rng.random((N, N))
seam = ((yy % 128) < 3) | ((xx % 128) < 3)
g = np.where(seam, g * 0.45, g)
for cy in (14, 114, 142, 242):
    for cx in (14, 114, 142, 242):
        d2 = (xx - cx) ** 2 + (yy - cy) ** 2
        g = np.where(d2 < 9, g * 0.5 + 0.35 * (d2 < 4), g)
metal = np.stack([g * 0.92, g * 0.97, g * 1.05], -1)
save("metal.png", metal)

# ---- wood: bands along x
rings = np.sin((v * 22 + 3.0 * periodic_noise(2, 6)) * 2 * np.pi)
g = 0.5 + 0.5 * rings
tone = 0.6 + 0.25 * g + 0.1 * periodic_noise(3, 24)
wood = np.stack([0.50 * tone + 0.12, 0.33 * tone + 0.07, 0.17 * tone + 0.03], -1)
save("wood.png", wood)

# ---- hazard stripes: diagonal yellow / black, grimy
stripe = (((xx + yy) // 32) % 2) == 0
base = np.where(stripe[..., None], np.array([0.95, 0.75, 0.05]), np.array([0.07, 0.07, 0.07]))
grime = 0.75 + 0.25 * periodic_noise(8, 8)
save("hazard.png", base * grime[..., None])

# ---- conveyor belt: dark rubber, yellow chevrons pointing along +u (the scroll direction), edge lines
g = 0.10 + 0.05 * periodic_noise(8, 8) + 0.03 * rng.random((N, N))
rub = np.stack([g, g, g * 1.1], -1)
cx = (xx % 64) / 64.0
cyv = np.abs(((yy / N * 4.0) % 1.0) - 0.5)
chev = (np.abs(cx - 0.25 - cyv * 0.7) < 0.06) & (cyv < 0.45)
edge = (yy % 128) < 6
rub[chev] = (0.95, 0.75, 0.08)
rub[edge & ~chev] = (0.55, 0.50, 0.10)
save("conveyor.png", rub)

# ---- exit hatch: green plate with white bars
g = 0.15 + 0.05 * periodic_noise(8, 8)
pad = np.stack([g * 0.5, g * 2.2, g * 0.8], -1)
bars = (np.abs(((xx / N * 8) % 1.0) - 0.5) < 0.08)
pad[bars] = (0.85, 1.0, 0.85)
save("exitpad.png", pad)

# ---- night sky: 6 faces of a cube map, faint orange glow of the plant on the horizon, stars
S = 512
def sky_face(kind):
    y = np.linspace(0, 1, S)[:, None] * np.ones((1, S))       # 0 top .. 1 bottom of the image
    if kind == "side":
        t = y                                                   # bottom of the image = horizon
        r = 0.01 + 0.30 * t ** 4
        g = 0.015 + 0.13 * t ** 4
        b = 0.04 + 0.14 * t ** 3
    elif kind == "up":
        r = np.full((S, S), 0.008); g = np.full((S, S), 0.012); b = np.full((S, S), 0.035)
    else:
        r = np.full((S, S), 0.03); g = np.full((S, S), 0.02); b = np.full((S, S), 0.02)
    img = np.stack([r, g, b], -1)
    if kind != "down":
        stars = rng.random((S, S)) > 0.9985
        weight = (1.0 - y) if kind == "side" else np.ones((S, S))
        img = img + (stars * rng.random((S, S)) * weight)[..., None] * np.array([0.9, 0.9, 1.0])
    return img
for nm, kind in (("rt", "side"), ("lf", "side"), ("up", "up"), ("dn", "down"), ("bk", "side"), ("ft", "side")):
    save("night_%s.png" % nm, sky_face(kind), "Skybox")
print("done")
