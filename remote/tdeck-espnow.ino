
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include "TouchDrvGT911.hpp"
#include "utilities.h"

// =====================================================
// CONFIGURATION
// =====================================================

#define WIFI_CHANNEL 6
#define HEARTBEAT_INTERVAL 150

// Enter your rover receiver ESP32 MAC address here
uint8_t RECEIVER_MAC[6] = {
  0xA0, 0xB7, 0x65, 0x6E, 0x44, 0xD0
};

// =====================================================
// MATRIX THEME
// =====================================================

#define UI_BG       TFT_BLACK
#define UI_BUTTON   0x0200
#define UI_ACTIVE   0x07E0
#define UI_BORDER   0x07E0
#define UI_STOP     0x3000
#define UI_STOP_ON  0xF800
#define UI_TEXT     0x07E0

// =====================================================
// COMMAND PROTOCOL
// =====================================================

#define PACKET_MAGIC 0x5444
#define PROTOCOL_VERSION 1

enum FrameType : uint8_t {
  FRAME_COMMAND = 1,
  FRAME_HEARTBEAT = 2
};

enum RoverCommand : uint8_t {
  CMD_STOP = 0,
  CMD_FORWARD = 1,
  CMD_BACKWARD = 2,
  CMD_LEFT = 3,
  CMD_RIGHT = 4
};

struct __attribute__((packed)) RoverPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t frameType;
  uint8_t command;
  uint8_t reserved;
  uint16_t sequence;
};

// =====================================================
// HARDWARE OBJECTS AND VARIABLES
// =====================================================

TFT_eSPI tft = TFT_eSPI();
TouchDrvGT911 touch;

int16_t touchX[5];
int16_t touchY[5];

bool touchWasPressed = false;
bool peerReady = false;

uint16_t sequenceNumber = 0;
uint32_t lastHeartbeat = 0;

RoverCommand activeCommand = CMD_STOP;

// =====================================================
// BACKLIGHT
// =====================================================

void setBrightness(uint8_t value) {
  static uint8_t level = 0;

  const uint8_t steps = 16;

  if (value == 0) {
    digitalWrite(BOARD_BL_PIN, LOW);
    level = 0;
    return;
  }

  if (level == 0) {
    digitalWrite(BOARD_BL_PIN, HIGH);
    level = steps;
    delayMicroseconds(30);
  }

  int from = steps - level;
  int to = steps - value;
  int count = (steps + to - from) % steps;

  for (int i = 0; i < count; i++) {
    digitalWrite(BOARD_BL_PIN, LOW);
    digitalWrite(BOARD_BL_PIN, HIGH);
  }

  level = value;
}

// =====================================================
// DISPLAY
// =====================================================

void drawButton(
  int x,
  int y,
  int w,
  int h,
  const char *arrow,
  const char *label,
  uint16_t color
) {
  tft.fillRoundRect(x, y, w, h, 8, color);

  // Neon green double outline
  tft.drawRoundRect(x, y, w, h, 8, UI_BORDER);
  tft.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 6, UI_BORDER);

  uint16_t textColor =
    (color == UI_STOP || color == UI_STOP_ON)
      ? TFT_WHITE
      : UI_TEXT;

  tft.setTextColor(textColor, color);

  tft.drawCentreString(
    arrow,
    x + w / 2,
    y + 5,
    4
  );

  tft.drawCentreString(
    label,
    x + w / 2,
    y + h - 21,
    2
  );
}

// =====================================================
// STATUS BAR
// =====================================================

void drawStatus() {
  tft.fillRect(0, 0, 320, 27, UI_BG);

  tft.setTextColor(UI_ACTIVE, UI_BG);
  tft.drawString("ROVER REMOTE", 6, 5, 2);

  if (peerReady) {
    tft.setTextColor(UI_ACTIVE, UI_BG);
    tft.drawRightString("PEER READY", 314, 5, 2);
  } else {
    tft.setTextColor(TFT_ORANGE, UI_BG);
    tft.drawRightString("NO RECEIVER", 314, 5, 2);
  }
}

// =====================================================
// DRAW ALL CONTROL BUTTONS
// =====================================================

void drawControls() {

  drawButton(
    110, 31, 100, 57,
    "^", "FORWARD",
    activeCommand == CMD_FORWARD
      ? UI_ACTIVE : UI_BUTTON
  );

  drawButton(
    8, 96, 96, 67,
    "<", "LEFT",
    activeCommand == CMD_LEFT
      ? UI_ACTIVE : UI_BUTTON
  );

  drawButton(
    112, 96, 96, 67,
    "X", "STOP",
    activeCommand == CMD_STOP
      ? UI_STOP_ON : UI_STOP
  );

  drawButton(
    216, 96, 96, 67,
    ">", "RIGHT",
    activeCommand == CMD_RIGHT
      ? UI_ACTIVE : UI_BUTTON
  );

  drawButton(
    110, 171, 100, 57,
    "v", "BACKWARD",
    activeCommand == CMD_BACKWARD
      ? UI_ACTIVE : UI_BUTTON
  );
}

// =====================================================
// COMPLETE INTERFACE
// =====================================================

