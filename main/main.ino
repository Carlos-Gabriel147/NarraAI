/*
 * Project: NarraAI Beacon Reader
 * Author: Carlos Gabriel Bezerra Pereira
 * Institution: Universidade Federal de Pernambuco (UFPE)
 * Lab: VoxarLabs - voxarlabs@cin.ufpe.br
 * Contact: carlosgabrieljj@gmail.com
 * Created: 2026
 * License: MIT
*/

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
#define BUTTON_INT_RS 18
#define DEBOUNCE_TIME 20
#define COUNTER_MAX_0_SIG 6

// Escolher display de acordo com a placa
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
  {"1 - Moiza", "de:ad:be:ef:00:02", -90, -9999, false},
  {"2 - Pardal", "03:00:ef:be:ad:de", -90, -9999, false},
  {"3 - Calopsita", "6b:21:77:4c:8f:d2", -90, -9999, false}
};

// Linguagens
const char* languages[] = { "Português", "English", "Español" };

// Quantiade de dispositivos e linguagens
const size_t num_languages = sizeof(languages) / sizeof(languages[0]);
const size_t deviceCount = sizeof(devices) / sizeof(devices[0]);

// Variáveis globais
volatile bool finished_scan = false;
volatile bool bnt_lg_press = false;
volatile bool bnt_rs_press = false;
volatile bool repeat_audio = false;
volatile bool playing_audio = false;
volatile int chosen_language = 0;
volatile int chosen_id = -1;

uint8_t dcr_counter = 0;
uint8_t display_custom_rssi = 0;
uint32_t lastScanTime = 0;
int pass_rssi = -9999;

std::vector<int> devices_found;

HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini mp3;
BLEScan* bleScanner = nullptr;

