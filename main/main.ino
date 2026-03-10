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
#define VIRTUAL_RX 2
#define VIRTUAL_TX 0
#define BUTTON_INT_LG 17
#define DEBOUNCE_TIME 20

// Ajuste automático para alguns modelos
#ifdef WIRELESS_STICK_V3
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED);
#else
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
#endif

// Estrutura dipositivo
struct Device {
  const char* name;
  const char* mac;
  int threshold_rssi;
  int current_rssi;
  bool is_set;
};

// Filtro de dispositivos
volatile Device devices[] = {
  { "MOIZA", "de:ad:be:ef:00:02", -80, -9999, false },
  { "MOIZA2", "e6:03:04:12:29:39", -80, -9999, false }
};

// Linguagens
const char* languages[] = { "Português", "English", "Español" };
size_t num_languages = sizeof(languages) / sizeof(languages[0]);

// Quantiade de dispositivos
const int deviceCount = sizeof(devices) / sizeof(devices[0]);

// Variáveis globais
volatile bool finished_scan = false;
static uint32_t lastScanTime = 0;
int pass_rssi = -9999;
volatile int chosen_id = -1;
volatile int chosen_language = 0;
volatile bool bnt_lg_press = false;

std::vector<int> devices_found;

HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini mp3;
BLEScan* bleScanner = nullptr;

// ========================== Bluetooth Class ========================== //
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice device) override {

    // Pega o MAC e a potencia do sinal atual do dispositivo encontrado
    String mac = device.getAddress().toString();
    int rsii = device.getRSSI();

    // Verifica se é um dispositivo da lista
    for (int i = 0; i < deviceCount; i++) {

      // Se a potencia atual é maior do que um limite, salva esse dispositivo
      if ((mac == devices[i].mac) and (rsii > devices[i].threshold_rssi) and (!devices[i].is_set)) {

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
        if (!found) { devices_found.push_back(i); }
      }
    }
  }
};

// ========================== Bluetooth Scan ========================== //
void onEndScan(BLEScanResults scanResults) {
  finished_scan = true;
  bleScanner->clearResults();
}

// ========================== Bluetooth Scan ========================== //
void startScan() {
  bleScanner->start(SCAN_DURATION_S, &onEndScan);
}

// =========================== MP3 Command =========================== //
void sendCommandMp3(uint8_t cmd, bool fb = true, uint8_t arg1 = 0, uint8_t arg2 = 0) {
  uint8_t command[8] = { 0x7E, 0xFF, 0x06, cmd, (uint8_t)fb, arg1, arg2, 0xEF };
  mp3Serial.write(command, sizeof(command));
  mp3Serial.flush();
}

// =========================== Thread display =========================== //
void displayRefrash(void *parameter) {

  while(true) {

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(display.getWidth()/2, 20, languages[chosen_language]);
    display.display();

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }

}

// =========================== Troca de linguagem =========================== //
void IRAM_ATTR buttonISR(){
  if(!bnt_lg_press){
    bnt_lg_press = true;
    xTaskCreatePinnedToCore(changeLanguage, "changeLanguade", 4096, NULL, 1, NULL, 0);
  }
}

void changeLanguage(void *parameter){

  bool current_state = 0;
  bool previous_state = 1;
  uint32_t time = millis();

  while(true){

    current_state = digitalRead(BUTTON_INT_LG);

    if(current_state != previous_state){
      time = millis();
    }

    if(millis() - time >= DEBOUNCE_TIME){

      if(current_state == LOW){
        chosen_language += 1;
        chosen_language %= num_languages;
      }

      bnt_lg_press = false;
      break;
    }

    previous_state = current_state;

  }

  vTaskDelete(NULL);
}

// ============================== SETUP ============================== //
void setup() {

  // Inicialização do serial
  Serial.begin(115200);
  delay(200);

  // Inicialização do OLED
  Serial.println();
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

  Serial.println("BLE OK");

  // Inicializaçã do MP3
  mp3Serial.begin(9600, SERIAL_8N1, VIRTUAL_RX, VIRTUAL_TX); delay(100);
  sendCommandMp3(0x0C, false, 0, 0); delay(100);
  sendCommandMp3(0x06, false, 0, 0x1E);

  // Seta a interrupção da mudança de linguagem
  pinMode(BUTTON_INT_LG, INPUT_PULLUP);
  attachInterrupt(BUTTON_INT_LG, buttonISR, FALLING);

  // Tudo inicializado
  display.clear();
  display.drawString(display.getWidth() / 2, 20, "INICIADO!");
  display.display();

  // Inicia thread de atualização do display
  xTaskCreatePinnedToCore(
    displayRefrash,      // função
    "DisplayRefrash",    // nome
    4096,             // stack
    NULL,             // parâmetro
    1,                // prioridade
    NULL,             // handle
    0                 // core (0 ou 1)
  );

  lastScanTime = millis();
}

// ============================== LOOP ============================== //
void loop() {

  // Scan periódico
  if (millis() - lastScanTime >= (SCAN_INTERVAL_MS + SCAN_DURATION_S * 1000)) {
    lastScanTime = millis();
    startScan();
  }

  // Ao término do scan e se encontrou algo
  if (finished_scan && !devices_found.empty()) {

    finished_scan = false;
    pass_rssi = -9999;

    // Percorre os dispositivos encontrados
    for (int id : devices_found) {

      int current_rssi = devices[id].current_rssi;

      if (current_rssi > pass_rssi) {
        pass_rssi = current_rssi;
        chosen_id = id;
      }
    }

    // Limpa a lista para a próxima procura
    devices_found.clear();

    // Executar som do id encontrado/escolhido
    if (!devices[chosen_id].is_set) {
      sendCommandMp3(0x0F, true, chosen_language + 1, chosen_id + 1);
    }

    Serial.println();
    Serial.print("Chosen language: ");
    Serial.println(chosen_language);
    Serial.print("Chosen id: ");
    Serial.println(chosen_id);
    Serial.println("MP3: " + (String)(chosen_language + 1) + (String)(chosen_id + 1));

    // Espera terminar o audio atual para continuar para o próximo audio
    while(true){
      if(mp3Serial.available()){
        if(mp3Serial.read()==61){
          Serial.println("Terminou audio");
          break;
        }
      }
    }

    // Setar que já foi reproduzido
    devices[chosen_id].is_set = true;
  }

  delay(10);
}