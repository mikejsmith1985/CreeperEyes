import numpy as np, sys
from PIL import Image, ImageDraw, ImageFilter
from shapely.geometry import Polygon, MultiPolygon
sys.path.insert(0, '.')
import scene
R = 4  # px per mm
N = int(scene.W * R)
def mask(geom):
    im = Image.new('L', (N, N), 0); d = ImageDraw.Draw(im)
    geoms = geom.geoms if hasattr(geom, 'geoms') else [geom]
    for g in geoms:
        if g.is_empty or g.geom_type != 'Polygon': continue
        d.polygon([(x * R, (scene.H - y) * R) for x, y in g.exterior.coords], fill=255)
        for hole in g.interiors: d.polygon([(x * R, (scene.H - y) * R) for x, y in hole.coords], fill=0)
    return np.asarray(im, np.float32) / 255
hill, windows = scene.hill_layer(); tree = scene.tree_layer(); fore = scene.fore_layer()
t, res = scene.sky_relief()
tt = np.asarray(Image.fromarray(t).resize((N, N), Image.BILINEAR))
def render(light, warm_house, name):
    trans = np.exp(-1.25 * (tt - 0.5))                     # light through the lithophane
    sky = trans[..., None] * np.array(light, np.float32)[None, None, :]
    img = sky.copy()
    wm = mask(windows)
    img = img * (1 - wm[..., None]) + (wm[..., None] * np.array(warm_house, np.float32))
    for m, colr, sh in ((mask(hill), (38, 30, 58), 6), (mask(tree), (22, 22, 24), 10), (mask(fore), (8, 8, 9), 14)):
        shadow = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(sh)), np.float32) / 255
        shadow = np.roll(np.roll(shadow, sh, 0), sh // 2, 1)
        img = img * (1 - 0.45 * shadow[..., None])
        img = img * (1 - m[..., None]) + m[..., None] * np.array(colr, np.float32)
    out = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))
    fr = Image.new('RGB', (N + 80, N + 80), (12, 12, 12)); fr.paste(out, (40, 40)); fr.save(name)
render((70, 80, 150), (255, 150, 40), 'preview_dim.png')
render((250, 250, 255), (255, 235, 200), 'preview_flash.png')
a = Image.open('preview_dim.png'); b = Image.open('preview_flash.png')
c = Image.new('RGB', (a.width * 2 + 20, a.height), (0, 0, 0)); c.paste(a, (0, 0)); c.paste(b, (a.width + 20, 0)); c = c.resize((c.width // 2, c.height // 2), Image.LANCZOS); c.save('preview.png')