// ========================== Bluetooth Class ========================== //
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice device) override {

    // Pega o MAC e a potencia do sinal atual do dispositivo encontrado
    int rsii = device.getRSSI();

    // Verifica se é um dispositivo da lista
    for (int i = 0; i < deviceCount; i++) {

      // Se a potencia atual é maior do que um limite, salva esse dispositivo
      if ((device.getAddress().toString() == devices[i].mac) and (rsii > devices[i].threshold_rssi)) {

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
  mp3Serial.write(command, 8);
  mp3Serial.flush();
}

// =========================== Thread display =========================== //
void displayRefrash(void *parameter) {

  while(true) {

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);

    // ---------------- BARRAS DE SINAL ----------------
    int bars = 0;

    if(display_custom_rssi >= 70) bars = 5;
    else if(display_custom_rssi >= 55) bars = 4;
    else if(display_custom_rssi >= 45) bars = 3;
    else if(display_custom_rssi >= 25) bars = 2;
    else if(display_custom_rssi >= 10) bars = 1;
    else bars = 0;

    int barWidth = 3;
    int spacing = 2;

    int totalWidth = 5 * barWidth + 4 * spacing;
    int baseX = display.getWidth() - totalWidth - 2;
    int baseY = 14;

    if(bars == 0){

      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);

      int centerX = baseX + totalWidth / 2;
      display.drawString(centerX, 2, "x");

    }
    else{

      for(int i = 0; i < bars; i++) {
        int height = 2 + i * 2;
        int x = baseX + i * (barWidth + spacing);
        int y = baseY - height;
        display.fillRect(x, y, barWidth, height);
      }

    }

    // ---------------- TÍTULO ----------------
    display.setFont(ArialMT_Plain_16);

    if(chosen_id >= 0){
      display.drawString(display.getWidth()/2, 16, devices[chosen_id].name);
    }
    else{
      display.drawString(display.getWidth()/2, 16, "- - -");
    }

    // ---------------- IDIOMA ----------------
    display.setFont(ArialMT_Plain_16);
    display.drawString(display.getWidth()/2, display.getHeight() - 20, languages[chosen_language]);

    display.display();

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// =========================== Troca de linguagem =========================== //
void IRAM_ATTR buttonLG_ISR(){
  // Chama a thread uma vez
  if(!bnt_lg_press){
    bnt_lg_press = true;
    xTaskCreatePinnedToCore(changeLanguage, "changeLanguage", 4096, NULL, 1, NULL, 0);
  }
}

void changeLanguage(void *parameter){

  bool current_state = 0;
  bool previous_state = 1;
  uint32_t time = millis();

  while(true){

    // Ler o estado atual
    current_state = digitalRead(BUTTON_INT_LG);

    // Reseta contagem se há instabilidade
    if(current_state != previous_state){
      time = millis();
    }

    // Se não houve instabilidade depois de um tempo
    if(millis() - time >= DEBOUNCE_TIME){

      // Incrementar linguagem
      if(current_state==LOW && !playing_audio){

        playing_audio = true;
        chosen_language += 1;
        chosen_language %= num_languages;

        sendCommandMp3(0x0F, false, 99, chosen_language+1);

        while(true){
          if(mp3Serial.available()){
            if(mp3Serial.read()==61){
              Serial.println("Terminou audio");
              break;
            }
          }
          delay(1);
        }
        playing_audio = false;
    }

      bnt_lg_press = false;
      break;
    }

    // Atualizar estado anterior
    previous_state = current_state;

    // Limitar o uso da cpu
    vTaskDelay(1 / portTICK_PERIOD_MS);

  }

  vTaskDelete(NULL);
}

// =========================== Reset de set  =========================== //
void IRAM_ATTR buttonRS_ISR(){
  // Chama a thread uma vez
  if(!bnt_rs_press){
    bnt_rs_press = true;
    xTaskCreatePinnedToCore(resetSet, "resetSet", 4096, NULL, 1, NULL, 0);
  }
}

void resetSet(void *parameter){

  bool current_state = 0;
  bool previous_state = 1;
  uint32_t time = millis();

  while(true){

    // Ler o estado atual
    current_state = digitalRead(BUTTON_INT_RS);

    // Reseta contagem se há instabilidade
    if(current_state != previous_state){
      time = millis();
    }

    // Se não houve instabilidade depois de um tempo
    if(millis() - time >= DEBOUNCE_TIME){

      // Ativar flag de repetição
      if(current_state == LOW && !playing_audio && chosen_id>=0){
        repeat_audio = true;
      }

      bnt_rs_press = false;
      break;
    }

    // Atualizar estado anterior
    previous_state = current_state;

    // Limitar uso da CPU
    vTaskDelay(1 / portTICK_PERIOD_MS);

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
  Serial.print("Inicializando OLED... ");

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);

  delay(100);

  display.init();
  display.clear();
  display.display();
  display.setContrast(255);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(display.getWidth() / 2, 20, "Starting...");
  display.display();

  Serial.println("OK!");

  // Inicialização do Bluetooth
  Serial.print("Inicializando BLE... ");

  BLEDevice::init("");
  bleScanner = BLEDevice::getScan();
  bleScanner->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), MULTIPLE_FINDS);
  bleScanner->setActiveScan(false);  // menor consumo
  bleScanner->setInterval(200);
  bleScanner->setWindow(100);

  Serial.println("OK!");

  // Seta a interrupção da mudança de linguagem
  pinMode(BUTTON_INT_LG, INPUT_PULLUP);
  attachInterrupt(BUTTON_INT_LG, buttonLG_ISR, FALLING);

  // Seta a interrupção do reset de set
  pinMode(BUTTON_INT_RS, INPUT_PULLUP);
  attachInterrupt(BUTTON_INT_RS, buttonRS_ISR, FALLING);

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

  // Inicializaçã do MP3
  Serial.print("Inicializando MP3... ");
  mp3Serial.begin(9600, SERIAL_8N1, VIRTUAL_RX, VIRTUAL_TX); delay(100);
  sendCommandMp3(0x0C, false, 0, 0); delay(100);
  sendCommandMp3(0x06, false, 0, 0x1E); delay(1000);
  sendCommandMp3(0x0F, true, 98, 1);
  while(true){
    if(mp3Serial.available()){
      if(mp3Serial.read()==61){
        break;
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  Serial.println("OK!");

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
  if (finished_scan) {
    finished_scan = false;

    if(!devices_found.empty()){

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

      // Evitar acesso de memória inválido, caso haja algum erro
      if(chosen_id<0) chosen_id=0;

      // Executar som do id encontrado/escolhido
      if (!devices[chosen_id].is_set && !playing_audio) {
        playing_audio = true;
        sendCommandMp3(0x0F, true, chosen_language + 1, chosen_id + 1);

        Serial.println();
        Serial.println("Tocando MP3: " + (String)devices[chosen_id].name + " (" + languages[chosen_language] + ")");

        // Espera terminar o audio atual para continuar para o próximo audio
        while(true){
          if(mp3Serial.available()){
            if(mp3Serial.read()==61){
              Serial.println("Terminou");
              break;
            }
          }
          vTaskDelay(1 / portTICK_PERIOD_MS);
        }

        playing_audio = false;

        // Setar que o atual foi reproduzio e limpar os outros
        for(size_t i=0; i<deviceCount; i++){
          devices[i].is_set = (i==chosen_id);
        } 
      }

      // Rssi que é mostrado na tela
      display_custom_rssi = max(1, min(devices[chosen_id].current_rssi+120, 100));
      dcr_counter = 0;

    // Se finalizou um scan e nao encontrou mais dispositivos, dizer que é 0 de "potencia"
    }else{
      dcr_counter += 1;
      if(dcr_counter>=COUNTER_MAX_0_SIG){
        display_custom_rssi = 0;
        dcr_counter = 0;
      }
    }

  }

  // Repetir audio se solicitado
  if(repeat_audio && chosen_id>=0){
    playing_audio = true;
    Serial.println("Iniciou Repetição");
    sendCommandMp3(0x0F, true, chosen_language + 1, chosen_id + 1);
    while(true){
      if(mp3Serial.available()){
        if(mp3Serial.read()==61){
          Serial.println("Terminou");
          break;
        }
      }
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    playing_audio = false;
    repeat_audio = false;
  }

  vTaskDelay(10 / portTICK_PERIOD_MS);
}
