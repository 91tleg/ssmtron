"""BLE DFU end-to-end: pairing, image upload, and the app's own
hal_dfu_confirm() auto-confirming the new image (see main.c's
CONFIRM_AFTER_MS). Nothing here substitutes for that: a host test can
fake hal_dfu_confirm's return value, but only real MCUboot + a real BLE
central can prove the whole chain actually works.

Requires:
  mcumgr CLI on PATH (https://github.com/apache/mynewt-mcumgr-cli)
  HIL_DFU_IMAGE   path to a signed app image (app.signed.bin from a west
                  build of firmware/app; any valid signed image works,
                  it does not need a bumped version)
  The DUT already bonded to this host over BLE (pairing is a one-time
  manual step outside this script: mcumgr's authenticated passkey
  pairing needs an interactive accept the first time).

Caveat: the `mcumgr image list` text parsing below follows the CLI's
documented command usage, not a verified transcript from a specific
mcumgr build -- if this fails to parse, run the command by hand once and
adjust _parse_image_list to match what your installed version prints.
"""

import os
import re
import subprocess
import time

import pytest

CONFIRM_TIMEOUT_S = 20.0  # main.c's CONFIRM_AFTER_MS (10 s) plus margin
REBOOT_TIMEOUT_S = 15.0


def _mcumgr(connstring, *args):
    cmd = ["mcumgr", "--conntype", "ble", "--connstring", connstring, *args]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        raise RuntimeError(f"{cmd} failed: {result.stderr or result.stdout}")
    return result.stdout


def _parse_image_list(output):
    """Yields {"slot": int, "hash": str, "active": bool, "confirmed": bool}
    per image block in `mcumgr image list` output."""
    images = []
    for block in re.split(r"\n(?=\s*image=)", output):
        slot_m = re.search(r"slot=(\d+)", block)
        hash_m = re.search(r"hash:\s*([0-9a-fA-F]+)", block)
        flags_m = re.search(r"flags:\s*(.*)", block)
        if slot_m and hash_m:
            flags = flags_m.group(1) if flags_m else ""
            images.append(
                {
                    "slot": int(slot_m.group(1)),
                    "hash": hash_m.group(1),
                    "active": "active" in flags,
                    "confirmed": "confirmed" in flags,
                }
            )
    return images


@pytest.fixture
def dfu_image():
    path = os.environ.get("HIL_DFU_IMAGE")
    if not path or not os.path.isfile(path):
        pytest.skip("HIL_DFU_IMAGE not set to a valid signed image path")
    return path


def test_upload_and_auto_confirm(dut, ble_connstring, dfu_image):
    before = _parse_image_list(_mcumgr(ble_connstring, "image", "list"))

    _mcumgr(ble_connstring, "image", "upload", dfu_image)

    after_upload = _parse_image_list(_mcumgr(ble_connstring, "image", "list"))
    new_images = [
        img for img in after_upload if img["hash"] not in {i["hash"] for i in before}
    ]
    assert new_images, f"uploaded image not visible in image list: {after_upload}"
    new_hash = new_images[0]["hash"]

    _mcumgr(ble_connstring, "image", "test", new_hash)
    dut.drain()
    _mcumgr(ble_connstring, "reset")

    # The device drops the BLE link on reset; give it time to reboot,
    # re-advertise, and re-pair before mcumgr can reach it again.
    time.sleep(REBOOT_TIMEOUT_S)

    deadline = time.monotonic() + CONFIRM_TIMEOUT_S
    confirmed = False
    while time.monotonic() < deadline and not confirmed:
        images = _parse_image_list(_mcumgr(ble_connstring, "image", "list"))
        running = next((i for i in images if i["hash"] == new_hash), None)
        confirmed = bool(running and running["active"] and running["confirmed"])
        if not confirmed:
            time.sleep(1.0)

    assert confirmed, "new image never became active+confirmed after boot"
