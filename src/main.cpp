#include <Arduino.h>
#include <WifiConfig.hpp>
#include <SerialHandler.hpp>
#include <BluetoothA2DPSink.h>
#include <Audio.h>
#include <OneButton.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <arduino-timer.h>
#include <time.h>
#include "defines.hpp"
#include "secrets.h"

RTC_NOINIT_ATTR int mode;
RTC_NOINIT_ATTR int station;

bool debug = true;
WifiConfig wifiConfig(WIFI_SSID, WIFI_PASSWORD, "ESP32 Kitchen-Radio", "kitchen-radio", AUTH_USER, AUTH_PASS, true, true, debug);
BluetoothA2DPSink a2dp_sink;
Audio *audio = nullptr;
Timer<1> timer;
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
OneButton whiteBtn(WHITE_BTN_PIN);
OneButton redBtn(RED_BTN_PIN);
OneButton blackBtn(BLACK_BTN_PIN);
char stationName[10];
SavedIntConfig *volumeConfig = nullptr;
int pirState = 1;

void switchModeTo(int);
void setupWebserver();
void setupAudio();
void setupButtons();
void updateDisplay();
void serialCb(String);
void metadataCb(uint8_t, const uint8_t*);

void setup() {
  if (debug) { Serial.begin(115200); delay(10); }
  // set mode to default if not coming from deep sleep/reset
  esp_reset_reason_t reason = esp_reset_reason();
  if ((reason != ESP_RST_DEEPSLEEP) && (reason != ESP_RST_SW)) {
    mode = LINE_IN;
    station = 0;
  }

  // setup relay pins
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT); // relay 2 is not switched yet
  pinMode(PIR_PIN, INPUT);
  configTzTime(CLOCK_TIMEZONE, NTP_SERVER);
  // setup display
  Wire.begin();
  if (display.begin(SCREEN_ADDRESS, true)) {
    if (debug) Serial.println(F("SH110X started"));
    display.setRotation(2);
    updateDisplay();
  }

  wifiConfig.getConfig()
    .add("volume", DEFAULT_VOLUME, [](int value) {
      int volume = value;
      if (volume < 0) volume = 0;
      if (volume > 21) volume = 21;
      if (audio) audio->setVolume(volume);
    })
    .addJson("stations", DEFAULT_STATIONS, nullptr, 1024)
  ;

  wifiConfig.begin();
  volumeConfig = wifiConfig.getConfig().getInt("volume");
  setupAudio();
  setupButtons();
  setupWebserver();
  // update display every second
  timer.every(1000, [](void*) -> bool { updateDisplay(); return true; });
}

void loop() {
  pirState = digitalRead(PIR_PIN);
  wifiConfig.loop();
  whiteBtn.tick();
  redBtn.tick();
  blackBtn.tick();
  timer.tick();
  if (audio) audio->loop();
  handleSerial(debug, serialCb);

  if (wifiConfig.isWifiConnected() && audio && audio->isRunning() == false) {
    JsonVariantConst stationsList = wifiConfig.getConfig().getJson("stations")->getJsonVal();
    JsonArrayConst arr = stationsList.as<JsonArrayConst>();
    if (arr.size() > 0) {
      if (station >= arr.size()) station = 0;
      audio->connecttohost(arr[station].as<const char*>());
      if (debug) Serial.printf("Playing  %s\n", arr[station].as<const char*>());
    }
  }

  switch(mode) {
    case LINE_IN:
      delay(15);
    case BT_SINK:
      delay(10);
    default:
      delay(1);
  }
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

  if (newMode < 0) newMode = 2;
  mode = newMode % 3;
  ESP.deepSleep(SLEEP_TIME);
}

