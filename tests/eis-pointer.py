#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Moves the pointer of a private test KWin through KWin 6's own emulated
input (EIS, org.kde.KWin.EIS.RemoteDesktop over the session bus), the way
tests/visibility-window.py drives its interaction matrix: no input device and
no ambient socket is opened.

Usage: eis-pointer.py X Y [X Y ...]
       eis-pointer.py push X Y DX DY [TIMES]
       eis-pointer.py click X Y [left|right]
The first moves the pointer to each point in turn, waiting for KWin to take
each move. The second puts it at X Y and pushes it by DX DY again and again
(five times unless told), a tenth of a second apart, as a hand pushes a mouse
against a screen edge. The third moves it to X Y and clicks there with the
left button, or the right one.
ARCHDOCK_PRIVATE_KWIN_PID must name the private KWin the test started; the
pointer is moved only when that KWin's runtime folder, session bus and
Wayland display are this process's own, so a session outside the test is
never touched.
"""
import ctypes as c
import os
import pathlib
import sys
import time

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib


def private_kwin_or_exit():
    pid = os.environ.get("ARCHDOCK_PRIVATE_KWIN_PID", "")
    if not pid.isdigit():
        sys.exit("eis-pointer: ARCHDOCK_PRIVATE_KWIN_PID names no process")
    proc = pathlib.Path("/proc") / pid
    if pathlib.Path(os.readlink(proc / "exe")).name != "kwin_wayland":
        sys.exit("eis-pointer: that process is not KWin")
    environment = dict(part.split(b"=", 1) for part in (proc / "environ").read_bytes().split(b"\0") if b"=" in part)
    for key in ("XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS", "WAYLAND_DISPLAY"):
        if environment.get(key.encode()) != os.environ.get(key, "").encode():
            sys.exit("eis-pointer: that KWin is not this test's own (%s differs)" % key)


def main(arguments):
    push = None
    button = None
    if arguments and arguments[0] == "push":
        if len(arguments) not in (5, 6):
            sys.exit(__doc__)
        push = [float(value) for value in arguments[1:5]] + [int(arguments[5]) if len(arguments) == 6 else 5]
        points = [(push[0], push[1])]
    elif arguments and arguments[0] == "click":
        if len(arguments) not in (3, 4) or (len(arguments) == 4 and arguments[3] not in ("left", "right")):
            sys.exit(__doc__)
        # The Linux input codes of the left and right buttons (BTN_LEFT, BTN_RIGHT).
        button = 0x111 if len(arguments) == 4 and arguments[3] == "right" else 0x110
        points = [(float(arguments[1]), float(arguments[2]))]
    else:
        if len(arguments) < 2 or len(arguments) % 2:
            sys.exit(__doc__)
        points = [(float(arguments[index]), float(arguments[index + 1])) for index in range(0, len(arguments), 2)]
    private_kwin_or_exit()

    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    reply, descriptors = bus.call_with_unix_fd_list_sync(
        "org.kde.KWin", "/org/kde/KWin/EIS/RemoteDesktop", "org.kde.KWin.EIS.RemoteDesktop", "connectToEIS",
        GLib.Variant("(i)", (3,)), None, Gio.DBusCallFlags.NONE, 5000, None, None)
    handle, _cookie = reply.unpack()
    lib = c.CDLL("libei.so.1")
    pointer = c.c_void_p
    bindings = {
        "new_sender": (pointer, [pointer]), "configure_name": (None, [pointer, c.c_char_p]),
        "setup_backend_fd": (c.c_int, [pointer, c.c_int]), "dispatch": (None, [pointer]),
        "get_event": (pointer, [pointer]), "event_get_type": (c.c_int, [pointer]),
        "event_get_seat": (pointer, [pointer]), "event_get_device": (pointer, [pointer]),
        "event_unref": (pointer, [pointer]), "seat_bind_capabilities": (None, [pointer]),
        "device_has_capability": (c.c_bool, [pointer, c.c_int]),
        "device_ref": (pointer, [pointer]), "device_start_emulating": (None, [pointer, c.c_uint32]),
        "device_pointer_motion_absolute": (None, [pointer, c.c_double, c.c_double]),
        "device_pointer_motion": (None, [pointer, c.c_double, c.c_double]),
        "device_button_button": (None, [pointer, c.c_uint32, c.c_bool]),
        "device_frame": (None, [pointer, c.c_uint64]), "now": (c.c_uint64, [pointer]),
        "new_ping": (pointer, [pointer]), "ping": (None, [pointer]), "ping_unref": (pointer, [pointer]),
    }
    for name, (result, argument_types) in bindings.items():
        function = getattr(lib, "ei_" + name)
        function.restype, function.argtypes = result, argument_types
    context = lib.ei_new_sender(None)
    lib.ei_configure_name(context, b"Arch Dock bar test")
    if lib.ei_setup_backend_fd(context, descriptors.get(handle)) != 0:
        sys.exit("eis-pointer: KWin's emulated input could not be reached")

    devices = {}
    pongs = 0

    def pump():
        nonlocal pongs
        lib.ei_dispatch(context)
        while event := lib.ei_get_event(context):
            kind = lib.ei_event_get_type(event)
            if kind == 2:
                sys.exit("eis-pointer: KWin disconnected")
            if kind == 3:
                # Relative and absolute pointer, keyboard, scroll and buttons.
                lib.ei_seat_bind_capabilities(lib.ei_event_get_seat(event), 1, 2, 4, 16, 32, pointer())
            if kind == 8:
                device = lib.ei_event_get_device(event)
                started = False
                # Relative pointer, absolute pointer, buttons.
                for capability in (1, 2, 32):
                    if lib.ei_device_has_capability(device, capability) and capability not in devices:
                        devices[capability] = lib.ei_device_ref(device)
                        started = True
                if started:
                    lib.ei_device_start_emulating(device, 1)
            if kind == 90:
                pongs += 1
            lib.ei_event_unref(event)

    def wait_for(predicate, what):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            pump()
            if predicate():
                return
            time.sleep(0.02)
        sys.exit("eis-pointer: no " + what)

    wait_for(lambda: 2 in devices, "absolute pointer from KWin")

    def taken():
        target = pongs + 1
        ping = lib.ei_new_ping(context)
        lib.ei_ping(ping)
        lib.ei_ping_unref(ping)
        wait_for(lambda: pongs >= target, "acknowledgement of the move")

    for x, y in points:
        lib.ei_device_pointer_motion_absolute(devices[2], x, y)
        lib.ei_device_frame(devices[2], lib.ei_now(context))
        taken()
        time.sleep(0.05)
    if button is not None:
        wait_for(lambda: 32 in devices, "buttons from KWin")
        for pressed in (True, False):
            lib.ei_device_button_button(devices[32], button, pressed)
            lib.ei_device_frame(devices[32], lib.ei_now(context))
            taken()
            time.sleep(0.05)
    if push:
        wait_for(lambda: 1 in devices, "relative pointer from KWin")
        for _ in range(push[4]):
            time.sleep(0.1)
            lib.ei_device_pointer_motion(devices[1], push[2], push[3])
            lib.ei_device_frame(devices[1], lib.ei_now(context))
            taken()


if __name__ == "__main__":
    main(sys.argv[1:])
