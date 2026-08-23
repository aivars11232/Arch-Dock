#!/usr/bin/env python3

import json
import os
import re
import sys
import time

from PySide6.QtCore import ClassInfo, QCoreApplication, QObject, QTimer, Slot
from PySide6.QtDBus import QDBusConnection, QDBusMessage


SERVICE_NAME = "org.archdock.VisibilityProbeReceiver"
OBJECT_PATH = "/org/archdock/VisibilityProbeReceiver"
INTERFACE_NAME = "org.archdock.VisibilityProbeReceiver"
NONCE_PATTERN = re.compile(r"^[0-9a-f]{32}$")
STAGE_PATTERN = re.compile(r"^[a-z][a-z0-9-]{0,31}$")
MAX_PAYLOAD_SIZE = 65536


@ClassInfo(**{"D-Bus Interface": INTERFACE_NAME})
class ProbeReceiver(QObject):
    def __init__(self, application):
        QObject.__init__(self)
        self._application = application
        self._records = {}

    @staticmethod
    def _key(nonce, stage):
        return f"{nonce}:{stage}"

    @staticmethod
    def _valid_request(nonce, stage):
        return bool(NONCE_PATTERN.fullmatch(nonce)) and bool(
            STAGE_PATTERN.fullmatch(stage)
        )

    @staticmethod
    def _sender(message):
        return message.service()

    @staticmethod
    def _log(event, **details):
        print(
            json.dumps(
                {"event": event, **details},
                ensure_ascii=True,
                separators=(",", ":"),
                sort_keys=True,
            ),
            flush=True,
        )

    @Slot(str, str, str, QDBusMessage, result=str)
    def Report(self, nonce, stage, payload, message):
        sender = self._sender(message)
        key = self._key(nonce, stage)
        if (
            not self._valid_request(nonce, stage)
            or not sender.startswith(":")
            or len(payload.encode("utf-8")) > MAX_PAYLOAD_SIZE
            or key in self._records
        ):
            self._log(
                "report-rejected",
                nonce=nonce,
                stage=stage,
                sender=sender,
            )
            return ""

        self._records[key] = {
            "confirmed": False,
            "confirmMonotonicNs": 0,
            "confirmWallTimeNs": 0,
            "nonce": nonce,
            "payload": payload,
            "reportMonotonicNs": time.monotonic_ns(),
            "reportWallTimeNs": time.time_ns(),
            "sender": sender,
            "stage": stage,
        }
        self._log("reported", nonce=nonce, stage=stage, sender=sender)
        return nonce

    @Slot(str, str, str, QDBusMessage, result=bool)
    def Confirm(self, nonce, stage, echoed_nonce, message):
        sender = self._sender(message)
        key = self._key(nonce, stage)
        record = self._records.get(key)
        if (
            record is None
            or sender != record["sender"]
            or echoed_nonce != nonce
            or record["confirmed"]
        ):
            self._log(
                "confirm-rejected",
                nonce=nonce,
                stage=stage,
                sender=sender,
            )
            return False

        record["confirmed"] = True
        record["confirmMonotonicNs"] = time.monotonic_ns()
        record["confirmWallTimeNs"] = time.time_ns()
        self._log("confirmed", nonce=nonce, stage=stage, sender=sender)
        return True

    @Slot(str, str, result=str)
    def Read(self, nonce, stage):
        if not self._valid_request(nonce, stage):
            return ""
        record = self._records.get(self._key(nonce, stage))
        if record is None:
            return ""
        return json.dumps(
            record,
            ensure_ascii=True,
            separators=(",", ":"),
            sort_keys=True,
        )

    @Slot(str, str, result=bool)
    def Reset(self, nonce, stage):
        if not self._valid_request(nonce, stage):
            return False
        return self._records.pop(self._key(nonce, stage), None) is not None

    @Slot(QDBusMessage, result=bool)
    def Shutdown(self, message):
        self._log("shutdown-requested", sender=self._sender(message))
        QTimer.singleShot(0, self._application.quit)
        return True


def main():
    if (
        os.environ.get("ARCHDOCK_PLASMA_LIFECYCLE_SESSION") != "1"
        or os.environ.get("XDG_CURRENT_DESKTOP") != "archdock-test"
    ):
        raise SystemExit(
            "kwin-probe-receiver.py is restricted to the private Arch Dock lifecycle session"
        )

    application = QCoreApplication(sys.argv)
    application.setApplicationName("Arch Dock KWin Probe Receiver")
    connection = QDBusConnection.sessionBus()
    if not connection.isConnected():
        raise SystemExit(
            f"could not connect to the private session bus: {connection.lastError().message()}"
        )

    receiver = ProbeReceiver(application)
    if not connection.registerObject(
        OBJECT_PATH,
        receiver,
        QDBusConnection.RegisterOption.ExportAllSlots,
    ):
        raise SystemExit(
            f"could not register {OBJECT_PATH}: {connection.lastError().message()}"
        )
    if not connection.registerService(SERVICE_NAME):
        connection.unregisterObject(OBJECT_PATH)
        raise SystemExit(
            f"could not register {SERVICE_NAME}: {connection.lastError().message()}"
        )

    ProbeReceiver._log("ready", path=OBJECT_PATH, service=SERVICE_NAME)
    try:
        return application.exec()
    finally:
        connection.unregisterService(SERVICE_NAME)
        connection.unregisterObject(OBJECT_PATH)


if __name__ == "__main__":
    raise SystemExit(main())
