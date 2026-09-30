/*
  vMix ESP-NOW Wireless Tally
  MASTER

  Hardware:
  - WeMos D1 Mini / ESP8266
  - ST7789 240x240 TFT

  Features:
  - Receives tally data from PC via Serial (57600 baud)
  - Sends tally state to 4 receivers using ESP-NOW
  - Displays PGM/PVW state on ST7789
  - Receives acknowledgements from receivers
  - Shows individual receiver ONLINE/OFFLINE status
*/

#include <ESP8266WiFi.h>
#include <espnow.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>


// ======================================================
// USER CONFIGURATION
// ======================================================

// Receiver MAC addresses.
// Replace these with the MAC addresses of your receivers.

uint8_t peer1[] = {0x8C, 0xCE, 0x4E, 0xCE, 0x4D, 0x82};
uint8_t peer2[] = {0xE0, 0x98, 0x06, 0x14, 0x9A, 0x73};
uint8_t peer3[] = {0xE0, 0x98, 0x06, 0x13, 0xA4, 0xA1};
uint8_t peer4[] = {0xCC, 0x50, 0xE3, 0x16, 0x39, 0x01};


// ======================================================
// TFT CONFIGURATION
// ======================================================

// WeMos D1 Mini:
//
// ST7789 GND  -> GND
// ST7789 VCC  -> 3V3
// ST7789 SCK  -> D5 / GPIO14
// ST7789 MOSI -> D7 / GPIO13
// ST7789 RST  -> D2 / GPIO4
// ST7789 DC   -> D1 / GPIO5
// ST7789 BLK  -> 3V3

#define TFT_DC   5
#define TFT_RST  4
#define TFT_CS   -1

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);


// ======================================================
// COMMUNICATION SETTINGS
// ======================================================

const unsigned long HEARTBEAT_MS = 250;
const unsigned long OFFLINE_MS   = 2500;


// ======================================================
// PACKETS
// ======================================================

typedef struct {
  uint16_t pins;
} tally_packet;

typedef struct {
  uint8_t camera;
} ack_packet;

tally_packet myData;


// ======================================================
// TIMERS / ONLINE STATE
// ======================================================

unsigned long lastSend = 0;

unsigned long lastSeen[4] = {
  0, 0, 0, 0
};

bool cameraOnline[4] = {
  false, false, false, false
};

bool displayedOnline[4] = {
  false, false, false, false
};


// ======================================================
// DISPLAY COLORS
// ======================================================

#define COLOR_BG      ST77XX_BLACK
#define COLOR_BORDER  0x4208
#define COLOR_OFF     0x2104
#define COLOR_PGM     ST77XX_RED
#define COLOR_PVW     ST77XX_GREEN
#define COLOR_OFFLINE 0x8410


// ======================================================
// CAMERA STATE
// ======================================================

enum CamState {
  CAM_UNKNOWN,
  CAM_OFF,
  CAM_PREVIEW,
  CAM_PROGRAM
};

CamState displayedState[4] = {
  CAM_UNKNOWN,
  CAM_UNKNOWN,
  CAM_UNKNOWN,
  CAM_UNKNOWN
};


// ======================================================
// GET CAMERA TALLY STATE
// ======================================================

CamState getCamState(uint8_t cam) {

  int actBit;
  int preBit;

  switch (cam) {

    case 1:
      actBit = 10;
      preBit = 9;
      break;

    case 2:
      actBit = 12;
      preBit = 11;
      break;

    case 3:
      actBit = 14;
      preBit = 13;
      break;

    case 4:
      actBit = 1;
      preBit = 0;
      break;

    default:
      return CAM_OFF;
  }

  bool isAct =
    (myData.pins & (1U << actBit)) != 0;

  bool isPre =
    (myData.pins & (1U << preBit)) != 0;

  // PROGRAM has priority over PREVIEW.

  if (isAct) {
    return CAM_PROGRAM;
  }

  if (isPre) {
    return CAM_PREVIEW;
  }

  return CAM_OFF;
}


// ======================================================
// CAMERA TILE POSITION
// ======================================================