void drawInterface() {
  tft.fillScreen(UI_BG);

  drawStatus();
  drawControls();

  tft.setTextColor(UI_TEXT, UI_BG);
  tft.drawString("COMMAND:", 7, 220, 1);
}

// =====================================================
// ESP-NOW
// =====================================================

void onDataSent(
  const uint8_t *mac,
  esp_now_send_status_t status
) {
  Serial.println(
    status == ESP_NOW_SEND_SUCCESS
      ? "ESP-NOW: sent"
      : "ESP-NOW: failed"
  );
}

bool receiverConfigured() {
  for (int i = 0; i < 6; i++) {
    if (RECEIVER_MAC[i] != 0) {
      return true;
    }
  }

  return false;
}

void sendFrame(uint8_t type, RoverCommand command) {
  if (!peerReady) return;

  RoverPacket packet = {
    PACKET_MAGIC,
    PROTOCOL_VERSION,
    type,
    (uint8_t)command,
    0,
    ++sequenceNumber
  };

  esp_now_send(
    RECEIVER_MAC,
    (uint8_t *)&packet,
    sizeof(packet)
  );
}

void sendCommand(RoverCommand command) {
  activeCommand = command;

  sendFrame(FRAME_COMMAND, command);

  lastHeartbeat = millis();

  Serial.printf("Command: %u\n", command);

  // Immediately refresh button colours
  drawControls();
}

void setupESPNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(
    WIFI_CHANNEL,
    WIFI_SECOND_CHAN_NONE
  );

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  if (!receiverConfigured()) {
    Serial.println("Receiver MAC not configured");
    return;
  }

  esp_now_peer_info_t peer = {};

  memcpy(peer.peer_addr, RECEIVER_MAC, 6);

  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) == ESP_OK) {
    peerReady = true;
    Serial.println("Receiver peer added");
  }
}

// =====================================================
// TOUCHSCREEN COMMAND HANDLING
// =====================================================

void handleTap(int16_t x, int16_t y) {
  Serial.printf("Touch: %d, %d\n", x, y);

  // FORWARD
  if (x >= 110 && x <= 210 &&
      y >= 31 && y <= 88) {
    sendCommand(CMD_FORWARD);
  }

  // BACKWARD
  else if (x >= 110 && x <= 210 &&
           y >= 171 && y <= 228) {
    sendCommand(CMD_BACKWARD);
  }

  // LEFT
  else if (x >= 8 && x <= 104 &&
           y >= 96 && y <= 163) {
    sendCommand(CMD_LEFT);
  }

  // STOP
  else if (x >= 112 && x <= 208 &&
           y >= 96 && y <= 163) {
    sendCommand(CMD_STOP);
  }

  // RIGHT
  else if (x >= 216 && x <= 312 &&
           y >= 96 && y <= 163) {
    sendCommand(CMD_RIGHT);
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("MATRIX ROVER REMOTE STARTING");

  // Power
  pinMode(BOARD_POWERON, OUTPUT);
  digitalWrite(BOARD_POWERON, HIGH);

  // TFT chip select
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  // SPI
  SPI.begin(
    BOARD_SPI_SCK,
    BOARD_SPI_MISO,
    BOARD_SPI_MOSI,
    BOARD_TFT_CS
  );

  // Display
  tft.begin();
  tft.setRotation(1);

  pinMode(BOARD_BL_PIN, OUTPUT);
  setBrightness(16);

  // Touchscreen
  pinMode(BOARD_TOUCH_INT, INPUT);

  Wire.begin(
    BOARD_I2C_SDA,
    BOARD_I2C_SCL
  );

  touch.setPins(-1, BOARD_TOUCH_INT);

  if (!touch.begin(
    Wire,
    GT911_SLAVE_ADDRESS_L
  )) {
    Serial.println("GT911 not found");

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);

    tft.drawCentreString(
      "TOUCHSCREEN ERROR",
      160,
      110,
      2
    );

    while (true) {
      delay(1000);
    }
  }

  touch.setMaxCoordinates(320, 240);
  touch.setSwapXY(true);
  touch.setMirrorXY(false, true);

  Serial.println("GT911 touchscreen ready");

  // ESP-NOW
  setupESPNow();

  // Draw interface
  drawInterface();

  Serial.println("MATRIX ROVER REMOTE READY");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // Continuously poll GT911
  uint8_t count = touch.getPoint(
    touchX,
    touchY,
    touch.getSupportTouchPoint()
  );

  bool currentlyTouched = (count > 0);

  // Detect new touch
  if (currentlyTouched && !touchWasPressed) {
    Serial.println("NEW TOUCH");

    Serial.printf(
      "X: %d  Y: %d\n",
      touchX[0],
      touchY[0]
    );

    handleTap(touchX[0], touchY[0]);
  }

  // Detect release
  if (!currentlyTouched && touchWasPressed) {
    Serial.println("TOUCH RELEASED");
  }

  touchWasPressed = currentlyTouched;

  // Keep rover connection alive
  if (peerReady &&
      millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {

    sendFrame(
      FRAME_HEARTBEAT,
      activeCommand
    );

    lastHeartbeat = millis();
  }

  delay(10);
}
