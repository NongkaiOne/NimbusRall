"""Create the ignored device header from the locally provisioned private JSON."""
import argparse
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("config", type=Path, help="firebase/.private/sl-xxxxxx.json")
parser.add_argument("--replace", action="store_true", help="replace an existing device header")
args = parser.parse_args()
config = json.loads(args.config.read_text(encoding="utf-8"))
endpoint = config.get("endpoint", "")
secret = config.get("deviceSecret", "")
if not re.fullmatch(r"https://asia-southeast1-[a-z0-9-]+\.cloudfunctions\.net/deviceSync", endpoint):
    parser.error("Invalid HTTPS endpoint")
if not re.fullmatch(r"[a-f0-9]{64}", secret):
    parser.error("Invalid device credential")
if not re.fullmatch(r"sl-[a-f0-9]{6}", config.get("deviceId", "")):
    parser.error("Invalid device ID")
target = Path(__file__).resolve().parents[1] / "include" / "firebase_secrets.h"
with target.open("w" if args.replace else "x", encoding="utf-8") as out:
    out.write("#pragma once\n")
    out.write("#define FIREBASE_SYNC_URL " + json.dumps(endpoint) + "\n")
    out.write("#define FIREBASE_DEVICE_SECRET " + json.dumps(secret) + "\n")
print("Created include/firebase_secrets.h for", config["deviceId"], "(credential not printed)")