void setupWebserver() {
  WebServer* server = wifiConfig.getServer();
  server->on("/play", HTTP_POST, [server]() {
    if (server->hasArg("plain") == false) return;

    String body = server->arg("plain");
    StaticJsonDocument<512> json;
    deserializeJson(json, body);
    JsonObject obj = json.as<JsonObject>();
    StaticJsonDocument<128> responseJson;

    if (mode != WEB_RADIO) {
      responseJson["error"] = "Not in web radio mode";
    } else {
      if (obj.containsKey("url")) {
        String url = obj["url"].as<String>();
        audio->connecttohost(url.c_str());
        if (debug) Serial.printf("Playing  %s\n", url.c_str());
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
    digitalWrite(RELAY_1_PIN, HIGH);
    i2s_pin_config_t my_pin_config = {
      .bck_io_num = I2S_BCK_PIN,
      .ws_io_num = I2S_WS_PIN,
      .data_out_num = I2S_DOUT_PIN,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    a2dp_sink.set_pin_config(my_pin_config);
    a2dp_sink.activate_pin_code(false);
    a2dp_sink.set_auto_reconnect(false);
    a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE);
    a2dp_sink.set_avrc_metadata_callback(metadataCb);
    a2dp_sink.start(wifiConfig.getConfig().get("name")->getValue().c_str());
  } else if (mode == WEB_RADIO) {
    if (debug) Serial.println(F("Starting WebRadio"));
    digitalWrite(RELAY_1_PIN, HIGH);
    audio = new Audio();
    audio->setPinout(I2S_BCK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
    audio->setVolume(volumeConfig->getIntVal());
  }
}

void setupButtons() {
  whiteBtn.attachClick([]() {
    if (mode == BT_SINK) {
      switchModeTo(LINE_IN);
    } else {
      switchModeTo((mode + 1) % 2);
    }
  });
  redBtn.attachClick([]() {
    if (mode == LINE_IN) {
      switchModeTo(BT_SINK);
    }
    if (mode == WEB_RADIO) {
      if (debug) Serial.println(F("Switching channel.."));
      station++;
      JsonVariantConst stationsList = wifiConfig.getConfig().getJson("stations")->getJsonVal();
      JsonArrayConst arr = stationsList.as<JsonArrayConst>();
      if (station >= arr.size()) station = 0;
      if (audio) {
        audio->stopSong();
        audio->connecttohost(arr[station].as<const char*>());
        if (debug) Serial.printf("Playing  %s\n", arr[station].as<const char*>());
      }
    }
  });
  redBtn.attachDoubleClick([]() {
    if (mode == WEB_RADIO) {
      if (debug) Serial.println(F("Switching channel.."));
      station--;
      JsonVariantConst stationsList = wifiConfig.getConfig().getJson("stations")->getJsonVal();
      JsonArrayConst arr = stationsList.as<JsonArrayConst>();
      if (station < 0) station = arr.size() - 1;
      if (audio) {
        audio->stopSong();
        audio->connecttohost(arr[station].as<const char*>());
        if (debug) Serial.printf("Playing  %s\n", arr[station].as<const char*>());
      }
    }
  });
  blackBtn.attachClick([]() {
    if (mode == WEB_RADIO) {
      if (volumeConfig->getIntVal() < 21) volumeConfig->setValue(volumeConfig->getIntVal() + 1);
    }
  });
  blackBtn.attachDoubleClick([]() {
    if (mode == WEB_RADIO) {
      if (volumeConfig->getIntVal() > 0) volumeConfig->setValue(volumeConfig->getIntVal() - 1);
    }
  });
}

void updateDisplay() {
  struct tm timeinfo;
  display.clearDisplay();
  if (pirState == 0) {
    display.display();
    return;
  }

  if (wifiConfig.isWifiConnected()) getLocalTime(&timeinfo);

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SH110X_WHITE);        // Draw white text
  display.setCursor(0,0);
  switch (mode) {
    case LINE_IN:
      display.println("Line In");
      break;
    case WEB_RADIO:
      display.print("Radio - Vol: ");
      if (volumeConfig) display.print(volumeConfig->getValue());
      else display.print("-");
      display.println("/21");
      break;
    case BT_SINK:
      display.print("BlueTooth - ");
      if (a2dp_sink.is_connected()) display.println("*");
      else display.println();
      break;
  }

  display.setCursor(0, 14);
  if (mode != LINE_IN) {
    display.setTextSize(2);
    display.println(stationName);
    display.setCursor(0, 44);
  } else {
    display.setTextSize(3);
  }
  if (wifiConfig.isWifiConnected()) display.print(&timeinfo, "%H:%M");
  else display.print("--:--");
  if (mode != LINE_IN) {
    display.setCursor(60, 48);
    display.setTextSize(1);
  } else {
    display.setCursor(90, 18);
    display.setTextSize(2);
  }
  if (wifiConfig.isWifiConnected()) display.println(&timeinfo, ":%S");
  else display.println(":--");

  display.display();
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

void metadataCb(uint8_t type, const uint8_t *data) {
  if (type == ESP_AVRC_MD_ATTR_TITLE) {
    if (debug) { Serial.print(F("title       "));Serial.println((char*)data); }
    strncpy(stationName, (char*)data, 10);
    updateDisplay();
  }
}

void audio_info(const char *info){
  if (debug) { Serial.print(F("info        "));Serial.println(info); }
}

void audio_showstation(const char *info){
  if (debug) { Serial.print(F("station     "));Serial.println(info); }
  strncpy(stationName, info, 10);
  updateDisplay();
}
