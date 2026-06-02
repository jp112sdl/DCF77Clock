#include <RTClib.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <NeoPixelBus.h>

#define DCF_PIN 3
#define STATUS_PIN A3
#define LED_PIN 6

#define TFT_RST 8
#define TFT_DC 9
#define TFT_CS 10

#define colorSaturation (uint8_t)255

// LED-Belegung des NeoPixel-Rings
#define LED_COUNT 72
#define LED_RING 60        // Sekunden-/Bitring, LEDs 0..59
#define LED_DOW_START 60   // Wochentag, LEDs 60..67
#define LED_SYNC 68        // Sync-Status (rot = kein Sync, gruen = Sync)
#define LED_CEST 69        // MESZ
#define LED_CET 70         // MEZ
#define LED_LEAP 71        // Schaltsekunde

// Hoechster im Frame verwendeter Bit-Index ist 58 (Paritaet Datum);
// +1 fuer den Index, +ein Reservebit fuer einen verpassten Minutenwechsel.
#define ZEIT_SIZE 61


Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
U8G2_FOR_ADAFRUIT_GFX u8g2;

NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> strip(LED_COUNT, LED_PIN);

RTC_Millis rtc;

unsigned long LOW_Start = 0;   // Beginn der aktuellen Traegerphase (LOW)
unsigned long LOW_Zeit = 0;    // Laenge der zuletzt beendeten Traegerphase
unsigned long HIGH_Start = 0;  // Beginn des aktuellen Absenkimpulses (HIGH)
unsigned long HIGH_Zeit = 0;   // Laenge des zuletzt beendeten Absenkimpulses

bool Signal = false;
bool DCF_SYNC = false;
bool DCF_SYNC_LAST = false;
bool LED59_shown = false;
int8_t BIT = -1;
uint8_t ZEIT[ZEIT_SIZE];


void setup() {
  Serial.begin(115200);

  pinMode(DCF_PIN, INPUT);
  pinMode(STATUS_PIN, OUTPUT);

  tft.init(76, 284);
  tft.invertDisplay(false);
  tft.setRotation(3);
  tft.setTextColor(ST77XX_WHITE);
  u8g2.begin(tft);

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(ST77XX_WHITE);
  u8g2.setFont(u8g2_font_inb16_mr);  // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall

  tft.fillScreen(ST77XX_BLACK);

  u8g2.setCursor(10, 30);
  u8g2.print("Arduino DCF77 Clock");
  u8g2.setCursor(10, 60);
  u8g2.print(" Warte auf Uhrzeit");

  strip.Begin();  // Initialize NeoPixel object

  for (uint8_t i = 0; i < LED_COUNT; i++) {
    strip.SetPixelColor(i, { 16, 16, 16 });
  }
  strip.Show();
  delay(500);
  for (uint8_t i = 0; i < LED_COUNT; i++) {
    strip.SetPixelColor(i, { 0, 0, 0 });
  }

  strip.SetPixelColor(LED_SYNC, { colorSaturation, 0, 0 });
  strip.Show();

  rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));

  Serial.println("Synchronisierung");
}

// Setzt eine LED auf 'color', wenn 'on', sonst aus.
void setOnOffLED(uint16_t idx, bool on, RgbColor color) {
  strip.SetPixelColor(idx, on ? color : RgbColor(0, 0, 0));
}

void setSyncLED() {
  if (DCF_SYNC != DCF_SYNC_LAST) {
    DCF_SYNC_LAST = DCF_SYNC;
    strip.SetPixelColor(LED_SYNC, DCF_SYNC ? RgbColor(0, colorSaturation, 0) : RgbColor(colorSaturation, 0, 0));
    strip.Show();
  }
}

void setCESTLED(uint8_t on) {
  setOnOffLED(LED_CEST, on, RgbColor(0, colorSaturation, 0));
}
void setCETLED(uint8_t on) {
  setOnOffLED(LED_CET, on, RgbColor(0, colorSaturation, 0));
}

void setLEAPLED(uint8_t on) {
  setOnOffLED(LED_LEAP, on, RgbColor(0, colorSaturation, 0));
}

void setDayOfWeekLED(uint8_t dayOfWeek) {
  for (uint8_t d = 0; d < 8; d++) {
    setOnOffLED(LED_DOW_START + d, dayOfWeek == d, RgbColor(colorSaturation, colorSaturation, 0));
  }
}

