#!/usr/bin/env python3
"""Render Luke Steuber's original Signal Station antenna mark from geometry."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]

def render(size, menu=False):
    scale = 4
    image = Image.new('RGBA', (size * scale, size * scale), (0, 0, 0, 0) if menu else '#07151c')
    draw = ImageDraw.Draw(image)
    unit = size * scale / 100
    cyan = '#ffffff' if menu else '#a5eef5'
    amber = '#ffffff' if menu else '#ffc044'
    def box(x0, y0, x1, y1):
        return tuple(round(x * unit) for x in (x0, y0, x1, y1))
    for radius in (39, 28, 17):
        draw.arc(box(50-radius, 45-radius, 50+radius, 45+radius), 112, 428, fill=cyan, width=max(1, round(4 * unit)))
    draw.line(box(50, 51, 50, 87), fill=cyan, width=max(1, round(4 * unit)))
    draw.ellipse(box(44, 39, 56, 51), fill=amber)
    return image.resize((size, size), Image.Resampling.LANCZOS)

for size in (80, 144):
    render(size).save(ROOT / f'store/assets/signal-station-{size}.png')
# Threshold original geometry for crisp monochrome launcher rendering.
icon = render(25, True)
mask = icon.getchannel('A').point(lambda value: 255 if value >= 128 else 0).convert('1')
mask.save(ROOT / 'resources/images/menu-icon.png')
