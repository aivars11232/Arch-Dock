#!/usr/bin/env python3

import pathlib
import sys

from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QGuiApplication, QWindow


if len(sys.argv) != 3:
    raise SystemExit("usage: visibility-window.py COMMAND_FILE SCREEN_INDEX")

command_file = pathlib.Path(sys.argv[1])
screen_index = int(sys.argv[2])

application = QGuiApplication(sys.argv)
application.setApplicationName("Arch Dock Visibility Fixture")
application.setDesktopFileName("org.archdock.visibilityfixture")

screens = application.screens()
if screen_index < 0 or screen_index >= len(screens):
    raise SystemExit(f"screen index {screen_index} is unavailable")

screen = screens[screen_index]
window = QWindow()
window.setScreen(screen)
window.setTitle("Arch Dock Visibility Fixture")
window.setFlags(Qt.WindowType.Window | Qt.WindowType.FramelessWindowHint)

last_serial = ""


def normal_geometry():
    geometry = screen.geometry()
    width = min(520, max(240, geometry.width() // 2))
    height = min(320, max(180, geometry.height() // 2))
    return geometry.x() + 32, geometry.y() + 32, width, height


def overlap_geometry():
    geometry = screen.geometry()
    width = min(700, max(360, geometry.width() - 160))
    height = min(240, max(160, geometry.height() // 3))
    return (
        geometry.x() + max(40, (geometry.width() - width) // 2),
        geometry.y() + geometry.height() - height,
        width,
        height,
    )


def activate():
    window.raise_()
    window.requestActivate()


def apply_command(serial, command):
    if command == "quit":
        print(f"STATE:{serial}:quit", flush=True)
        application.quit()
        return

    if command == "normal":
        window.showNormal()
        window.setGeometry(*normal_geometry())
    elif command == "overlap":
        window.showNormal()
        window.setGeometry(*overlap_geometry())
    elif command == "maximized":
        window.showMaximized()
    elif command == "fullscreen":
        window.showFullScreen()
    else:
        raise RuntimeError(f"unknown fixture command: {command}")

    QTimer.singleShot(50, activate)
    print(f"STATE:{serial}:{command}", flush=True)


def poll_command():
    global last_serial

    try:
        contents = command_file.read_text(encoding="utf-8").strip()
    except FileNotFoundError:
        return
    if not contents:
        return

    parts = contents.split(maxsplit=1)
    if len(parts) != 2 or parts[0] == last_serial:
        return

    last_serial = parts[0]
    try:
        apply_command(parts[0], parts[1])
    except Exception as error:  # The harness treats any fixture exit as a failure.
        print(f"ERROR:{parts[0]}:{error}", flush=True)
        application.exit(1)


timer = QTimer()
timer.setInterval(50)
timer.timeout.connect(poll_command)
timer.start()
poll_command()

raise SystemExit(application.exec())
