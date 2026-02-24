# JC4827W543 Animated GIF (Pin Driven)

This project plays GIFs from SD card based on the state of 3 GPIO pins on an ESP32-S3, with an optional startup GIF that plays once at boot and loops while no switch is active.

## Pin To GIF Mapping

Defined in `JC4827W543_AnimatedGIF_pins_driven.ino`:

- `VIDEO_PINS[] = {46, 9, 14}`
- `VIDEO_GIF_PATHS[] = {"/gif/alien_eye.gif", "/gif/bird.gif", "/gif/train.gif"}`
- `VIDEO_ACTIVE_LEVEL = LOW`

Mapping:

- Pin `46` -> `/gif/alien_eye.gif`
- Pin `9` -> `/gif/bird.gif`
- Pin `14` -> `/gif/train.gif`

## GIF File Requirements

- GIF files must exist on the SD card at the exact hardcoded paths.
- Current defaults:
  - `/gif/alien_eye.gif`
  - `/gif/bird.gif`
  - `/gif/train.gif`
- Optional startup GIF (plays once at power-up, then loops while idle if present):
  - `/gif/startup.gif` (configured by `STARTUP_GIF_PATH`)
- On startup, the sketch verifies all 3 files can be opened.
- If the startup GIF file is missing, the sketch logs a message and continues normally.

## Switch Wiring (Internal Pull-Up, Active-LOW)

The code currently uses:

- `pinMode(pin, INPUT_PULLUP)`
- `VIDEO_ACTIVE_LEVEL = LOW`

Wiring per input pin:

1. Connect one switch terminal to `GND`.
2. Connect the other switch terminal to GPIO (`46`, `9`, or `14`).

Result:

- Switch open -> pin reads `HIGH` -> no GIF for that pin.
- Switch closed -> pin reads `LOW` -> corresponding GIF plays.

Important:

- Do not connect switches to `5V` GPIO logic. Use `3.3V`.

## Runtime Behavior

- At boot, `/gif/startup.gif` is played once (if the file exists).
- When all switches are open (no active pin), `/gif/startup.gif` loops continuously.
- The loop checks pins in this order: `46`, then `9`, then `14`.
- If multiple pins are active at once, the first active pin in that order wins.
- A GIF keeps playing while its pin remains active.
- Releasing the switch returns to the startup GIF loop (or waits if no startup GIF is available).

## Optional Alternatives

If you prefer internal pull-downs:

1. Change `pinMode(VIDEO_PINS[i], INPUT_PULLUP)` to `INPUT_PULLDOWN`.
2. Set `VIDEO_ACTIVE_LEVEL` to `HIGH`.
3. Wire each switch between GPIO and `3.3V`.

Then pressed switch = `HIGH` = active.

## Customization

To change behavior, edit:

- `VIDEO_PINS[]` for GPIO assignment.
- `VIDEO_GIF_PATHS[]` for GIF file paths.
- `STARTUP_GIF_PATH` for the boot/idle GIF path (set to an empty string to disable).
- `VIDEO_ACTIVE_LEVEL` for trigger polarity (`HIGH` or `LOW`).
