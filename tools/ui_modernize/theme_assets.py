"""Neutral paint layers for user-selected palettes; original artwork stays frozen."""


def layers(pixels, icon=False):
    body = bytearray(len(pixels))
    accent = bytearray(len(pixels))
    for at in range(0, len(pixels), 4):
        r, g, b, a = pixels[at:at + 4]
        gold = r > 100 and g > 70 and r > b * 1.25 and g > b * 1.2
        if gold or icon:
            accent[at:at + 4] = bytes((255, 255, 255, a))
        else:
            # The original dark raster is not a luminance mask: multiplying it
            # by a light palette can never produce a light panel. Keep a small
            # neutral bevel variation while allowing the selected base RGB to
            # determine the real surface tone, including Grey/Black and Custom.
            gray = min(255, 224 + round(max(r, g, b) * 31 / 128))
            body[at:at + 4] = bytes((gray, gray, gray, a))
    return bytes(body), bytes(accent)


def extend(images, output, tga):
    original = list(images.items())
    for name, (w, h, pixels) in original:
        if name in ('background', 'cursor'):
            continue
        body, accent = layers(pixels, name.startswith('icons/'))
        for suffix, data in (('body', body), ('accent', accent)):
            key = 'theme/' + name + '_' + suffix
            images[key] = w, h, data
            output['tce2_ui/' + key + '.tga'] = tga(w, h, data)
