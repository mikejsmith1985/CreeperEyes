#!/usr/bin/env python3
"""Renders every eye style the same way the firmware does, into a contact sheet PNG.
Usage: python3 tools/eyepreview.py /tmp/ue out.png"""
import runpy, sys
from PIL import Image, ImageDraw

out = sys.argv[2] if len(sys.argv) > 2 else 'eyes.png'
g = runpy.run_path(__file__.replace('eyepreview.py', 'eyegen.py'), run_name='eyegen')
tables, STYLES = g['tables'], g['STYLES']

def c565(p):
    return (((p >> 11) & 31) * 255 // 31, ((p >> 5) & 63) * 255 // 63, (p & 31) * 255 // 31)

def render(t, imin, imax, gx=512, gy=512, frac=0.5, lid=(0x50, 0x68, 0x50)):
    sw, sh, sc = t['sclera']; iw, ih, ir = t['iris']; pw, ph, po = t['polar']
    up, lo = t['upper'][2], t['lower'][2]
    iscale = int(imin + frac * (imax - imin))
    thr = (128 * (1023 - iscale) + 512) // 1024
    iscl = ih * 65536 // max(1, thr)
    ex = gx * (sw - 128) // 1023 + 4; ex = min(ex, sw - 128)
    ey = gy * (sh - 128) // 1023
    im = Image.new('RGB', (128, 128)); px = im.load()
    for y in range(128):
        for x in range(128):
            if lo[y * 128 + x] <= 0 or up[y * 128 + x] <= 0:
                px[x, y] = lid; continue
            sx, sy = ex + x, ey + y
            ixx, iyy = sx - (sw - pw) // 2, sy - (sh - ph) // 2
            if 0 <= ixx < pw and 0 <= iyy < ph:
                p = po[iyy * pw + ixx]; d = p & 0x7F
                if d < thr:
                    d = d * iscl // 65536; a = iw * (p >> 7) // 512
                    px[x, y] = c565(ir[d * iw + a]); continue
            px[x, y] = c565(sc[sy * sw + sx])
    return im

cols = 4
rows = (len(STYLES) + cols - 1) // cols
sheet = Image.new('RGB', (cols * 150, rows * 165), (20, 20, 20))
d = ImageDraw.Draw(sheet)
for i, (sid, label, src, imin, imax) in enumerate(STYLES):
    im = render(tables[sid], imin, imax)
    x, y = (i % cols) * 150 + 11, (i // cols) * 165 + 8
    sheet.paste(im, (x, y))
    d.text((x, y + 132), label, fill=(230, 230, 230))
sheet.save(out)
print('wrote', out)
