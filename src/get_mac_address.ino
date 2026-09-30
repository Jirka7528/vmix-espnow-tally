/*
  ESP8266 MAC Address Utility

  Upload this sketch to an ESP8266 / WeMos D1 Mini
  to find its Wi-Fi station MAC address.

  Open Serial Monitor at 115200 baud.

  Copy the displayed MAC address and use it in
  the receiver configuration section of master.ino.
*/

#include <ESP8266WiFi.h>

void setup() {

  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);

  Serial.println();
  Serial.println("====================");
  Serial.println("ESP8266 MAC ADDRESS");
  Serial.println("====================");

  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.println("====================");
}

void loop() {
}