void getCamPosition(uint8_t cam, int &x, int &y) {

  switch (cam) {

    case 1:
      x = 10;
      y = 45;
      break;

    case 2:
      x = 125;
      y = 45;
      break;

    case 3:
      x = 10;
      y = 130;
      break;

    case 4:
      x = 125;
      y = 130;
      break;
  }
}


// ======================================================
// ONLINE / OFFLINE INDICATOR
// ======================================================

void drawOnlineStatus(uint8_t cam) {

  int x, y;

  getCamPosition(cam, x, y);

  uint16_t color;

  if (cameraOnline[cam - 1]) {
    color = ST77XX_GREEN;
  }
  else {
    color = COLOR_OFFLINE;
  }

  // Black border around the indicator.

  tft.fillCircle(
    x + 91,
    y + 13,
    6,
    ST77XX_BLACK
  );

  // Status indicator.

  tft.fillCircle(
    x + 91,
    y + 13,
    4,
    color
  );

  displayedOnline[cam - 1] =
    cameraOnline[cam - 1];
}


// ======================================================
// DRAW CAMERA TILE
// ======================================================

void drawCamera(
  uint8_t cam,
  int x,
  int y,
  CamState state
) {

  uint16_t color;
  uint16_t textColor;

  const char *stateText;

  if (state == CAM_PROGRAM) {

    color = COLOR_PGM;
    textColor = ST77XX_WHITE;
    stateText = "PGM";

  }
  else if (state == CAM_PREVIEW) {

    color = COLOR_PVW;
    textColor = ST77XX_BLACK;
    stateText = "PVW";

  }
  else {

    color = COLOR_OFF;
    textColor = ST77XX_WHITE;
    stateText = "---";
  }


  tft.fillRoundRect(
    x,
    y,
    105,
    75,
    7,
    color
  );

  tft.drawRoundRect(
    x,
    y,
    105,
    75,
    7,
    COLOR_BORDER
  );


  // Camera number.

  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setTextColor(textColor);

  tft.setCursor(
    x + 12,
    y + 12
  );

  tft.print("CAM ");
  tft.print(cam);


  // Tally state.

  tft.setCursor(
    x + 33,
    y + 44
  );

  tft.print(stateText);


  // Receiver status.

  drawOnlineStatus(cam);
}


// ======================================================
// UPDATE TALLY DISPLAY
// ======================================================

// Only redraw a camera tile when its tally state changes.
// This greatly reduces TFT flickering.

void updateDisplay() {

  for (uint8_t cam = 1; cam <= 4; cam++) {

    CamState newState =
      getCamState(cam);

    if (
      newState !=
      displayedState[cam - 1]
    ) {

      int x, y;

      getCamPosition(
        cam,
        x,
        y
      );

      drawCamera(
        cam,
        x,
        y,
        newState
      );

      displayedState[cam - 1] =
        newState;
    }
  }
}


// ======================================================
// UPDATE RECEIVER ONLINE STATE
// ======================================================

void updateOnlineStates() {

  unsigned long now = millis();

  for (uint8_t i = 0; i < 4; i++) {

    bool newOnline = false;

    if (
      lastSeen[i] != 0 &&
      (now - lastSeen[i]) < OFFLINE_MS
    ) {
      newOnline = true;
    }

    cameraOnline[i] =
      newOnline;

    // Only redraw the small status indicator
    // when the online state changes.

    if (
      cameraOnline[i] !=
      displayedOnline[i]
    ) {

      drawOnlineStatus(i + 1);
    }
  }
}


// ======================================================
// RECEIVE ACK FROM RECEIVER
// ======================================================

void OnDataRecv(
  uint8_t *mac,
  uint8_t *incomingData,
  uint8_t len
) {

  if (len != sizeof(ack_packet)) {
    return;
  }

  ack_packet ack;

  memcpy(
    &ack,
    incomingData,
    sizeof(ack)
  );

  if (
    ack.camera >= 1 &&
    ack.camera <= 4
  ) {

    lastSeen[ack.camera - 1] =
      millis();
  }
}


// ======================================================
// SEND TALLY
// ======================================================

