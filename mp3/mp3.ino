#include <Arduino.h>

// --- Pins for Serial2 (hardware UART2) ---
static const int MP3_RX_PIN = 16; // ESP32 RX2  <- MP3 TX
static const int MP3_TX_PIN = 17; // ESP32 TX2  -> MP3 RX
static const uint32_t MP3_BAUD = 9600;

// --- YX5300-style 10-byte command frame ---
// [0] 0x7E start
// [1] 0xFF version
// [2] 0x06 length
// [3] cmd
// [4] feedback (0x00 no, 0x01 yes)
// [5] param high
// [6] param low
// [7] checksum high
// [8] checksum low
// [9] 0xEF end

static void mp3SendCmd(uint8_t cmd, uint16_t param, bool feedback = false) {
  uint8_t buf[10];
  buf[0] = 0x7E;
  buf[1] = 0xFF;
  buf[2] = 0x06;
  buf[3] = cmd;
  buf[4] = feedback ? 0x01 : 0x00;
  buf[5] = (param >> 8) & 0xFF;
  buf[6] = (param >> 0) & 0xFF;

  // checksum = 0 - sum(bytes[1..6])
  uint16_t sum = 0;
  for (int i = 1; i <= 6; i++) sum += buf[i];
  uint16_t checksum = 0 - sum;

  buf[7] = (checksum >> 8) & 0xFF;
  buf[8] = (checksum >> 0) & 0xFF;
  buf[9] = 0xEF;

  Serial2.write(buf, sizeof(buf));
  Serial2.flush();
}

// --- Convenience commands ---
static void mp3SetVolume(uint8_t vol0to30) {
  if (vol0to30 > 30) vol0to30 = 30;
  mp3SendCmd(0x06, vol0to30);
}

static void mp3PlayTrack(uint16_t trackNumber) {
  mp3SendCmd(0x03, trackNumber);
}

static void mp3PlayFolderFile(uint8_t folder, uint8_t file) {
  // Plays /NN/NNN.mp3 style on many modules
  uint16_t param = ((uint16_t)folder << 8) | file;
  mp3SendCmd(0x0F, param);
}

static void mp3Next()   { mp3SendCmd(0x01, 0); }
static void mp3Prev()   { mp3SendCmd(0x02, 0); }
static void mp3Pause()  { mp3SendCmd(0x0E, 0); }
static void mp3Resume() { mp3SendCmd(0x0D, 0); }
static void mp3Stop()   { mp3SendCmd(0x16, 0); }

// Optional: print any responses (only useful if you send commands with feedback=true)
static void mp3PollResponses() {
  while (Serial2.available() >= 10) {
    uint8_t r[10];
    Serial2.readBytes(r, 10);
    Serial.print("MP3 RX: ");
    for (int i = 0; i < 10; i++) {
      if (r[i] < 16) Serial.print('0');
      Serial.print(r[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // Start Serial2 on custom pins
  Serial2.begin(MP3_BAUD, SERIAL_8N1, MP3_RX_PIN, MP3_TX_PIN);

  Serial.println("YX5300 control via Serial2 on GPIO16/17");
  delay(1000); // allow MP3 board to boot

  mp3SetVolume(20); // 0..30
  delay(200);

  // Example playback
  mp3PlayTrack(1);
  // Or: mp3PlayFolderFile(1, 1);
}

void loop() {
  static bool paused = false;

  // Simple Serial monitor controls:
  // n=next, p=prev, space=pause/resume, s=stop, 1..9=play track #
  if (Serial.available()) {
    char c = Serial.read();
    Serial.println(c);

    if (c == 'n') mp3Next();
    else if (c == 'p') mp3Prev();
    else if (c == 's') { mp3Stop(); paused = false; }
    else if (c == ' ') {
      if (!paused) { mp3Pause(); paused = true; }
      else         { mp3Resume(); paused = false; }
    }
    else if (c >= '1' && c <= '9') {
      uint16_t t = (uint16_t)(c - '0');
      mp3PlayTrack(t);
      paused = false;
    }
  }

  mp3PollResponses();
}