Meshtastic firmware file for dual boot
======================================

  data/meshtastic.bin  -- used by Nocturne menu "Meshtastic -> Switch to Meshtastic"

AUTOMATIC: When you run "Upload Filesystem Image" (or "buildfs"), the build script
builds Meshtastic from firmware-2.7.15.567b8ea (env heltec-v4) and copies the
result here. No manual copy needed if that folder is present.

Manual: Run once  pio run -t meshtastic  to build and copy, then uploadfs.
If firmware-2.7.15.567b8ea is missing, add a Heltec V4 .bin here and rename to
meshtastic.bin (must fit ota_1, 2 MB). Do not commit the .bin (see .gitignore).