void sendTally() {

  esp_now_send(
    peer1,
    (uint8_t *)&myData,
    sizeof(myData)
  );

  esp_now_send(
    peer2,
    (uint8_t *)&myData,
    sizeof(myData)
  );

  esp_now_send(
    peer3,
    (uint8_t *)&myData,
    sizeof(myData)
  );

  esp_now_send(
    peer4,
    (uint8_t *)&myData,
    sizeof(myData)
  );

  lastSend = millis();
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  // Serial communication with PC / vMix.

  Serial.begin(57600);


  // ----------------------------------------------------
  // TFT
  // ----------------------------------------------------

  SPI.begin();

  tft.init(
    240,
    240,
    SPI_MODE3
  );

  tft.setRotation(0);
  tft.setTextWrap(false);

  tft.fillScreen(COLOR_BG);


  // Title.

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(
    55,
    13
  );

  tft.print("vMix TALLY");


  // Footer.

  tft.setTextSize(1);
  tft.setTextColor(0x8410);

  tft.setCursor(
    55,
    220
  );

  tft.print("ESP-NOW MASTER");


  // ----------------------------------------------------
  // ESP-NOW
  // ----------------------------------------------------

  WiFi.mode(WIFI_STA);

  WiFi.setSleepMode(
    WIFI_NONE_SLEEP
  );

  if (esp_now_init() != 0) {

    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);

    tft.setCursor(
      35,
      215
    );

    tft.print("ESP-NOW ERR");

    return;
  }


  // Master sends tally and receives ACK packets.

  esp_now_set_self_role(
    ESP_NOW_ROLE_COMBO
  );

  esp_now_register_recv_cb(
    OnDataRecv
  );


  // Add all receivers.

  esp_now_add_peer(
    peer1,
    ESP_NOW_ROLE_COMBO,
    1,
    NULL,
    0
  );

  esp_now_add_peer(
    peer2,
    ESP_NOW_ROLE_COMBO,
    1,
    NULL,
    0
  );

  esp_now_add_peer(
    peer3,
    ESP_NOW_ROLE_COMBO,
    1,
    NULL,
    0
  );

  esp_now_add_peer(
    peer4,
    ESP_NOW_ROLE_COMBO,
    1,
    NULL,
    0
  );


  // Default tally state.

  myData.pins = 0;


  // Initial display.

  updateDisplay();


  // Initial ESP-NOW packet.

  sendTally();
}


// ======================================================
// LOOP
// ======================================================

void loop() {

  bool stateChanged = false;


  // ----------------------------------------------------
  // SERIAL DATA FROM VMIX
  // ----------------------------------------------------

  while (Serial.available() >= 3) {

    uint8_t status =
      Serial.read();

    uint8_t data1 =
      Serial.read();

    uint8_t data2 =
      Serial.read();


    // --------------------------------------------------
    // 0x90
    // --------------------------------------------------

    if (status == 0x90) {

      // data1 -> bits 7 to 13

      myData.pins &=
        ~((uint16_t)0x3F80);

      myData.pins |=
        ((uint16_t)(data1 & 0x7F) << 7);


      // CAM3 Action -> helper bit 14

      if (data2 & 0x01) {

        myData.pins |=
          (1U << 14);

      }
      else {

        myData.pins &=
          ~(1U << 14);
      }

      stateChanged = true;
    }


    // --------------------------------------------------
    // 0x91
    // --------------------------------------------------

    else if (status == 0x91) {

      // data1 -> bits 0 to 6

      myData.pins &=
        ~((uint16_t)0x007F);

      myData.pins |=
        (data1 & 0x7F);

      stateChanged = true;
    }
  }


  // ----------------------------------------------------
  // TALLY STATE CHANGED
  // ----------------------------------------------------

  if (stateChanged) {

    // Send immediately.

    sendTally();

    // Only changed camera tiles will be redrawn.

    updateDisplay();
  }


  // ----------------------------------------------------
  // ESP-NOW HEARTBEAT
  // ----------------------------------------------------

  if (
    millis() - lastSend >=
    HEARTBEAT_MS
  ) {

    sendTally();
  }


  // ----------------------------------------------------
  // ONLINE / OFFLINE STATUS
  // ----------------------------------------------------

  updateOnlineStates();


  yield();
}
