#include <Arduino.h>
#include <WifiConfig.hpp>
#include <SerialHandler.hpp>
#include <OneButton.h>
#include "BluetoothA2DPSink.h"
#include "secrets.h"

bool debug = true;
WifiConfig wifiConfig(WIFI_SSID, WIFI_PASSWORD, "ESP32 Kitchen-Radio", "kitchen-radio", AUTH_USER, AUTH_PASS, true, true, debug);
Configuration outputs("/outputs", debug);
BluetoothA2DPSink a2dp_sink;

void serialCb(String);

bool btMode = false;
OneButton modeSw(GPIO_NUM_5);

void setup() {
  if (debug) {
    Serial.begin(115200);
    delay(10);
  }

  i2s_pin_config_t my_pin_config = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_out_num = 17,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  a2dp_sink.set_pin_config(my_pin_config);

  modeSw.attachClick([]() {
    btMode = !btMode;
    if (btMode) {
      if (debug) Serial.println("Switching to BT mode");
      a2dp_sink.start(wifiConfig.getConfig().get("name")->getValue().c_str());
    } else {
      if (debug) Serial.println("Switching to Wifi mode");
      a2dp_sink.end();
    }
  });

  pinMode(GPIO_NUM_13, OUTPUT);

  outputs.add("13", LOW, [](int value) {
    digitalWrite(GPIO_NUM_13, value);
  });

  wifiConfig.registerConfigApi(outputs);
  wifiConfig.setup();
}

void loop() {
  wifiConfig.loop();
  modeSw.tick();
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
