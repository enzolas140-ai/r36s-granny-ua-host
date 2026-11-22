# Granny UA Host (R36S / ARM64)

This fork builds a dedicated ARM64 `ua_host64` for the Granny Android Unity port. It is based on the PortMaster Unity runtime and keeps the host source separate from the game APK and game data.

## Granny pointer profile

The packaged `granny-pointer.env` configures the host as follows:

- R3 toggles pointer mode on/off.
- Right stick moves the pointer.
- A performs a left click while pointer mode is on.
- The host draws the cursor over the Unity frame; `UA_POINTER_HIDE=0` keeps it visible.
- When pointer mode is off, the pad returns to normal game input.

The host's pointer implementation draws the cursor in the GLES swap path and restores the GL state it changes. The CI workflow runs the upstream pointer tests, including cursor pixels and GL-state preservation, before uploading the binary.

## Build

GitHub Actions builds the ARM64 runtime and publishes `granny-ua-host64-arm64` as an artifact. No APK, OBB, or proprietary Granny game files are included in this repository or build artifact.

## Compatibility note

The binary is an ARM64 Linux host for Unity Android IL2CPP players. Granny 1.7.9 uses Unity 2020.1.4f1 and arm64-v8a. The actual game launch and pointer behavior still need verification on the R36S; passing the host's pointer tests does not by itself prove Granny's UI/gameplay accepts mouse events.
