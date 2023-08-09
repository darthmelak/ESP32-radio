#include <Arduino.h>
#include <WifiConfig.hpp>
#include <SerialHandler.hpp>
#include <OneButton.h>
#include "secrets.h"

bool debug = false;
WifiConfig wifiConfig(WIFI_SSID, WIFI_PASSWORD, "ESP32 Kitchen-Radio", "kitchen-radio", AUTH_USER, AUTH_PASS, true, true, debug);

void serialCb(String);

void setup() {
  if (debug) {
    Serial.begin(115200);
    delay(10);
  }

  wifiConfig.setup();
}

void loop() {
  wifiConfig.loop();
  handleSerial(debug, serialCb);
  delay(1);
}

void serialCb(String cmd) {
  if (cmd.equals("reset")) {
    SavedConfiguration wc = wifiConfig.getConfig();
    wc.get("ssid")->setValue(WIFI_SSID);
    wc.get("password")->setValue(WIFI_PASSWORD);
    wc.get("auth_user")->setValue(AUTH_USER);
    wc.get("auth_pass")->setValue(AUTH_PASS);
    Serial.println("Resetting wifi settings");
  }
}
