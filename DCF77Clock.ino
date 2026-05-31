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


Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
U8G2_FOR_ADAFRUIT_GFX u8g2;

#define LED_COUNT 72
NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> strip(LED_COUNT, LED_PIN);

RTC_Millis rtc;

uint16_t HIGH_Start = 0;
uint16_t HIGH_Ende = 0;
uint16_t HIGH_Zeit = 0;
uint16_t LOW_Start = 0;
uint16_t LOW_Ende = 0;
uint16_t LOW_Zeit = 0;

bool Signal = false;
bool DCF_SYNC = false;
bool DCF_SYNC_LAST = false;
bool FIRST_SYNC = true;
int8_t BIT = -1;
uint8_t ZEIT[65];

unsigned long lastLCDMillis = 0;
unsigned long lastLEDStripMillis = 0;


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

  strip.SetPixelColor(68, { colorSaturation, 0, 0 });
  strip.Show();

  rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));

  Serial.println("Synchronisierung");
}

void setSyncLED() {
  if (DCF_SYNC != DCF_SYNC_LAST) {
    DCF_SYNC_LAST = DCF_SYNC;
    strip.SetPixelColor(68, { DCF_SYNC ? (uint8_t)0 : colorSaturation, DCF_SYNC ? colorSaturation : (uint8_t)0, 0 });
    strip.Show();
  }
}

void setCESTLED(uint8_t on) {
  strip.SetPixelColor(69, { 0, on ? colorSaturation : (uint8_t)0, 0 });
}
void setCETLED(uint8_t on) {
  strip.SetPixelColor(70, { 0, on ? colorSaturation : (uint8_t)0, 0 });
}

void setLEAPLED(uint8_t on) {
  strip.SetPixelColor(71, { 0, on ? colorSaturation : (uint8_t)0, 0 });
}

void setDayOfWeekLED(uint8_t dayOfWeek) {
  for (uint8_t d = 0; d < 8; d++) {
    strip.SetPixelColor(60 + d, { dayOfWeek == d ? colorSaturation : (uint8_t)0, dayOfWeek == d ? colorSaturation : (uint8_t)0, 0 });
  }
}

