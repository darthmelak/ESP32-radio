## Webradio / A2DP sink using ESP32

ESP32-A2DP as submodule, because it wasn't published to platformio library registry.

#### PCM5102 as DAC, connection:
VIN -> 5V  
GND -> GND  
LCK -> ws_io_num (25)  
DIN -> data_out_num (17)  
BCK -> bck_io_num (26)  
SCK -> GND  
