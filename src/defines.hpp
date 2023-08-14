#ifndef defines_hpp
#define defines_hpp

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C
#define SCREEN_UPDATE_INTERVAL 1000
#define NTP_UPDATE_INTERVAL 3600000
#define NTP_SERVER "europe.pool.ntp.org"
#define CLOCK_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
#define DEFAULT_VOLUME 5
#define DEFAULT_STATIONS "[\"http://stream.infostart.hu/lejatszo/index.html?sid=1\",\"http://radio.musorok.org/listen/jazzy/jazzy.mp3\",\"http://icast.connectmedia.hu/4736/mr1.mp3\"]"

#define SLEEP_TIME 100000

#define I2S_BCK_PIN 26
#define I2S_WS_PIN 25
#define I2S_DOUT_PIN 17
#define WHITE_BTN_PIN 18
#define RED_BTN_PIN 19
#define BLACK_BTN_PIN 23
#define RELAY_1_PIN 27
#define RELAY_2_PIN 16

enum Modes {
  LINE_IN,
  WEB_RADIO,
  BT_SINK
};

#endif
