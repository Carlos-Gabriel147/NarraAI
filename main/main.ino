#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// Definições de valores
#define SCAN_INTERVAL_MS 1000
#define SCAN_DURATION_S 2
#define MULTIPLE_FINDS true

// Ajuste automático para alguns modelos
#ifdef WIRELESS_STICK_V3
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);
#else
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
#endif

// Estrutura dipositivo
struct Device{
  const char* name;
  const char* mac;
  int threshold_rssi;
  int current_rssi;
  bool is_set;
};

// Estrutura das linguages
struct Language{
  const char* language;
  uint8_t id;
};

// Filtro de dispositivos
Device devices[] = {
  {"MOIZA", "de:ad:be:ef:00:02", -80, -9999, false},
  {"MOIZA2", "e6:03:04:12:29:39", -80, -9999, false}
};

// Linguagens
Language languages[] = {
  {"Portugues", 1},
  {"English", 2},
  {"Espanol", 3}
};

// Quantiade de dispositivos e linguagens
const int deviceCount = sizeof(devices) / sizeof(devices[0]);
const int languadeCount = sizeof(languages) / sizeof(languages[0]);

// Variáveis globais
static uint32_t lastScanTime = 0;
int chosen_id = -1;
int pass_rssi = -9999;
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini mp3;
std::vector<int> devices_found;
BLEScan* bleScanner = nullptr;

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
      
        // Adicionar a potencia atual do dispositivo
        devices[i].current_rssi = rsii;

        // Adicionar na lista de dispositivos encontrados, se ele nao estiver nela antes
        bool found = false;
        for (int v : devices_found) {
            if (v == i) {
                found = true;
                break;
            }
        }
        if (!found) {devices_found.push_back(i);}

      }

    }

  }
  
};

// ========================== Bluetooth Scan ========================== //
void startScan() {
  bleScanner->start(SCAN_DURATION_S, false);
  bleScanner->clearResults();
}

// =========================== mp3 Command =========================== //
void sendCommandMp3(uint8_t cmd, uint8_t arg1=0, uint8_t arg2=0){
  uint8_t command[8] = {0x7E, 0xFF, 0x06, cmd, 0x00, arg1, arg2, 0xEF};
  mp3Serial.write(command, sizeof(command)); 
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

  // Inicializaçã mp3
  mp3Serial.begin(9600, SERIAL_8N1, 25, 26);
  delay(1500);
  sendCommandMp3(0x0C);
  delay(1000);
  sendCommandMp3(0x09, 0, 2);
  delay(500);
  sendCommandMp3(0x06, 0, 0x1E);
  delay(500);
  sendCommandMp3(0x0F, 1, 1);

  // Tudo inicializado
  display.clear();
  display.drawString(display.getWidth() / 2, 20, "INICIADO!");
  display.display();

  lastScanTime = millis();
}

// ============================== LOOP ============================== //
void loop() {

  // Scan periódico
  if (millis() - lastScanTime >= (SCAN_INTERVAL_MS + SCAN_DURATION_S*1000)) {
    lastScanTime = millis();
    Serial.println("========= INICIOU SCAN ============");
    startScan();
    Serial.println("========= TERMINOU SCAN ===========");
  }

  if(!devices_found.empty()){

    pass_rssi = -9999;
    
    // Percorre os dispositivos encontrados
    for(int id : devices_found){

      int current_rssi = devices[id].current_rssi;

      if(current_rssi > pass_rssi){
        pass_rssi = current_rssi;
        chosen_id = id;
      }

    }

    // Limpa a lista para a próxima procura
    devices_found.clear();

    Serial.print(chosen_id);
    Serial.print(": ");
    Serial.println(devices[chosen_id].name);

    // Display
    display.clear();

    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(display.getWidth()/2, 0, devices[chosen_id].name);

    display.setFont(ArialMT_Plain_16);
    String rssiText = String(devices[chosen_id].current_rssi);
    display.drawString(display.getWidth()/2, 35, rssiText);

    display.display();
  }

  delay(10);
}