"""Turn a generated icon's painted-in background into real transparency.

Image models asked for "a transparent background" routinely paint the *checkerboard* --
two near-white shades -- into the RGB pixels instead. The shop tiles are dark, so a
white background or even a white fringe shows.

Only background connected to the border is removed, so white highlights inside the
subject survive. Run it on the raw generation:

    python tools/icon-cutout.py <in.png> <out.png>
"""

import sys

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

SIZE = 512
LIGHT = 224      # a background shade is at least this bright ...
NEUTRAL = 16     # ... and this close to grey; the blue glow never is
FEATHER = 1.0    # px, to keep the cut edge from looking stamped
SPECK = 0.0005   # drop foreground blobs smaller than this fraction of the frame


def main(src, dst):
    im = Image.open(src).convert("RGB")
    a = np.asarray(im).astype(np.int16)

    light = a.min(axis=2) >= LIGHT
    neutral = (a.max(axis=2) - a.min(axis=2)) <= NEUTRAL
    lab, _ = ndimage.label(light & neutral)
    border = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]])))
    border.discard(0)
    bg = np.isin(lab, list(border))

    # One pixel of erosion: the outermost ring is a blend of subject and background, and
    # keeping it leaves a white fringe on the dark tile.
    fg = ndimage.binary_erosion(~bg, np.ones((3, 3), bool), border_value=1)

    # Specks the generator left in the corners are foreground by every test above, and
    # show on the tile as dirt. Nothing that small is part of the subject.
    lab, n = ndimage.label(fg)
    keep = [i for i in range(1, n + 1) if (lab == i).sum() >= fg.size * SPECK]
    fg = np.isin(lab, keep)

    alpha = Image.fromarray((fg * 255).astype(np.uint8), "L")
    alpha = alpha.filter(ImageFilter.GaussianBlur(FEATHER))
    out = im.copy()
    out.putalpha(alpha)
    out = out.resize((SIZE, SIZE), Image.LANCZOS)
    out.save(dst)

    cov = np.asarray(out)[:, :, 3] > 10
    print("%s  %dx%d  opaque %.1f%%" % (dst, out.width, out.height, 100 * cov.mean()))
    ys, xs = np.nonzero(cov)
    print("  bbox x %d..%d  y %d..%d" % (xs.min(), xs.max(), ys.min(), ys.max()))


main(sys.argv[1], sys.argv[2])
