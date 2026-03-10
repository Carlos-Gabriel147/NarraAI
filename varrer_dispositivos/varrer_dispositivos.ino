#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// Definições
#define SCAN_PERIOD_MS 1000
#define SCAN_DURATION_S 1
#define MULTIPLE_FINDS true

// ===== LISTA DE DISPOSITIVOS QUE VOCÊ QUER DETECTAR =====
const char* targetNames[] = {
  "MOIZA",
  "2 - Pardal",
  "3 - Calopsita"
};

const int targetCount = sizeof(targetNames) / sizeof(targetNames[0]);

// Variáveis globais
static uint32_t lastScanTime = 0;
volatile int lastRSSI = 0;
int lastValidRSSI = 0;
bool hasValidReading = false;
bool deviceFound = false;

String foundName = "";
String foundMAC = "";

BLEScan* bleScanner = nullptr;

// OLED
#ifdef WIRELESS_STICK_V3
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);
#else
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
#endif

// ========================== Bluetooth Class ========================== //
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice device) override {

    String name = device.getName();

    if(name.isEmpty()) return;

    // verifica se o nome está na lista
    for(int i = 0; i < targetCount; i++){

      if(name == targetNames[i]){

        deviceFound = true;

        lastRSSI = device.getRSSI();
        lastValidRSSI = lastRSSI;
        hasValidReading = true;

        foundName = name;
        foundMAC = device.getAddress().toString();

        Serial.println();
        Serial.println("Encontrado:");
        Serial.print("Nome: ");
        Serial.println(foundName);
        Serial.print("MAC: ");
        Serial.println(foundMAC);
        Serial.print("RSSI: ");
        Serial.println(lastRSSI);

        break;
      }
    }
  }
};

// ========================== Bluetooth Scan ========================== //
void startScan() {
  deviceFound = false;
  bleScanner->start(SCAN_DURATION_S, false);
  bleScanner->clearResults();
}

// ============================== SETUP ============================== //
void setup() {

  Serial.begin(115200);
  delay(200);

  Serial.println("Inicializando OLED...");

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);

  delay(100);

  display.init();
  display.clear();
  display.display();
  display.setContrast(255);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);

  display.drawString(display.getWidth()/2,20,"INICIANDO...");
  display.display();

  Serial.println("OLED OK");

  Serial.println("Inicializando BLE...");

  BLEDevice::init("");

  bleScanner = BLEDevice::getScan();
  bleScanner->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), MULTIPLE_FINDS);
  bleScanner->setActiveScan(false);
  bleScanner->setInterval(200);
  bleScanner->setWindow(100);

  Serial.println("BLE pronto");

  lastScanTime = millis();
}

// ============================== LOOP ============================== //
void loop() {

  if (millis() - lastScanTime >= SCAN_PERIOD_MS) {
    lastScanTime = millis();
    startScan();
  }

  display.clear();

  display.setFont(ArialMT_Plain_24);

  if(deviceFound){
    display.drawString(display.getWidth()/2,18,String(lastRSSI));
  } 
  else{
    display.drawString(display.getWidth()/2,18,"**");
  }

  if(hasValidReading){

    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);

    display.drawString(
      display.getWidth()-2,
      display.getHeight()-12,
      String(lastValidRSSI));

    display.setTextAlignment(TEXT_ALIGN_CENTER);
  }

  display.display();

  delay(10);
}