#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// Definições de valores
#define SCAN_PERIOD_MS 1000
#define SCAN_DURATION_S 1
#define MULTIPLE_FINDS true

// Variáveis globais
static uint32_t lastScanTime = 0;
static bool moizaFound = false;
volatile int lastRSSI = 0;
volatile bool newData = false;
int lastValidRSSI = 0;
bool hasValidReading = false;
BLEScan* bleScanner = nullptr;

// Ajuste automático para alguns modelos
#ifdef WIRELESS_STICK_V3
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);
#else
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
#endif

// ========================== Bluetooth Class ========================== //
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice device) override {

    String name = device.getName();

    if (!name.isEmpty() && name == "MOIZA2") {

      moizaFound = true;

      lastRSSI = device.getRSSI();
      lastValidRSSI = lastRSSI;
      hasValidReading = true;
      newData = true;

      Serial.print("RSSI: ");
      Serial.println(lastRSSI);
      Serial.print("MAC: ");
      Serial.println(device.getAddress().toString());
    }
  }
};

// ========================== Bluetooth Scan ========================== //
void startScan() {
  moizaFound = false;
  bleScanner->start(SCAN_DURATION_S, false);
  bleScanner->clearResults();
}

// ============================== SETUP ============================== //
void setup() {

  // Inicialização do serial
  Serial.begin(115200);
  delay(200);

  // Inicialização do OLED
  Serial.println("Inicializando OLED...");

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);  // LOW liga alimentação do OLED

  delay(100);

  display.init();
  display.clear();
  display.display();
  display.setContrast(255);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);

  display.drawString(display.getWidth() / 2, 20, "INICIANDO...");
  display.display();

  Serial.println("OLED OK");

  // Inicialização do Bluetooth
  Serial.println("Inicializando BLE...");

  BLEDevice::init("");

  bleScanner = BLEDevice::getScan();
  bleScanner->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), MULTIPLE_FINDS);
  bleScanner->setActiveScan(false);  // menor consumo
  bleScanner->setInterval(200);
  bleScanner->setWindow(100);

  Serial.println("BLE pronto");

  lastScanTime = millis();
}

// ============================== LOOP ============================== //
void loop() {

  // Scan periódico
  if (millis() - lastScanTime >= SCAN_PERIOD_MS) {
    lastScanTime = millis();
    startScan();
  }

  display.clear();

  // ===== TEXTO CENTRAL GRANDE =====
  display.setFont(ArialMT_Plain_24);

  if (moizaFound) {
    // Mostra leitura atual
    display.drawString(display.getWidth() / 2, 18, String(lastRSSI));
  } else {
    // Não encontrou → mostra **
    display.drawString(display.getWidth() / 2, 18, "**");
  }

  // ===== ÚLTIMA LEITURA PEQUENA NO CANTO =====
  if (hasValidReading) {

    display.setFont(ArialMT_Plain_10);

    display.setTextAlignment(TEXT_ALIGN_RIGHT);

    display.drawString(
      display.getWidth() - 2,
      display.getHeight() - 12,
      String(lastValidRSSI));

    display.setTextAlignment(TEXT_ALIGN_CENTER);
  }

  display.display();

  delay(10);
}