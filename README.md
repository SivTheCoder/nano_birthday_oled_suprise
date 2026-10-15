# Nano Birthday OLED Suprise

A small Arduino Nano project that combines animated robot eyes with a button-triggered birthday surprise. It uses a 128×64 I²C OLED, a push button, and a passive piezo buzzer.

> **Status:** Hobby project in progress. Compile and test on your own hardware before relying on the behavior; the sketch has not been verified here against your exact Arduino IDE/library versions.

## Features

- Animated robot eyes with startup behavior
- Button-triggered birthday sequence
- Pixel-art heart, cake, candle flicker, and sparkles
- Happy Birthday melody using a passive piezo buzzer
- Message screens advanced by button presses
- Millis-based timing for the added birthday sequence and melody
- Serial diagnostics and animation commands retained from the source sketch

## Hardware

- Arduino Nano (ATmega328P-compatible)
- 128×64 I²C OLED, commonly using address `0x3C`
- Momentary push button
- Passive piezo buzzer
- Breadboard and jumper wires

## Wiring

| Component | Arduino Nano |
|---|---|
| OLED VCC | Match your OLED module's required supply voltage |
| OLED GND | GND |
| OLED SDA | A4 |
| OLED SCL | A5 |
| Button terminal 1 | D3 |
| Button terminal 2 | GND |
| Passive buzzer + | D9 |
| Passive buzzer − | GND |

The sketch configures the button as `INPUT_PULLUP`, so a press connects D3 to GND. Confirm your OLED module's voltage requirements before connecting power.

## Software setup

1. Install the Arduino IDE.
2. Install **Adafruit GFX Library** and **Adafruit SSD1306** from Library Manager.
3. Open `BirthdayRobot_Integrated.ino`.
4. Select the correct Arduino Nano board and processor/bootloader option for your board.
5. Compile with **Verify** before uploading.
6. Open Serial Monitor at **115200 baud** for diagnostics.

## Controls

- Press the button in normal mode to start the birthday sequence.
- During the automatic animation, button presses are ignored so the sequence is not accidentally skipped.
- Once the message screens begin, press the button to advance through them.
- After the last message, press again to return to normal robot-eye mode.
- Serial commands retained by the sketch: `I` checks I²C, `R` reinitializes the OLED, and `A0` through `A8` request animation modes.

## Project structure

```text
robot-birthday-oled/
├── BirthdayRobot_Integrated.ino
├── README.md
├── LICENSE
└── .gitignore
```

## Notes

- The melody is intended for a **passive** piezo buzzer; an active buzzer may not play different pitches correctly.
- OLED modules and Nano clones can differ. Check your module's address and supply requirements if the display does not initialize.
- The visual appearance of text depends on the OLED panel, viewing angle, and library font rendering.
- This repository is shared for learning and experimentation. Contributions and issue reports are welcome.

## License

Released under the MIT License. See [LICENSE](LICENSE).
