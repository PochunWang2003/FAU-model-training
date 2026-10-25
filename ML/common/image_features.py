"""Small dependency-free image helpers for the offline PGM prototype."""

import math


def read_pgm(path):
    with open(path, "rb") as stream:
        content = stream.read()
    index = 0
    tokens = []
    while len(tokens) < 4:
        while index < len(content) and content[index] in b" \t\r\n":
            index += 1
        if index < len(content) and content[index] == ord("#"):
            while index < len(content) and content[index] not in b"\r\n":
                index += 1
            continue
        start = index
        while index < len(content) and content[index] not in b" \t\r\n":
            index += 1
        if start == index:
            raise ValueError("incomplete PGM header")
        tokens.append(content[start:index])
    magic = tokens[0]
    width, height, maximum = (int(item) for item in tokens[1:])
    if magic not in (b"P5", b"P2") or width < 3 or height < 3 or maximum < 1 or maximum > 255:
        raise ValueError("unsupported PGM image")
    if magic == b"P5":
        if index < len(content) and content[index] == 13 and index + 1 < len(content) and content[index + 1] == 10:
            index += 2
        elif index < len(content) and content[index] in b" \t\r\n":
            index += 1
        pixels = list(content[index:index + width * height])
    else:
        while index < len(content) and content[index] in b" \t\r\n":
            index += 1
        pixels = [int(item) for item in content[index:].split()[:width * height]]
    if len(pixels) != width * height:
        raise ValueError("PGM pixel data length does not match its dimensions")
    if maximum != 255:
        pixels = [round(value * 255 / maximum) for value in pixels]
    return width, height, pixels


def focus_features(path):
    width, height, pixels = read_pgm(path)
    laplacian = []
    gradients = []
    values = []
    for y in range(1, height - 1):
        for x in range(1, width - 1):
            center = pixels[y * width + x]
            left = pixels[y * width + x - 1]
            right = pixels[y * width + x + 1]
            up = pixels[(y - 1) * width + x]
            down = pixels[(y + 1) * width + x]
            laplacian.append(left + right + up + down - 4 * center)
            gradients.append(abs(right - left) + abs(down - up))
            values.append(center)
    mean = sum(values) / len(values)
    variance = sum((value - mean) ** 2 for value in values) / len(values)
    lap_mean = sum(laplacian) / len(laplacian)
    lap_variance = sum((value - lap_mean) ** 2 for value in laplacian) / len(laplacian)
    tenengrad = sum(value * value for value in gradients) / len(gradients)
    return [math.log1p(lap_variance), math.log1p(tenengrad), variance / 65025.0]


def focus_score(path):
    """Return a classic variance-of-Laplacian score; this is not a model."""
    return math.expm1(focus_features(path)[0])
