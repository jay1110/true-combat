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
            gray = min(255, round(max(r, g, b) * 255 / 128))
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