void setLedRing60Bit(uint8_t bit, bool on) {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  switch (bit) {
    case 0:
      for (uint8_t i = 0; i < LED_RING; i++) strip.SetPixelColor(i, { 0, 0, 0 });
      break;
    case 1 ... 14:
      if (on) b = colorSaturation;
      break;
    case 15 ... 19:
      if (on) r = colorSaturation;
      break;
    case 20:
      if (on) g = colorSaturation;
      break;
    case 21 ... 27:
      if (on) r = colorSaturation;
      break;
    case 28:
      if (on) {
        r = colorSaturation;
        b = colorSaturation;
      }
      break;
    case 29 ... 34:
      if (on) r = colorSaturation;
      break;
    case 35:
      if (on) {
        r = colorSaturation;
        b = colorSaturation;
      }
      break;
    case 36 ... 41:
      if (on) {
        r = colorSaturation;
        g = colorSaturation / 8;
      }
      break;
    case 42 ... 44:
      if (on) {
        b = colorSaturation;
        g = colorSaturation;
      }
      break;
    case 45 ... 49:
      if (on) {
        b = colorSaturation;
        g = colorSaturation;
      }
      break;
    case 50 ... 57:
      if (on) {
        b = colorSaturation;
        g = colorSaturation;
      }
      break;
    case 58:
      if (on) {
        r = colorSaturation;
        b = colorSaturation;
      }
      break;
  }


  if (r == 0 && g == 0 && b == 0) {
    r = 2;
    g = 2;
    b = 2;
  }

  strip.SetPixelColor(bit, { r, g, b });
}

void loop() {
  if (BIT > 58) { DCF_SYNC = false; }
  setSyncLED();
  uint8_t DCF_SIGNAL = digitalRead(DCF_PIN);
  digitalWrite(STATUS_PIN, DCF_SIGNAL);

  // Steigende Flanke: Beginn eines Absenkimpulses = Ende der Traegerphase.
  if (DCF_SIGNAL == HIGH && Signal == false) {
    Signal = true;
    HIGH_Start = millis();
    LOW_Zeit = HIGH_Start - LOW_Start;  // Laenge der gerade beendeten Traegerphase

    if (LOW_Zeit >= 1700) {
      // Lange Traegerphase = fehlende 59. Sekundenmarke -> neue Minute.
      // Der zuvor empfangene Frame (ZEIT[0..58]) ist jetzt vollstaendig.
      NEUMINUTE();
      BIT = 0;
      DCF_SYNC = true;
      LED59_shown = false;
    } else if (DCF_SYNC == true) {
      BIT++;
    } else {
      Serial.print(".");
    }
  }

  // Fallende Flanke: Ende des Absenkimpulses -> Bit aus der Impulsbreite lesen.
  else if (DCF_SIGNAL == LOW && Signal == true) {
    Signal = false;
    LOW_Start = millis();
    HIGH_Zeit = LOW_Start - HIGH_Start;  // Laenge des Absenkimpulses

    if (DCF_SYNC == true && BIT >= 0 && BIT <= 58) {
      PrintBeschreibung(BIT);
      ZEIT[BIT] = BIT_Zeit(HIGH_Zeit);
      setLedRing60Bit(BIT, ZEIT[BIT] == 1);
      Serial.print(ZEIT[BIT]);
      if (ZEIT[BIT] > 1) {
        DCF_SYNC = false;
        BIT = -1;
      }
    }
    strip.Show();
  }

  // Sekunde 59 (Minutenmarke) hat keinen Impuls. Damit die LED nicht erst beim
  // Minutenwechsel erscheint, wird sie nach ~1 s anhaltendem Traeger gesetzt.
  if (DCF_SYNC == true && BIT == 58 && Signal == false && !LED59_shown
      && (millis() - LOW_Start) > 1000) {
    LED59_shown = true;
    setLedRing60Bit(59, 0);
    strip.Show();
  }
}

// Dekodiert ein Bit aus der Breite des Absenkimpulses: ~100 ms = 0, ~200 ms = 1.
// Schwellwerte ggf. an den verwendeten Empfaenger anpassen.
uint8_t BIT_Zeit(unsigned long HIGH_Zeit) {
  if (HIGH_Zeit >= 50 && HIGH_Zeit <= 150) { return 0; }
  if (HIGH_Zeit > 150 && HIGH_Zeit <= 300) { return 1; }
  Serial.print("X");
  Serial.println(HIGH_Zeit, DEC);
  return 3;  // ungueltige Impulsbreite -> Sync verwerfen
}

// Erwartetes Even-Parity-Bit ueber ZEIT[from..to]:
// liefert 1, wenn die Anzahl gesetzter Bits ungerade ist.
uint8_t expectedParity(uint8_t from, uint8_t to) {
  uint8_t cnt = 0;
  for (uint8_t i = from; i < (to + 1); i++) {
    if (ZEIT[i] == 1) cnt++;
  }
  return (cnt % 2 != 0);
}

