#!/usr/bin/env python3
"""
Screenshot a CoCo program running under xroar, without a window appearing on
the developer's screen.

Starts a private Xvfb, runs xroar on it, autotypes a RUNM at the BASIC prompt,
then grabs the X display. Works for both CoCo 1/2 and CoCo 3 - unlike reading
the framebuffer over the gdb target, which cannot see the CoCo 3 screen (it
lives in MMU blocks outside the 64K cpu map) and which does not work anyway,
because xroar's gdb target holds the machine halted.

  ./support/xroar-shot.py --disk r2r/coco/fujirkle-demo.dsk --run FUJIRKL1 \\
                          --out /tmp/coco2.png

  ./support/xroar-shot.py --disk r2r/coco/fujirkle-demo.dsk --run FUJIRKL3 \\
                          --machine coco3 --out /tmp/coco3.png --keys ' '

--keys sends extra keystrokes after the program starts, one per --key-delay
seconds, which is how you step a demo on to its next screen.
"""

import argparse
import os
import random
import signal
import shutil
import subprocess
import sys
import time


def wait_for_display(display, timeout=10.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        probe = subprocess.run(["xdpyinfo", "-display", display],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if probe.returncode == 0:
            return True
        time.sleep(0.2)
    return False


def send_keys(display, keys, delay):
    """Type into the xroar window with xdotool, if it is available."""
    if not shutil.which("xdotool"):
        return False
    for ch in keys:
        subprocess.run(["xdotool", "search", "--name", "XRoar", "key", "--clearmodifiers",
                        "space" if ch == " " else ch],
                       env=dict(os.environ, DISPLAY=display),
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(delay)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--disk", required=True)
    ap.add_argument("--run", required=True, help="binary name, e.g. FUJIRKL1")
    ap.add_argument("--out", required=True)
    ap.add_argument("--machine", default="coco2b")
    ap.add_argument("--wait", type=float, default=10.0,
                    help="seconds to let the program boot and draw")
    ap.add_argument("--keys", default="",
                    help="keystrokes to send after it starts, to advance screens")
    ap.add_argument("--key-delay", type=float, default=2.0)
    args = ap.parse_args()

    from PIL import ImageGrab

    display = ":%d" % random.randint(90, 120)
    xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", "1024x768x24"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                            preexec_fn=os.setsid)
    xroar = None
    try:
        if not wait_for_display(display):
            print("Xvfb did not come up on %s" % display, file=sys.stderr)
            return 1

        env = dict(os.environ, DISPLAY=display)
        env.pop("LD_LIBRARY_PATH", None)      # a snap LD_LIBRARY_PATH breaks xroar
        env.pop("WAYLAND_DISPLAY", None)

        cmd = ["xroar", "-machine", args.machine, "-ao", "null",
               "-load-fd0", os.path.abspath(args.disk),
               "-type", '\\rRUNM"%s"\\r' % args.run]

        xroar = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 env=env, preexec_fn=os.setsid)
        time.sleep(args.wait)

        if xroar.poll() is not None:
            print("xroar exited early:\n"
                  + xroar.stdout.read().decode(errors="replace"), file=sys.stderr)
            return 1

        if args.keys:
            if not send_keys(display, args.keys, args.key_delay):
                print("note: xdotool not installed, --keys ignored", file=sys.stderr)

        img = ImageGrab.grab(xdisplay=display)
        img.save(args.out)
        print("wrote %s (%dx%d)" % (args.out, img.width, img.height))
        return 0
    finally:
        for proc in (xroar, xvfb):
            if proc is None:
                continue
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except (ProcessLookupError, PermissionError):
                pass


if __name__ == "__main__":
    sys.exit(main())
