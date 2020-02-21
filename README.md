# Lappies Firmware Version 3

Firmware that runs on the ESP32 in the central measurement unit for the Lappies Aquaponic.

## Getting Started

To correctly install this firmare, the ESP32 Filesystem Uploader tool is needed. After this, the Arduino sketch can be uploaded.

### ESP32FS tool

You can find the current releases of the ESP32FS tool [here](https://github.com/me-no-dev/arduino-esp32fs-plugin/releases/).

After this, the `index.html` for the dashboard and the `config.txt` can be uploaded using the tool.

### Libraries

* `RTClib.h`
* `Wire.h`
* `SPI.h`
* `SD.h`
* `DHT.h`
* `OneWire.h`
* `DallasTemperature.h`
* `FS.h`
* `Arduino.h`
* `WiFi.h`
* `AsyncTCP.h`
* `ESPAsyncWebServer.h`
* `WebSerial.h`
* `SPIFFS.h`
* `WiFiClient.h`
* `ESPmDNS.h`
* `ArduinoJson.h`


### Arduino Sketch

Finally the arduino sketch can get uploaded to the ESP32 using the Arduino IDE


