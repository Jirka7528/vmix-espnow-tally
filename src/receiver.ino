/*
  vMix ESP-NOW Wireless Tally
  RECEIVER

  Hardware:
  - ESP8266 / WeMos D1 Mini
  - WS2812 / NeoPixel LED

  Features:
  - Receives tally state using ESP-NOW
  - PROGRAM = Red
  - PREVIEW = Green
  - Sends periodic acknowledgement to master
  - Automatically turns tally LED off if master signal is lost
*/

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Adafruit_NeoPixel.h>


// ======================================================
// USER CONFIGURATION
// ======================================================

// Change ONLY this value for each receiver:
//
// CAM1 = 1
// CAM2 = 2
// CAM3 = 3
// CAM4 = 4

#define CAMERA_NUMBER 1


// ======================================================
// CAMERA TALLY BIT MAPPING
// ======================================================

#if CAMERA_NUMBER == 1

  const int Act = 10;
  const int Pre = 9;

#elif CAMERA_NUMBER == 2

  const int Act = 12;
  const int Pre = 11;

#elif CAMERA_NUMBER == 3

  const int Act = 14;
  const int Pre = 13;

#elif CAMERA_NUMBER == 4

  const int Act = 1;
  const int Pre = 0;

#else

  #error "CAMERA_NUMBER must be between 1 and 4"

#endif


// ======================================================
// NEOPIXEL
// ======================================================

// GPIO4 = D2 on WeMos D1 Mini.

#define LED_PIN 4

Adafruit_NeoPixel pixels(
  1,
  LED_PIN,
  NEO_GRB + NEO_KHZ800
);


// ======================================================
// COMMUNICATION SETTINGS
// ======================================================

const unsigned long SIGNAL_TIMEOUT_MS = 1500;
const unsigned long ACK_INTERVAL_MS    = 1000;


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
ack_packet ack;


// ======================================================
// MASTER INFORMATION
// ======================================================

// The receiver automatically learns the master's
// MAC address from the first valid tally packet.

uint8_t masterMac[6];

bool masterKnown = false;


// ======================================================
// TIMERS
// ======================================================

unsigned long lastPacket = 0;
unsigned long lastAck = 0;

bool signalLost = true;


// ======================================================
// UPDATE TALLY LED
// ======================================================

void updateLED() {

  bool isAct =
    (myData.pins & (1U << Act)) != 0;

  bool isPre =
    (myData.pins & (1U << Pre)) != 0;


  // PROGRAM has priority.

  if (isAct) {

    // PROGRAM = RED

    pixels.setPixelColor(
      0,
      pixels.Color(
        255,
        0,
        0
      )
    );

  }
  else if (isPre) {

    // PREVIEW = GREEN

    pixels.setPixelColor(
      0,
      pixels.Color(
        0,
        255,
        0
      )
    );

  }
  else {

    // OFF

    pixels.setPixelColor(
      0,
      0
    );
  }

  pixels.show();
}


// ======================================================
// REMEMBER MASTER
// ======================================================

void rememberMaster(uint8_t *mac) {

  if (masterKnown) {
    return;
  }

  memcpy(
    masterMac,
    mac,
    6
  );


  // Add master as an ESP-NOW peer
  // so the receiver can send ACK packets back.

  esp_now_add_peer(
    masterMac,
    ESP_NOW_ROLE_COMBO,
    1,
    NULL,
    0
  );

  masterKnown = true;
}


// ======================================================
// RECEIVE TALLY
// ======================================================

void OnDataRecv(
  uint8_t *mac,
  uint8_t *incomingData,
  uint8_t len
) {

  if (
    len != sizeof(tally_packet)
  ) {
    return;
  }


  // Learn master MAC automatically.

  rememberMaster(mac);


  // Copy tally data.

  memcpy(
    &myData,
    incomingData,
    sizeof(myData)
  );


  lastPacket = millis();
  signalLost = false;


  // Update tally LED immediately.

  updateLED();
}


// ======================================================
// SEND ACK
// ======================================================

void sendAck() {

  if (!masterKnown) {
    return;
  }

  ack.camera =
    CAMERA_NUMBER;

  esp_now_send(
    masterMac,
    (uint8_t *)&ack,
    sizeof(ack)
  );

  lastAck = millis();
}


// ======================================================
// SETUP
// ======================================================

void setup() {

  // ----------------------------------------------------
  // NEOPIXEL
  // ----------------------------------------------------

  pixels.begin();

  pixels.clear();

  pixels.show();


  // ----------------------------------------------------
  // ESP-NOW
  // ----------------------------------------------------

  WiFi.mode(WIFI_STA);

  WiFi.setSleepMode(
    WIFI_NONE_SLEEP
  );

  if (esp_now_init() != 0) {

    // Initialization error:
    // show MAGENTA LED.

    pixels.setPixelColor(
      0,
      pixels.Color(
        255,
        0,
        255
      )
    );

    pixels.show();

    return;
  }


  // Receiver both receives tally
  // and sends acknowledgements.

  esp_now_set_self_role(
    ESP_NOW_ROLE_COMBO
  );

  esp_now_register_recv_cb(
    OnDataRecv
  );
}


// ======================================================
// LOOP
// ======================================================

void loop() {

  unsigned long now =
    millis();


  // ----------------------------------------------------
  // MASTER SIGNAL TIMEOUT
  // ----------------------------------------------------

  if (
    !signalLost &&
    (now - lastPacket) >
    SIGNAL_TIMEOUT_MS
  ) {

    signalLost = true;


    // Fail-safe:
    // turn tally LED off.

    pixels.setPixelColor(
      0,
      0
    );

    pixels.show();
  }


  // ----------------------------------------------------
  // PERIODIC ACK
  // ----------------------------------------------------

  if (
    !signalLost &&
    masterKnown &&
    (now - lastAck) >=
    ACK_INTERVAL_MS
  ) {

    sendAck();
  }


  yield();
}
