#include <BTLE.h>
#include <SPI.h>
#include <RF24.h>

RF24 radio(9, 10);
BTLE btle(&radio);

uint8_t dummy = 0x00;   // dado mínimo

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("BTLE beacon iniciado");

  // nome (máx 8 chars)
  btle.begin("3 - Calopsita");
}

void loop() {

  if(!btle.advertise(0xFF, &dummy, sizeof(dummy))) {  // 0xFF = manufacturer data
    Serial.println("Erro no advertisement");
  }

  btle.hopChannel();

  delay(100);
}