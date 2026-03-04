#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// Definições de valores
#define SCAN_PERIOD_MS 2000
#define SCAN_DURATION_S 1
#define MULTIPLE_FINDS true

// Estrutura dipositivo
struct Device{
  const char* name;
  const char* mac;
  int threshold_rssi;
  int current_rssi;
  bool is_set;
};

// Variáveis globais
static uint32_t lastScanTime = 0;
volatile bool newData = false;
std::vector<int> devices_found;
BLEScan* bleScanner = nullptr;
Device devices[] = {
  {"Macaco", "de:ad:be:ef:00:02", -70, -9999, false},
  {"Rato", "11:22:33:44:55:66", -70, -9999, false},
  {"Arara", "aa:22:33:44:55:66", -70, -9999, false},
  {"Tartaruga", "bb:22:33:44:55:66", -70, -9999, false}
};
const int deviceCount = sizeof(devices) / sizeof(devices[0]);

// Ajuste automático para alguns modelos
#ifdef WIRELESS_STICK_V3
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);
#else
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
#endif

// ========================== Bluetooth Class ========================== //
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice device) override {
  
    // Pega o MAC e a potencia do sinal atual do dispositivo encontrado
    String mac = device.getAddress().toString();
    int rsii = device.getRSSI();
    
    // Verifica se é um dispositivo da lista
    for(int i=0; i<deviceCount; i++){
      
      // Se a potencia atual é maior do que um limite, salva esse dispositivo
      if((mac==devices[i].mac) and (rsii>devices[i].threshold_rssi) and (!devices[i].is_set)){
        devices[i].current_rssi = rsii;
        devices_found.push_back(i);
        newData = true;
      }

    }

  }
  
};

// ========================== Bluetooth Scan ========================== //
void startScan() {
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

  if(newData){

    newData = false;
    int chosen_id = -1;
    int pass_rssi = -9999;
    
    // Percorre dos dispositivos da lista encotrados
    for(int id in devices_found){

      // Se a potencia do sinal atual de um dispositivo for maior que outro, escolhe o maior
      if(device[id].current_rssi > pass_rssi){
        chosen_id = id;
      }

      // Atualiza e limpa valores do rssi
      pass_rssi = device[id].current_rssi;
      device[id].current_rssi = -9999;
    }

    Serial.print(chosen_id);
    Serial.print(": ");
    Serial.println(devices[chosen_id]);

  }



  Serial.println();

  //display.clear();
  //display.setFont(ArialMT_Plain_24);
  //display.display();

  delay(10);
}