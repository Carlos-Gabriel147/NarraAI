#include <Arduino.h>
#include <Wire.h>               
#include "HT_SSD1306Wire.h"
#include "images.h"

static const int MP3_RX_PIN = 2; // ESP32 RX2  <- MP3 TX
static const int MP3_TX_PIN = 0; // ESP32 TX2  -> MP3 RX
static const uint32_t MP3_BAUD = 9600;

static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

#define DEMO_DURATION 3000
typedef void (*Demo)(void);

int demoMode = 0;
int counter = 1;

static void mp3SendCmd(uint8_t cmd, uint16_t param, bool feedback = true) {
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
  Serial.println();
  Serial.println();
  VextON();
  delay(100);

  // Initialising the UI will init the display too.
  display.init();

  display.setFont(ArialMT_Plain_10);

  Serial2.begin(MP3_BAUD, SERIAL_8N1, MP3_RX_PIN, MP3_TX_PIN);

  Serial.println("YX5300 control via Serial2 on GPIO16/17");
  delay(1000); // allow MP3 board to boot

  mp3SetVolume(30); // 0..30
  delay(200);

}

void drawFontFaceDemo() {
    // Font Demo1
    // create more fonts at http://oleddisplay.squix.ch/
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Hello world");
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 10, "Hello world");
    display.setFont(ArialMT_Plain_24);
    display.drawString(0, 26, "Hello world");
}

void drawTextFlowDemo() {
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawStringMaxWidth(0, 0, 128,
      "Lorem ipsum\n dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore." );
}

void drawTextAlignmentDemo() {
  // Text alignment demo
  char str[30];
  int x = 0;
  int y = 0;
  display.setFont(ArialMT_Plain_10);
  // The coordinates define the left starting point of the text
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(x, y, "Left aligned (0,0)");

  // The coordinates define the center of the text
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  x = display.width()/2;
  y = display.height()/2-5;
  sprintf(str,"Center aligned (%d,%d)",x,y);
  display.drawString(x, y, str);

  // The coordinates define the right end of the text
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  x = display.width();
  y = display.height()-12;
  sprintf(str,"Right aligned (%d,%d)",x,y);
  display.drawString(x, y, str);
}

void drawRectDemo() {
      // Draw a pixel at given position
    for (int i = 0; i < 10; i++) {
      display.setPixel(i, i);
      display.setPixel(10 - i, i);
    }
    display.drawRect(12, 12, 20, 20);

    // Fill the rectangle
    display.fillRect(14, 14, 17, 17);

    // Draw a line horizontally
    display.drawHorizontalLine(0, 40, 20);

    // Draw a line horizontally
    display.drawVerticalLine(40, 0, 20);
}

void drawCircleDemo() {
  for (int i=1; i < 8; i++) {
    display.setColor(WHITE);
    display.drawCircle(32, 32, i*3);
    if (i % 2 == 0) {
      display.setColor(BLACK);
    }
    display.fillCircle(96, 32, 32 - i* 3);
  }
}

void drawProgressBarDemo() {
  int progress = (counter / 5) % 100;
  // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);

  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, String(progress) + "%");
}

void drawImageDemo() {
    // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
    // on how to create xbm files
    display.drawXbm(0, 0, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
}

void VextON(void)
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextOFF(void) //Vext default OFF
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, HIGH);
}


Demo demos[] = {drawFontFaceDemo, drawTextFlowDemo, drawTextAlignmentDemo, drawRectDemo, drawCircleDemo, drawProgressBarDemo, drawImageDemo};
int demoLength = (sizeof(demos) / sizeof(Demo));
long timeSinceLastModeSwitch = 0;

void loop() {
  // clear the display
  display.clear();
  // draw the current demo method
  demos[6]();

  //display.setTextAlignment(TEXT_ALIGN_RIGHT);
  //display.drawString(10, 128, String(millis()));
  // write the buffer to the display
  display.display();

  //if (millis() - timeSinceLastModeSwitch > DEMO_DURATION) {
  //  demoMode = (demoMode + 1)  % demoLength;
  //  timeSinceLastModeSwitch = millis();
  //}
  //counter++;
  delay(10);

    static bool paused = false;

  // Simple Serial monitor controls:
  // n=next, p=prev, space=pause/resume, s=stop, 1..9=play track #
  if (Serial.available()) {
    char c = Serial.read();
    Serial.println(c);

    if (c == 'n') mp3Next();
    else if (c == 'p') mp3Prev();
    else if (c == 's') { mp3Stop(); paused = false; }
    else if (c == '1') {mp3PlayFolderFile(1,1); paused = false;}
    else if (c == '2') {mp3PlayFolderFile(2,1); paused = false;}
    else if (c == '3') {mp3PlayFolderFile(3,1); paused = false;}
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
