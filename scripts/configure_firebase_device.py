# ภาพรวม: สร้างค่าบอร์ดและ JSON จับคู่ในเครื่อง ไม่สร้างบัญชีหรือแก้ Firebase ออนไลน์
"""Configure Firebase Spark locally. Does not create cloud accounts or enable billing."""
import argparse
import getpass
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# ตรวจ URL/บัญชี โดยเจ้าของและ ESP32 ต้องมี UID คนละค่า
def validate_config(config):
    fields = ("databaseURL", "apiKey", "deviceEmail", "devicePassword", "deviceUid", "ownerUid", "deviceId")
    if any(not isinstance(config.get(k), str) or not config[k].strip() for k in fields):
        raise ValueError("Missing Spark settings; legacy Functions credentials are not supported.")
    config = dict(config)
    config["databaseURL"] = config["databaseURL"].rstrip("/")
    if not re.fullmatch(r"https://[a-z0-9-]+(?:\.[a-z0-9-]+)?\.(?:firebasedatabase\.app|firebaseio\.com)", config["databaseURL"]):
        raise ValueError("Use the HTTPS Realtime Database URL, without paths or query parameters.")
    if not re.fullmatch(r"[A-Za-z0-9_-]{20,100}", config["apiKey"]):
        raise ValueError("Invalid Firebase Web API key.")
    if not re.fullmatch(r"sl-[a-f0-9]{6}", config["deviceId"]):
        raise ValueError("Device ID must match Serial Monitor, e.g. sl-4824d3.")
    if any(not re.fullmatch(r"[A-Za-z0-9_-]{1,128}", config[k]) for k in ("deviceUid", "ownerUid")):
        raise ValueError("Copy each UID from Firebase Authentication.")
    if config["ownerUid"] == config["deviceUid"]:
        raise ValueError("Use separate Auth accounts for the owner and ESP32.")
    if "@" not in config["deviceEmail"] or len(config["devicePassword"]) < 6:
        raise ValueError("Enter the device Auth email and password (at least 6 characters).")
    return config

# สร้าง C++ define พร้อม escape ตัวอักษรในข้อความ
def render_header(config):
    names = {"databaseURL": "FIREBASE_DATABASE_URL", "apiKey": "FIREBASE_API_KEY",
             "deviceEmail": "FIREBASE_DEVICE_EMAIL", "devicePassword": "FIREBASE_DEVICE_PASSWORD",
             "deviceUid": "FIREBASE_DEVICE_UID"}
    return "#pragma once\n// Firebase Spark device account; keep this file private.\n" + "".join(
        "#define " + macro + " " + json.dumps(config[key], ensure_ascii=False) + "\n"
        for key, macro in names.items())

# รับค่าจากไฟล์หรือ terminal แล้วสร้าง header/JSON สำหรับนำเข้า Console
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", nargs="?", type=Path, help="Optional private Spark JSON settings")
    parser.add_argument("--replace", action="store_true", help="Replace an existing firebase_secrets.h")
    args = parser.parse_args()
    if args.config:
        config = json.loads(args.config.read_text(encoding="utf-8-sig"))
    else:
        config = {
            "databaseURL": input("Realtime Database HTTPS URL: ").strip(),
            "apiKey": input("Firebase Web API key: ").strip(),
            "ownerUid": input("Owner Auth UID: ").strip(),
            "deviceUid": input("ESP32 Auth UID (different account): ").strip(),
            "deviceEmail": input("ESP32 Auth email: ").strip(),
            "devicePassword": getpass.getpass("ESP32 Auth password (hidden): "),
            "deviceId": input("Device ID from Serial [sl-4824d3]: ").strip() or "sl-4824d3",
        }
    try:
        config = validate_config(config)
    except ValueError as error:
        parser.error(str(error))
    target = ROOT / "include" / "firebase_secrets.h"
    if target.exists() and not args.replace:
        parser.error("firebase_secrets.h exists. Use --replace only when replacing its settings.")
    device_id = config["deviceId"]
    private = ROOT / "firebase" / ".private"
    private.mkdir(parents=True, exist_ok=True)
    access = {"ownerUid": config["ownerUid"], "deviceUid": config["deviceUid"]}
    device = {"name": "NimbusRail"}
    pairing = {"access": {device_id: access}, "users": {config["ownerUid"]: {"devices": {device_id: device}}}}
    for name, content in [(f"access-{device_id}.json", access), (f"owner-{device_id}.json", device),
                          (f"pairing-{device_id}.json", pairing)]:
        (private / name).write_text(json.dumps(content, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    with target.open("w" if args.replace else "x", encoding="utf-8") as out:
        out.write(render_header(config))
    print("Created include/firebase_secrets.h (password not printed).")
    print("Pairing JSON:", private / f"pairing-{device_id}.json")
    print("Import the pairing JSON at database root ONLY for a new, empty database.")
    print("For an existing database, import the access/owner files at the exact paths in the setup guide.")

if __name__ == "__main__":
    main()
