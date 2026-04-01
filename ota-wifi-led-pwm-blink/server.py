from flask import Flask, jsonify, request, send_from_directory, abort
from pathlib import Path
import json
import re

app = Flask(__name__)

BASE_DIR = Path(__file__).resolve().parent
ARTIFACTS_DIR = BASE_DIR / "artifacts"
STATE_FILE = BASE_DIR / "ota_state.json"

ARTIFACTS_DIR.mkdir(exist_ok=True)

VERSION_RE = re.compile(r"_v(\d+(?:\.\d+)*)\.bin$", re.IGNORECASE)


def load_state() -> dict:
    if not STATE_FILE.exists():
        return {
            "target_version": None,
            "firmware_file": None
        }

    try:
        return json.loads(STATE_FILE.read_text(encoding="utf-8"))
    except Exception:
        return {
            "target_version": None,
            "firmware_file": None
        }


def save_state(state: dict) -> None:
    STATE_FILE.write_text(
        json.dumps(state, indent=2, ensure_ascii=False),
        encoding="utf-8"
    )


def parse_version_from_filename(filename: str):
    """
    Extracts version tuple from names like:
    esp32s3-led-pwm-blink_v1.0.3.bin
    Returns:
        ("1.0.3", (1, 0, 3))
    or None if no version found.
    """
    match = VERSION_RE.search(filename)
    if not match:
        return None

    version_str = match.group(1)
    version_tuple = tuple(int(part) for part in version_str.split("."))
    return version_str, version_tuple


def scan_artifacts():
    """
    Finds the newest firmware by version number, not by file date.
    Returns dict like:
    {
        "target_version": "1.0.3",
        "firmware_file": "esp32s3-led-pwm-blink_v1.0.3.bin"
    }
    or None if nothing valid found.
    """
    best = None

    for path in ARTIFACTS_DIR.iterdir():
        if not path.is_file() or path.suffix.lower() != ".bin":
            continue

        parsed = parse_version_from_filename(path.name)
        if not parsed:
            continue

        version_str, version_tuple = parsed

        if best is None or version_tuple > best["version_tuple"]:
            best = {
                "target_version": version_str,
                "firmware_file": path.name,
                "version_tuple": version_tuple
            }

    if best is None:
        return None

    return {
        "target_version": best["target_version"],
        "firmware_file": best["firmware_file"]
    }


def refresh_state_if_newer():
    """
    Scans artifacts and updates ota_state.json only if a newer version exists.
    Returns:
        (state, updated_bool, message)
    """
    current_state = load_state()
    scanned = scan_artifacts()

    if scanned is None:
        return current_state, False, "No valid .bin firmware files found"

    current_version = current_state.get("target_version")
    current_tuple = None

    if current_version:
        current_tuple = tuple(int(part) for part in current_version.split("."))

    scanned_tuple = tuple(int(part) for part in scanned["target_version"].split("."))

    if current_tuple is None or scanned_tuple > current_tuple:
        save_state(scanned)
        return scanned, True, "State updated to newer firmware"

    return current_state, False, "Already on newest firmware"


@app.get("/")
def index():
    files = sorted(
        [p.name for p in ARTIFACTS_DIR.iterdir() if p.is_file()],
        reverse=True
    )

    return jsonify({
        "message": "Local OTA server is running",
        "files": files,
        "state": load_state()
    })


@app.get("/fw/<path:filename>")
def get_firmware(filename: str):
    file_path = ARTIFACTS_DIR / filename
    if not file_path.exists() or not file_path.is_file():
        abort(404, description="Firmware file not found")

    return send_from_directory(ARTIFACTS_DIR, filename, as_attachment=False)


@app.get("/api/latest")
def get_latest():
    # auto-refresh every time ESP asks
    state, updated, message = refresh_state_if_newer()

    firmware_file = state.get("firmware_file")
    firmware_url = None

    if firmware_file:
        firmware_url = f"{request.host_url.rstrip('/')}/fw/{firmware_file}"

    return jsonify({
        "message": message,
        "updated": updated,
        "target_version": state.get("target_version"),
        "firmware_file": firmware_file,
        "firmware_url": firmware_url
    })


@app.post("/api/refresh")
def refresh():
    state, updated, message = refresh_state_if_newer()

    firmware_file = state.get("firmware_file")
    firmware_url = None

    if firmware_file:
        firmware_url = f"{request.host_url.rstrip('/')}/fw/{firmware_file}"

    return jsonify({
        "message": message,
        "updated": updated,
        "target_version": state.get("target_version"),
        "firmware_file": firmware_file,
        "firmware_url": firmware_url
    })


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000, debug=True)