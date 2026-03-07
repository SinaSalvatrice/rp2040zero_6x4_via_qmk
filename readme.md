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

## Flash
Enter RP2040 bootloader (BOOTSEL while plugging in, or reset into UF2 mode), then copy the generated `.uf2` to the mounted drive. The RP2040 UF2 flashing flow is the standard QMK method for RP2040 boards.
