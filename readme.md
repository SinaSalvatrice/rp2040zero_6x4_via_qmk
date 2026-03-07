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

If you compile without `-km via` and flash that firmware, VIA may connect to the USB device but fail with protocol/version errors.

## Flash
Enter RP2040 bootloader (BOOTSEL while plugging in, or reset into UF2 mode), then copy the generated `.uf2` to the mounted drive. The RP2040 UF2 flashing flow is the standard QMK method for RP2040 boards.

## VIA troubleshooting
- Load `keymaps/via/via.json` manually in VIA (Design tab -> Load Draft Definition).
- Ensure the flashed firmware was built from this repo after enabling VIA.
- If VIA still shows the old state, unplug/replug the keypad and restart the browser before reconnecting.
