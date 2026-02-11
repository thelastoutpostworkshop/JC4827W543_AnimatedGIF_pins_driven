# JC4827W543 Animated GIF (Pin Driven)

This project plays GIFs from SD card based on the state of 3 GPIO pins on an ESP32-S3.

## Pin To GIF Mapping

Defined in `JC4827W543_AnimatedGIF_pins_driven.ino`:

- `VIDEO_PINS[] = {46, 9, 14}`
- `VIDEO_GIF_PATHS[] = {"/gif/video1.gif", "/gif/video2.gif", "/gif/video3.gif"}`
- `VIDEO_ACTIVE_LEVEL = HIGH`

Mapping:

- Pin `46` -> `/gif/video1.gif`
- Pin `9` -> `/gif/video2.gif`
- Pin `14` -> `/gif/video3.gif`

## GIF File Requirements

- GIF files must exist on the SD card at the exact hardcoded paths.
- Current defaults:
  - `/gif/video1.gif`
  - `/gif/video2.gif`
  - `/gif/video3.gif`
- On startup, the sketch verifies all 3 files can be opened.

## Switch Wiring (Internal Pull-Down, Active-HIGH)

The code currently uses:

- `pinMode(pin, INPUT_PULLDOWN)`
- `VIDEO_ACTIVE_LEVEL = HIGH`

Wiring per input pin:

1. Connect one switch terminal to `3.3V`.
2. Connect the other switch terminal to GPIO (`46`, `9`, or `14`).

Result:

- Switch open -> pin reads `LOW` -> no GIF for that pin.
- Switch closed -> pin reads `HIGH` -> corresponding GIF plays.

Important:

- Do not connect switches to `5V` GPIO logic. Use `3.3V`.

## Runtime Behavior

- The loop checks pins in this order: `46`, then `9`, then `14`.
- If multiple pins are active at once, the first active pin in that order wins.
- A GIF keeps playing while its pin remains active.
- Releasing the switch (pin inactive) stops playback and waits.

## Optional Alternatives

If you prefer internal pull-ups:

1. Change `pinMode(VIDEO_PINS[i], INPUT_PULLDOWN)` to `INPUT_PULLUP`.
2. Set `VIDEO_ACTIVE_LEVEL` to `LOW`.
3. Wire each switch between GPIO and `GND`.

Then pressed switch = `LOW` = active.

## Customization

To change behavior, edit:

- `VIDEO_PINS[]` for GPIO assignment.
- `VIDEO_GIF_PATHS[]` for GIF file paths.
- `VIDEO_ACTIVE_LEVEL` for trigger polarity (`HIGH` or `LOW`).
