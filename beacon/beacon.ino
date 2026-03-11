#include <BTLE.h>
#include <SPI.h>
#include <RF24.h>

RF24 radio(9, 10);
BTLE btle(&radio);

uint8_t beaconID = 2;

uint8_t mac[6] = {0xD2, 0x8F, 0x4C, 0x77, 0x21, 0x6B};

void setup() {

  Serial.begin(115200);
  btle.begin("3 - Calopsita");

  btle.setMAC(mac);   // ← define MAC aqui
}

void loop() {

  if(!btle.advertise(0xFF, &beaconID, sizeof(beaconID))) {
    Serial.println("Erro no advertisement");
  }

  btle.hopChannel();
  delay(100);
}