#!/usr/bin/env python3
# Capture the nested kwin's output via org.kde.KWin.ScreenShot2 and write a PNG.
# Usage: shot.py <out.png> [workspace | screen | area x y w h]
import os, sys, struct, zlib
import dbus

OUT = sys.argv[1]
mode = sys.argv[2] if len(sys.argv) > 2 else "workspace"

bus = dbus.SessionBus()
obj = bus.get_object("org.kde.KWin.ScreenShot2", "/org/kde/KWin/ScreenShot2")
iface = dbus.Interface(obj, "org.kde.KWin.ScreenShot2")

r, w = os.pipe()
opts = dbus.Dictionary({"native-resolution": dbus.Boolean(True)}, signature="sv")

if mode == "area":
    x, y, ww, hh = (int(a) for a in sys.argv[3:7])
    reply = iface.CaptureArea(x, y, ww, hh, opts, dbus.types.UnixFd(w))
elif mode == "screen":
    reply = iface.CaptureActiveScreen(opts, dbus.types.UnixFd(w))
else:
    reply = iface.CaptureWorkspace(opts, dbus.types.UnixFd(w))

os.close(w)
meta = {str(k): v for k, v in reply.items()}
width = int(meta["width"]); height = int(meta["height"])
stride = int(meta.get("stride", width * 4))

buf = bytearray()
while True:
    chunk = os.read(r, 1 << 20)
    if not chunk:
        break
    buf += chunk
os.close(r)

def write_png(path, w, h, stride, raw):
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        row = raw[y*stride : y*stride + w*4]
        for x in range(0, len(row), 4):
            b, g, r_, a = row[x], row[x+1], row[x+2], row[x+3]
            rows += bytes((r_, g, b))
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    idat = zlib.compress(bytes(rows), 6)
    with open(path, "wb") as f:
        f.write(sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b""))

if len(buf) >= stride * height and width > 0 and height > 0:
    write_png(OUT, width, height, stride, buf)
    print(f"wrote {OUT} ({width}x{height})")
else:
    print("SHORT READ - not writing PNG", file=sys.stderr)
    sys.exit(3)
