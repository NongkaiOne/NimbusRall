# Legacy Blaze backend

This directory contains the previous Cloud Functions implementation for reference.
The active app and ESP32 firmware now use Firebase Spark with direct Realtime Database access.
firebase/firebase.json does not deploy these Functions. Do not run the old provision script.
Use docs/FIREBASE_SETUP.md and firebase/scripts/setup-spark.cjs instead.
