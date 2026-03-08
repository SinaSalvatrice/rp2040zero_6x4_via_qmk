# RP2040 Zero 6x4 VIA

Handwired QMK keyboard for RP2040 Zero.

## Hardware
- Rows: GP0, GP1, GP2, GP3, GP4, GP5
- Cols: GP6, GP7, GP8, GP9
- Diode direction: ROW2COL
- Encoder: A=GP10, B=GP11
- Encoder button: GP12
- WS2812 / NeoPixel: GP13, 10 LEDs

## Build locally
```bash
qmk compile -kb rp2040zero_6x4_via_qmk -km via
```

VIA support is enabled at keyboard level in `rules.mk`, so firmware built from any keymap in this repo stays VIA-compatible.

## Build on GitHub Actions
- Run the `Build QMK Firmware (rp2040zero_6x4_via_qmk)` workflow.
- Download artifact `rp2040zero_6x4_via_qmk_via_firmware`.
- Flash `rp2040zero_6x4_via_qmk_via.uf2` from that artifact.

## Flash
Enter RP2040 bootloader (BOOTSEL while plugging in, or reset into UF2 mode), then copy the generated `.uf2` to the mounted drive. The RP2040 UF2 flashing flow is the standard QMK method for RP2040 boards.

## VIA troubleshooting
- If VIA shows “Fetching v3 definition failed”, load the bundled v3 definition from `keymaps/via/via.json` (Design tab -> Load Draft Definition).
- Ensure the flashed firmware was built from this repo after enabling VIA.
- Load `keymaps/via/via.json` manually in VIA (Design tab -> Load Draft Definition).
- If VIA reports `via.json Object: should NOT have additional properties`, keep the VIA definition minimal. In this repo, removing `"lighting": "qmk_rgblight"` from `keymaps/via/via.json` fixed the error.
- Ensure the flashed firmware was built from this repo after the latest VIA settings update.
- If VIA still shows the old state, unplug/replug the keypad and restart the browser before reconnecting.
