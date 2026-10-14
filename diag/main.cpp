// DIAG: sprawdza tylko wyswietlacz, podswietlenie i wejscia. Wynik na porcie USB-JTAG (Serial).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>
static TFT_eSPI tft;
static Adafruit_NeoPixel ring(5, 48, NEO_GRB + NEO_KHZ800);
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("DIAG: boot ok");
  Serial.printf("DIAG: psram=%u heap=%u flash=%u\n", (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFlashChipSize());
  pinMode(1, OUTPUT); digitalWrite(1, HIGH);
  pinMode(2, OUTPUT); digitalWrite(2, HIGH);
  pinMode(46, OUTPUT); digitalWrite(46, HIGH);
  pinMode(41, INPUT_PULLUP); pinMode(45, INPUT_PULLUP); pinMode(42, INPUT_PULLUP);
  Serial.println("DIAG: gpio ok, calling tft.init()");
  ring.begin(); ring.show();
  tft.init();
  Serial.println("DIAG: tft.init() ok");
  tft.setRotation(0);
}
void loop() {
  static const uint16_t cols[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
  static const char *names[] = {"RED", "GREEN", "BLUE", "WHITE"};
  static int i = 0;
  tft.fillScreen(cols[i]);
  // pierscien RGB w tym samym kolorze co ekran (zla kolejnosc kolorow = inny kolor niz ekran)
  static const uint32_t led[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF};
  for (int k = 0; k < 5; k++) ring.setPixelColor(k, ((led[i] >> 16) & 0xFF) * 40 / 255, ((led[i] >> 8) & 0xFF) * 40 / 255, (led[i] & 0xFF) * 40 / 255);
  ring.show();
  Serial.printf("DIAG: fill %s | btn=%d encA=%d encB=%d\n", names[i], digitalRead(41), digitalRead(45), digitalRead(42));
  i = (i + 1) % 4;
  delay(1000);
}
