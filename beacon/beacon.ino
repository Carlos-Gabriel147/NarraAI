#include <BTLE.h>
#include <SPI.h>
#include <RF24.h>

RF24 radio(9, 10);
BTLE btle(&radio);

const uint8_t beaconID = 1;
const uint8_t mac[6] = {0xD2, 0x8F, 0x4C, 0x77, 0x21, 0x6B};

/*
RF24_PA_Min  / -18 dBm / 016 uW / 0-5   m
RF24_PA_LOW  / -12 dBm / 063 uw / 5-10  m
RF24_PA_HIGH / -6  dBm / 250 uW / 10-20 m
RF24_PA_MAX  /  0  dBm / 1   mW / 20-50 m
*/

void setup() {

  Serial.begin(115200);

  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  Serial.println("Potência definida.");

  btle.begin("3 - Calopsita");
  btle.setMAC(mac);
  Serial.println("BLE iniciado!");
}

void loop() {

  if(!btle.advertise(0xFF, &beaconID, sizeof(beaconID))) {
    Serial.println("Erro no advertisement");
  }else{
    Serial.println("Pulso!");
  }

  btle.hopChannel();
  delay(100);
}