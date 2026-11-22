# Granny UA Host (R36S / ARM64)

This fork builds a Granny-specific ARM64 `ua_host64` from the PortMaster Unity runtime. The host source is separate from the APK and game data.

## Granny input profile

- R3 toggles the visible mouse pointer on and off.
- Right stick moves the pointer.
- A performs a left click while pointer mode is active.
- The host draws the cursor over the Unity frame and restores the GL state it changes.
- The host also reads the `Granny Dual Touchscreen` evdev device and forwards its two multitouch slots as Android MotionEvents.
- The existing Granny mapper and the host use R3 together: normal mode forwards dual touch; pointer mode stops mapper touch and uses the drawn mouse cursor.

## Build and tests

GitHub Actions builds the ARM64 runtime and publishes `granny-ua-host64-arm64` as an artifact. CI tests the dual-touch event sequence and the upstream cursor rendering / GL-state preservation tests before packaging.

No APK, OBB, or proprietary Granny game files are included in this repository or build artifact.

## Compatibility note

Granny 1.7.9 is Unity 2020.1.4f1, IL2CPP, arm64-v8a, GLES2/GLES3. The APK was successfully inspected by `pmunity apk --target arm64 --dry-run`. The actual host launch and input behavior still require testing on the R36S.