// Dekodiert 'count' BCD-Bits ab Index 'from' zu einem Dezimalwert.
uint16_t bcdDecode(uint8_t from, uint8_t count) {
  static const uint8_t weights[] = { 1, 2, 4, 8, 10, 20, 40, 80 };
  uint16_t value = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (ZEIT[from + i] == 1) value += weights[i];
  }
  return value;
}

// Wird beim Minutenwechsel aufgerufen; ZEIT[0..58] enthaelt den kompletten,
// gerade abgeschlossenen Frame (inkl. regulaer gemessenem Bit 58).
void NEUMINUTE() {
  uint8_t ZEIT_STUNDE = bcdDecode(29, 6);
  uint8_t ZEIT_MINUTE = bcdDecode(21, 7);
  uint8_t ZEIT_TAG = bcdDecode(36, 6);
  uint8_t ZEIT_MONAT = bcdDecode(45, 5);
  uint16_t ZEIT_JAHR = 2000 + bcdDecode(50, 8);
  uint8_t ZEIT_WOCHENTAG = bcdDecode(42, 3);
  bool PAR_STUNDE = ZEIT[35] & 1;
  bool PAR_MINUTE = ZEIT[28] & 1;
  bool PAR_DATUM = ZEIT[58] & 1;
  uint8_t ZEIT_LEAP = ZEIT[16];
  uint8_t ZEIT_CEST = ZEIT[17];
  uint8_t ZEIT_CET = ZEIT[18];

  uint8_t PAR_ZEIT_MINUTE = expectedParity(21, 27);
  uint8_t PAR_ZEIT_STUNDE = expectedParity(29, 34);
  uint8_t PAR_ZEIT_DATUM = expectedParity(36, 57);

  // Nur uebernehmen, wenn alle Paritaeten stimmen ...
  if (PAR_ZEIT_MINUTE != PAR_MINUTE) return;
  if (PAR_ZEIT_STUNDE != PAR_STUNDE) return;
  if (PAR_ZEIT_DATUM != PAR_DATUM) return;
  if (ZEIT_JAHR <= 2025) return;

  // ... und die Werte plausibel sind.
  if (ZEIT_STUNDE > 23 || ZEIT_MINUTE > 59) return;
  if (ZEIT_TAG < 1 || ZEIT_TAG > 31) return;
  if (ZEIT_MONAT < 1 || ZEIT_MONAT > 12) return;
  if (ZEIT_WOCHENTAG < 1 || ZEIT_WOCHENTAG > 7) return;

  setDayOfWeekLED(ZEIT_WOCHENTAG);
  setCESTLED(ZEIT_CEST);
  setCETLED(ZEIT_CET);
  setLEAPLED(ZEIT_LEAP);
  rtc.adjust(DateTime(ZEIT_JAHR, ZEIT_MONAT, ZEIT_TAG, ZEIT_STUNDE, ZEIT_MINUTE, 0));

  DateTime now = rtc.now();
  char bufDate[] = "DD.MM.YYYY";
  char bufTime[] = "hh:mm";
  Serial.println(now.toString(bufDate));
  Serial.println(now.toString(bufTime));

  // Alte Anzeige loeschen, damit keine Reste schmalerer Werte stehen bleiben.
  tft.fillScreen(ST77XX_BLACK);

  u8g2.setCursor(20, 35);
  u8g2.print("Datum: ");
  u8g2.print(now.toString(bufDate));
  u8g2.setCursor(20, 60);
  u8g2.print("Uhrzeit: ");
  u8g2.print(now.toString(bufTime));
}

void PrintBeschreibung(int BitNummer) {
  switch (BitNummer) {
    case 0: Serial.println("\n# START MINUTE (IMMER 0)"); break;
    case 1: Serial.println("\n# WETTERDATEN"); break;
    case 15: Serial.println("\n# RUFBIT"); break;
    case 16: Serial.println("\n# MEZ/MESZ"); break;
    case 17: Serial.println("\n# MESZ"); break;
    case 18: Serial.println("\n# MEZ"); break;
    case 19: Serial.println("\n# SCHALTSEKUNDE"); break;
    case 20: Serial.println("\n# BEGIN ZEITINFORMATION (IMMER 1)"); break;
    case 21: Serial.println("\n# MINUTEN"); break;
    case 28: Serial.println("\n# PARITAET MINUTE"); break;
    case 29: Serial.println("\n# STUNDE"); break;
    case 35: Serial.println("\n# PARITAET STUNDE"); break;
    case 36: Serial.println("\n# TAG"); break;
    case 42: Serial.println("\n# WOCHENTAG"); break;
    case 45: Serial.println("\n# MONAT"); break;
    case 50: Serial.println("\n# JAHR"); break;
    case 58: Serial.println("\n# PARITAET DATUM"); break;
  }
}