void setLedRing60Bit(uint8_t bit, bool on) {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  switch (bit) {
    case 0:
      for (uint8_t i = 0; i < 60; i++) strip.SetPixelColor(i, { 0, 0, 0 });
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
  if (BIT > 60) { DCF_SYNC = false; }
  setSyncLED();
  int DCF_SIGNAL = digitalRead(DCF_PIN);
  digitalWrite(STATUS_PIN, DCF_SIGNAL);

  if (DCF_SIGNAL == HIGH && Signal == false) {
    Signal = true;
    HIGH_Start = millis();
    LOW_Ende = HIGH_Start;
    LOW_Zeit = LOW_Ende - LOW_Start;

    if (DCF_SYNC == true) {
      PrintBeschreibung(BIT);
      //Serial.print("Bit");
      //Serial.print (BIT);
      //Serial.print (": ");
      ZEIT[BIT] = (BIT_Zeit(LOW_Zeit));
      setLedRing60Bit(BIT, ZEIT[BIT] == 1);
      Serial.print(ZEIT[BIT]);
      if (ZEIT[BIT] > 1) {
        DCF_SYNC = false;
        BIT = -1;
      }
      //Serial.println ();
    } else {
      Serial.print(".");
      setLedRing60Bit(0, 0);
    }

    strip.Show();
  }

  else if (DCF_SIGNAL == LOW && Signal == true) {
    Signal = false;
    HIGH_Ende = millis();
    LOW_Start = HIGH_Ende;
    HIGH_Zeit = HIGH_Ende - HIGH_Start;

    NEUMINUTE(LOW_Zeit);
    strip.Show();
  }
}

uint8_t BIT_Zeit(int LOW_Zeit) {
  if (LOW_Zeit >= 851 && LOW_Zeit <= 960) { return 0; }
  if (LOW_Zeit >= 750 && LOW_Zeit <= 850) { return 1; }
  Serial.print("X");
  Serial.println(LOW_Zeit, DEC);
  if (LOW_Zeit <= 450) { return 2; }
  return 3;
}

uint8_t even_parity(uint8_t from, uint8_t to) {
  uint8_t cnt = 0;
  for (uint8_t i = from; i < (to + 1); i++) {
    if (ZEIT[i] == 1) cnt++;
  }
  return (cnt % 2 != 0);
}

void NEUMINUTE(int LOW_Zeit) {
  if (LOW_Zeit >= 1700) {
    //Serial.println("L="+String(LOW_Zeit));
    BIT = 0;
    setLedRing60Bit(59, 0);
    strip.Show();

    DCF_SYNC = true;
    uint8_t ZEIT_STUNDE = ZEIT[29] * 1 + ZEIT[30] * 2 + ZEIT[31] * 4 + ZEIT[32] * 8 + ZEIT[33] * 10 + ZEIT[34] * 20;
    uint8_t ZEIT_MINUTE = ZEIT[21] * 1 + ZEIT[22] * 2 + ZEIT[23] * 4 + ZEIT[24] * 8 + ZEIT[25] * 10 + ZEIT[26] * 20 + ZEIT[27] * 40;
    uint8_t ZEIT_TAG = ZEIT[36] * 1 + ZEIT[37] * 2 + ZEIT[38] * 4 + ZEIT[39] * 8 + ZEIT[40] * 10 + ZEIT[41] * 20;
    uint8_t ZEIT_MONAT = ZEIT[45] * 1 + ZEIT[46] * 2 + ZEIT[47] * 4 + ZEIT[48] * 8 + ZEIT[49] * 10;
    uint16_t ZEIT_JAHR = 2000 + ZEIT[50] * 1 + ZEIT[51] * 2 + ZEIT[52] * 4 + ZEIT[53] * 8 + ZEIT[54] * 10 + ZEIT[55] * 20 + ZEIT[56] * 40 + ZEIT[57] * 80;
    uint8_t ZEIT_WOCHENTAG = ZEIT[42] * 1 + ZEIT[43] * 2 + ZEIT[44] * 4;
    bool PAR_STUNDE = ZEIT[35] & 1;
    bool PAR_MINUTE = ZEIT[28] & 1;
    bool PAR_DATUM = (LOW_Zeit <= 1860);  //1900 = 0, 1800 = 1
    uint8_t ZEIT_LEAP = ZEIT[16];
    uint8_t ZEIT_CEST = ZEIT[17];
    uint8_t ZEIT_CET = ZEIT[18];

    //Serial.println(PAR_DATUM, BIN);

    uint8_t PAR_ZEIT_MINUTE = even_parity(21, 27);
    uint8_t PAR_ZEIT_STUNDE = even_parity(29, 34);
    uint8_t PAR_ZEIT_DATUM = even_parity(36, 57);

    if (PAR_ZEIT_MINUTE == PAR_MINUTE) {
      if (PAR_ZEIT_STUNDE == PAR_STUNDE) {
        if (PAR_ZEIT_DATUM == PAR_DATUM) {
          if (ZEIT_JAHR > 2025) {
            setDayOfWeekLED(ZEIT_WOCHENTAG);
            setCESTLED(ZEIT_CEST);
            setCETLED(ZEIT_CET);
            setLEAPLED(ZEIT_LEAP);
            rtc.adjust(DateTime(ZEIT_JAHR, ZEIT_MONAT, ZEIT_TAG, ZEIT_STUNDE, ZEIT_MINUTE, 0));
            static DateTime now = rtc.now();
            now = rtc.now();
            //char bufDate[] = "DDD, DD.MM.YYYY hh:mm:ss";
            char bufDate[] = "DD.MM.YYYY";
            char bufTime[] = "hh:mm";
            Serial.println(now.toString(bufDate));
            Serial.println(now.toString(bufTime));

            if (FIRST_SYNC == true) {
              FIRST_SYNC = false;
              tft.fillScreen(ST77XX_BLACK);
            }

            u8g2.setCursor(20, 35);
            u8g2.print("Datum: ");
            u8g2.print(now.toString(bufDate));
            u8g2.setCursor(20, 60);
            u8g2.print("Uhrzeit: ");
            u8g2.print(now.toString(bufTime));

            /*
            Serial.println();
            Serial.println("*****************************");
            Serial.print ("Uhrzeit: ");
            Serial.println();
            Serial.print (ZEIT_STUNDE);
            Serial.print (":");
            Serial.print (ZEIT_MINUTE);
            Serial.println();
            Serial.println();
            Serial.print ("Datum: ");
            Serial.println();
            Serial.print (ZEIT_TAG);
            Serial.print (".");
            Serial.print (ZEIT_MONAT);
            Serial.print (".");
            Serial.print (ZEIT_JAHR+2000);
            Serial.println();
            Serial.print ("Wochentag: ");
            Serial.println();
            Serial.print (ZEIT_WOCHENTAG);
            Serial.println();
            Serial.println("*****************************");
            */
          }
        }
      }
    }
  } else {
    BIT++;
  }
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
