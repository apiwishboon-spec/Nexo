#!/usr/bin/env python3
"""Interactive Cardputer emulator front-end.

Spawns the host-built `nexo_emu` (which runs the real ui.cpp) and shows its
240x135 framebuffer in a Tk window, forwarding your keystrokes to the firmware
UI and re-rendering on every event.

    ./emu.sh            # build + launch
"""
import os
import struct
import subprocess
import sys
import threading
import tkinter as tk
from queue import SimpleQueue

from PIL import Image, ImageTk

W, H = 240, 135
SCALE = 3

NAMED = {
    "Up": "up", "Down": "down", "Left": "left", "Right": "right",
    "Return": "enter", "KP_Enter": "enter", "Escape": "esc",
    "BackSpace": "backspace", "Delete": "del", "Tab": "tab",
    "ISO_Left_Tab": "tab", "space": "space",
}


def read_exact(stream, n):
    buf = b""
    while len(buf) < n:
        chunk = stream.read(n - len(buf))
        if not chunk:
            raise EOFError("emulator closed the stream")
        buf += chunk
    return buf


def read_frame(stream):
    (n,) = struct.unpack("<I", read_exact(stream, 4))
    return read_exact(stream, n)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    binpath = os.path.join(here, "out", "nexo_emu")
    if not os.path.exists(binpath):
        print("nexo_emu not built; run tools/preview/build_emu.sh", file=sys.stderr)
        return 1

    # Run the emulator from the repo root and point it at the music folder so
    # the Music screen lists real files regardless of where this was launched.
    repo_root = os.path.abspath(os.path.join(here, "..", "..", ".."))
    env = dict(os.environ)
    music_dir = os.path.join(repo_root, "dist", "sd", "music")
    if os.path.isdir(music_dir):
        env["NEXO_MUSIC_DIR"] = music_dir

    proc = subprocess.Popen([binpath], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, bufsize=0, cwd=repo_root, env=env)

    # The emulator stub prints "[beep ...]" to stderr whenever the firmware plays
    # a tone; forward those to the Tk bell so timer alarms are audible.
    beeps = SimpleQueue()

    def pump_beeps():
        for line in proc.stderr:
            if b"beep" in line:
                beeps.put(True)

    threading.Thread(target=pump_beeps, daemon=True).start()

    root = tk.Tk()
    root.title("Nexo Cardputer emulator  -  240x135")

    canvas = tk.Canvas(root, width=W * SCALE, height=H * SCALE, highlightthickness=0,
                       bg="#0c0c0e")
    canvas.pack()

    legend = tk.Label(
        root,
        text="Arrows/Enter/Esc type like the device  -  WASD + ; . , / also navigate  -  type letters/digits  -  1-6 jump",
        font=("Menlo", 10), fg="#cccccc", bg="#0c0c0e",
    )
    legend.pack(fill="x")

    def send(cmd):
        proc.stdin.write((cmd + "\n").encode())
        proc.stdin.flush()
        return read_frame(proc.stdout)

    def render(ppm):
        img = Image.open(__import__("io").BytesIO(ppm)).convert("RGB")
        tkimg = ImageTk.PhotoImage(img.resize((W * SCALE, H * SCALE), Image.NEAREST))
        canvas.delete("all")
        canvas.create_image(0, 0, anchor="nw", image=tkimg)
        canvas.image = tkimg  # keep a reference

    def on_key(event):
        ks = event.keysym
        try:
            if ks in NAMED:
                render(send("press " + NAMED[ks]))
            elif event.char and len(event.char) == 1 and event.char.isprintable():
                render(send("char " + event.char))
        except EOFError:
            root.destroy()

    def tick():
        try:
            render(send("tick"))
        except EOFError:
            root.destroy()

    def drain_beeps():
        try:
            while True:
                beeps.get_nowait()
                root.bell()
        except Exception:
            pass
        root.after(120, drain_beeps)

    root.after(120, drain_beeps)

    def on_close():
        try:
            send("quit")
        except Exception:
            pass
        proc.terminate()
        root.destroy()

    root.bind("<Key>", on_key)
    root.protocol("WM_DELETE_WINDOW", on_close)

    # First frame is emitted by the core at startup (splash).
    render(read_frame(proc.stdout))
    root.after(150, tick)
    root.mainloop()
    # Stop any track still playing through the host player.
    subprocess.run(["pkill", "-x", "afplay"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return 0


if __name__ == "__main__":
    sys.exit(main())
