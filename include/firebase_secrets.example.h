#pragma once
// Copy to firebase_secrets.h (gitignored). Generate a per-device secret with
// firebase/functions/scripts/provision.js; never use a service-account key on ESP32.
#define FIREBASE_SYNC_URL "https://asia-southeast1-YOUR_PROJECT.cloudfunctions.net/deviceSync"
#define FIREBASE_DEVICE_SECRET ""
