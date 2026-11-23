"""PlatformIO post-upload: avoid extra esptool 'run' (RTS leaves ESP32-S3 in USB download mode)."""
Import("env")

def _after_upload(source, target, env):
    print(
        "Side B upload complete. If serial shows ESP-ROM, tap Reset (Side B) once "
        "(Boot not held), then run MAS/scripts/side-b-finish-recovery.ps1"
    )

env.AddPostAction("upload", _after_upload)
