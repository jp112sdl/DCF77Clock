# DCF77 Clock

Arduino-Uhr, die das DCF77-Zeitsignal dekodiert und auf einem TFT-Display
sowie einem NeoPixel-LED-Ring anzeigt.

## Funktion

- Liest das DCF77-Empfängersignal an `DCF_PIN` (D3) bit-genau über die
  Längenmessung der LOW-Phasen (~100 ms = Bit 0, ~200 ms = Bit 1).
- Erkennt den Minutenwechsel an der fehlenden 59. Sekundenmarke (LOW > 1700 ms).
- Dekodiert Minute, Stunde, Tag, Wochentag, Monat, Jahr sowie die Status-Bits
  MEZ/MESZ, Schaltsekunde und Rufbit.
- Prüft die drei DCF77-Paritätsbits (Minute, Stunde, Datum). Nur bei korrekter
  Parität und Jahr > 2025 wird die Zeit übernommen.
- Hält die Zeit per `RTC_Millis` (Software-RTC) zwischen den Synchronisationen.

## Anzeige

**TFT (ST7789, 76×284, SPI):** Datum + Uhrzeit, gerendert mit
`U8g2_for_Adafruit_GFX`. Vor erstem Sync: „Warte auf Uhrzeit".

**NeoPixel-Ring (72× WS2812x an D6):**

| LEDs   | Bedeutung                                            |
|--------|------------------------------------------------------|
| 0–59   | Sekunden-/Bitring, farbcodiert nach DCF77-Bitgruppe  |
| 60–67  | Wochentag (Mo–So)                                    |
| 68     | Sync-Status (rot = kein Sync, grün = Sync)           |
| 69     | MESZ (CEST)                                          |
| 70     | MEZ (CET)                                            |
| 71     | Schaltsekunde (Leap)                                 |

## Hardware / Pins

| Pin     | Funktion                  |
|---------|---------------------------|
| D3      | DCF77-Signal              |
| A3      | Status-Ausgang (Signal-LED)|
| D6      | NeoPixel-Daten            |
| D8/D9/D10 | TFT RST / DC / CS       |

Board: **Arduino Nano (AVR)** · Verkabelung siehe `Verkabelungsdiagramm.png`/`.svg`.

## Bibliotheken

RTClib · Adafruit GFX · Adafruit ST7789 · Adafruit BusIO ·
U8g2_for_Adafruit_GFX · NeoPixelBus

## Build

Kompilierung wird per GitHub Action (`.github/workflows/compile.yml`) bei
jedem Push automatisch geprüft.
