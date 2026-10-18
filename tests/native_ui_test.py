#!/usr/bin/env python3
"""Optional native SDL integration checks; requires DISPLAY and xdotool.
Run: xvfb-run -a python3 tests/native_ui_test.py build/growlander
Use an isolated Xvfb display to avoid desktop focus interference.
"""
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import time

checks = 0

def check(value, label):
    global checks
    checks += 1
    if not value:
        raise AssertionError(label)
    print(f"PASS: {label}", flush=True)

def xd(*args):
    return subprocess.check_output(["xdotool", *map(str, args)], text=True).strip()

with tempfile.TemporaryDirectory(prefix="growlander-native-") as temp:
    save = Path(temp) / "world.glw"
    process = subprocess.Popen([str(Path(sys.argv[1]).resolve()), "--save", str(save)])
    try:
        window = ""
        for _ in range(100):
            try:
                window = xd("search", "--pid", process.pid, "--name", "Growlander").splitlines()[0]
                break
            except (subprocess.CalledProcessError, IndexError):
                time.sleep(.05)
        check(bool(window), "jendela SDL terbuka")
        # windowfocus works with or without a window manager under Xvfb.
        xd("windowfocus", "--sync", window)
        time.sleep(.2)
        def key(name):
            xd("key", name)
            time.sleep(.08)
        def state():
            key("F5")
            lines = save.read_text().splitlines()
            return {line.split()[0]: line.split()[1:] for line in lines if line and line[0] != "C"}, lines
        baseline, _ = state()
        x0, y0 = map(float, baseline["P"][:2])
        xd("keydown", "d"); time.sleep(.25); xd("keyup", "d")
        moved, _ = state()
        x1, y1 = map(float, moved["P"][:2])
        check(x1 > x0 + .5, "tombol D menggerakkan avatar")
        xd("keydown", "space"); time.sleep(.12); xd("keyup", "space")
        jumped, _ = state()
        check(float(jumped["P"][1]) < y1 - .3, "tombol Spasi melompat")
        time.sleep(1)
        key("i")
        paused, _ = state()
        xd("keydown", "d"); time.sleep(.3); xd("keyup", "d")
        still, _ = state()
        check(still["P"][:2] == paused["P"][:2] and still["S"][1] == paused["S"][1], "inventaris menjeda gerak dan waktu dunia")
        key("Escape")
        key("h")
        help1, _ = state(); time.sleep(.2); help2, _ = state()
        check(help1["S"][1] == help2["S"][1], "bantuan menjeda waktu dunia")
        key("Escape")
        xd("windowsize", window, 1024, 576)
        time.sleep(.2)
        key("6")
        current, _ = state()
        x, y = map(float, current["P"][:2])
        tx, ty = math.floor(x) + 3, 23
        cx = max(0, min(2720, x * 40 - 512))
        cy = max(0, min(1680, y * 40 - 381.6))
        mousex = round((tx * 40 + 20 - cx) * .8)
        mousey = round((ty * 40 + 20 - cy) * .8)
        xd("mousemove", "--window", window, mousex, mousey)
        xd("click", "3"); time.sleep(.3)
        planted, _ = state()
        check(int(planted["R"][2]) == 1 and int(planted["I"][5]) == 7, "hotbar 6 dan klik kanan tetap akurat setelah resize")
        print("Menunggu tanaman tumbuh 18 detik waktu nyata...", flush=True)
        time.sleep(18.3)
        xd("click", "1"); time.sleep(.3)
        harvested, _ = state()
        check(int(harvested["R"][3]) == 1 and int(harvested["D"][0]) > 0, "panen native menghasilkan drop")
        xd("keydown", "d"); time.sleep(.26); xd("keyup", "d")
        time.sleep(.4)
        harvested, _ = state()
        check(int(harvested["I"][3]) == 22 and int(harvested["I"][5]) == 8, "mendekat mengambil hasil panen native")
        saved_x = float(harvested["P"][0])
        xd("keydown", "d"); time.sleep(.25); xd("keyup", "d")
        key("F9")
        loaded, _ = state()
        check(abs(float(loaded["P"][0]) - saved_x) < .01, "F9 memulihkan posisi simpanan")
        key("Escape"); key("Return")
        check(process.wait(timeout=5) == 0, "menu Simpan & keluar menutup game dengan sukses")
        process = subprocess.Popen([str(Path(sys.argv[1]).resolve()), "--save", str(save)])
        time.sleep(.6)
        xd("search", "--pid", process.pid, "--name", "Growlander")
        # Quit via native window close; saved world must remain readable.
        process.terminate()
        process.wait(timeout=5)
        check(save.exists() and "GROWLANDER 2" in save.read_text(), "save tetap tersedia untuk sesi selanjutnya")
        print(f"{checks} native UI checks, 0 failures")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
