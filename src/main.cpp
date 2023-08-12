#include <Arduino.h>
#include <WifiConfig.hpp>
#include <SerialHandler.hpp>
#include <BluetoothA2DPSink.h>
#include <Audio.h>
#include <OneButton.h>
#include "secrets.h"

#define I2S_BCK_PIN 26
#define I2S_WS_PIN 25
#define I2S_DOUT_PIN 17

enum Modes {
  LINE_IN,
  WEB_RADIO,
  BT_SINK
};
RTC_NOINIT_ATTR int mode;

bool debug = true;
WifiConfig wifiConfig(WIFI_SSID, WIFI_PASSWORD, "ESP32 Kitchen-Radio", "kitchen-radio", AUTH_USER, AUTH_PASS, true, true, debug);
Configuration outputs("/outputs", debug);
BluetoothA2DPSink a2dp_sink;
Audio *audio = nullptr;
OneButton modeSw(GPIO_NUM_5);

void switchModeTo(int);
void setupWebserver();
void setupAudio();
void serialCb(String);

void setup() {
  if (debug) { Serial.begin(115200); delay(10); }
  esp_reset_reason_t reason = esp_reset_reason();
  if ((reason != ESP_RST_DEEPSLEEP) && (reason != ESP_RST_SW)) {
    mode = LINE_IN;
  }

  wifiConfig.setup();
  setupAudio();
  modeSw.attachClick([]() { switchModeTo(mode + 1); });
  pinMode(GPIO_NUM_13, OUTPUT);
  outputs.add("13", LOW, [](int value) { digitalWrite(GPIO_NUM_13, value); });
  if (mode == LINE_IN) wifiConfig.registerConfigApi(outputs);
  setupWebserver();
}

void loop() {
  wifiConfig.loop();
  modeSw.tick();
  if (audio) audio->loop();
  handleSerial(debug, serialCb);
}

void switchModeTo(int newMode) {
  // shutdown current mode
  switch (mode) {
    case BT_SINK:
      if (debug) Serial.println(F("Stopping BT sink"));
      a2dp_sink.stop();
      a2dp_sink.disconnect();
      a2dp_sink.end();
      break;
    case WEB_RADIO:
      if (debug) Serial.println(F("Stopping WebRadio"));
      audio->stopSong();
      delete audio;
      audio = nullptr;
      break;
  }

  mode = newMode % 3;
  ESP.restart();
}

void setupWebserver() {
  WebServer* server = wifiConfig.getServer();
  server->on("/play", HTTP_POST, [server]() {
    if (server->hasArg("plain") == false) return;

    String body = server->arg("plain");
    StaticJsonDocument<512> json;
    deserializeJson(json, body);
    JsonObject obj = json.as<JsonObject>();
    StaticJsonDocument<256> responseJson;

    if (mode != WEB_RADIO) {
      responseJson["error"] = "Not in web radio mode";
    } else {
      if (obj.containsKey("url")) {
        audio->connecttohost(obj["url"].as<String>().c_str());
        if (debug) Serial.println("Playing  " + obj["url"].as<String>());
        responseJson["status"] = "ok";
      } else {
        responseJson["error"] = "No url given";
      }
    }

    String response;
    serializeJson(responseJson, response);
    server->send(200, "application/json", response);
  });
}

void setupAudio() {
  if (mode == BT_SINK) {
    if (debug) Serial.println(F("Starting BT sink"));
    i2s_pin_config_t my_pin_config = {
      .bck_io_num = I2S_BCK_PIN,
      .ws_io_num = I2S_WS_PIN,
      .data_out_num = I2S_DOUT_PIN,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    a2dp_sink.set_pin_config(my_pin_config);
    a2dp_sink.activate_pin_code(false);
    a2dp_sink.set_auto_reconnect(false);
    a2dp_sink.start(wifiConfig.getConfig().get("name")->getValue().c_str());
  } else if (mode == WEB_RADIO) {
    if (debug) Serial.println(F("Starting WebRadio"));
    // audio = new Audio(false, 3, I2S_NUM_1);
    audio = new Audio();
    audio->setVolume(7);
    audio->setPinout(I2S_BCK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
  }
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

void audio_info(const char *info){
  if (debug) {
    Serial.print(F("info        "));
    Serial.println(info);
  }
}
