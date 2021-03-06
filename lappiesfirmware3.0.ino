//including the libraries
#include <RTClib.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <SPIFFS.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

//defining all the pins
#define ONE_WIRE_BUS 27
#define DHTPIN 25
#define CS_PIN 5
#define relayPin 4

#define DHTTYPE DHT22                //defining the right sensor (we are working with the DHT22)
AsyncWebServer server(80);           //creating a webserver instance
DHT dht(DHTPIN, DHTTYPE);            //creating an instance of DHT
OneWire oneWire(ONE_WIRE_BUS);       // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature.
RTC_DS3231 rtc;                      //creating an instance of rtc
hw_timer_t *timer = NULL;            //declare timer variable
//global variables
bool state = 0;
bool prevstate = 0;
String dateString = "";
String dataString = "";
String JSONString = "";
int numberOfDevices;
int Year;
int Month;
int Day;
int Hour;
int Minute;
int interval = 1;
int standardRoutineTimer = 0;
int oldFilesTimer = 0;
int pumpTimer = 0;
int daysTillDelete = 30;
DeviceAddress tempDeviceAddress;
//network credentials
const char *ssid;
const char *password;
const char *hostname;
//webinterface inputs
const char *PARAM_INPUT_1;
const char *PARAM_INPUT_2;
const char *PARAM_INPUT_3;
const char *PARAM_INPUT_4;
const char *PARAM_INPUT_5;
const char *PARAM_INPUT_6;
const char *PARAM_INPUT_7;
const char *PARAM_INPUT_8;

String IM1;
String IM2;
String IM3;
String IP1;
String IP2;
String IP3;
String style = "<style>*{font-family: Arial, Helvetica, sans-serif;padding: 70px 0;text-align: center;font-size: large;}</style>";

//files
File root;

StaticJsonDocument<555> jsonDoc;
JsonArray measurements;
String logInstance = "[";
String nameOfFile;
String rootpath = "/";

struct Config {
  int measureInterval;
  int daysTillDelete;
  char ssid[64];
  char password[64];
  char hostname[64];
};
bool restart;

const char *filename = "/config.txt";
Config config;

//all prototypes
void printAddress(DeviceAddress deviceAddress);
void initializeSPIFFS();
void initializeConfig();
void initializeTimer();
void initializeRTC();
void initializeSensors();
void initializeSD();
void initializeDashboard();
String measureDallasTemp(char index);
String measureDHThum();
String measureDHTtemp();
void getMeasurements();
String generateFileName();
void logDate();
void appendFile(String path, String message);
void deleteOldFiles();
String handleRoot();
void handleNotFound(AsyncWebServerRequest *request);
void downloadLog(AsyncWebServerRequest *request, String filename);
String mostRecentFile();
void setupJSON(String epoc);
void registerMeasurement(String key, String value);
void deleteSD();
void deleteFile(String path);

void pumpCheck();
//creating struct for pumpPeriods
struct period {
  String beginTime;
  String endTime;
  int beginMinutes;
  int beginHour;
  int endMinutes;
  int endHour;
};
struct period pumpPeriod_1;
struct period pumpPeriod_2;
struct period pumpPeriod_3;
struct period pumpPeriod_4;
int pp11;
int pp12;
int pp21;
int pp22;
int pp31;
int pp32;
int pp41;
int pp42;
bool turnPumpOn[3600];

void loadConfiguration(const char *filename, Config &config);
void saveConfiguration(const char *filename, const Config &config);
void printFile(const char *filename);

void IRAM_ATTR onTimer()
{
  standardRoutineTimer++;
  oldFilesTimer++;
  pumpTimer++;
}

void setup()
{
  pinMode(relayPin, OUTPUT);
  // start serial port
  Serial.begin(115200);
  initializeSPIFFS();
  initializeConfig();
  initializeTimer();
  initializeRTC();
  initializeSensors();
  initializeSD();
  initializeDashboard();
  nameOfFile = generateFileName();
}
void loop()
{
  if (standardRoutineTimer == interval)
  {
    logDate();
    getMeasurements();
    appendFile(nameOfFile, logInstance);
    standardRoutineTimer = 0;
    logInstance = "";
    jsonDoc.clear();

  }
  if (oldFilesTimer == 60)
  {
    deleteOldFiles();
    oldFilesTimer = 0;
  }

    pumpCheck();
  

}

//all the methods and functions
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

//all the initialize methods
//initialize file system
void initializeSPIFFS()
{
  if (!SPIFFS.begin())
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
}

//initialize config file
void initializeConfig()
{
  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename, config);
  ssid = config.ssid;
  password = config.password;
  hostname = config.hostname;
  interval = config.measureInterval;
  Serial.println(ssid);
  Serial.println(password);
  Serial.println(hostname);
}

//initialize timer
void initializeTimer()
{
  Serial.println("start timer");
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 60000000, true);
  timerAlarmEnable(timer);
  Serial.println("Timer is initialized");
}

//initialize all the sensors
void initializeSensors()
{
  // Start up the library's
  sensors.begin();
  dht.begin();
  // Grab a count of devices on the wire (watch out, pragram stops working if you connect a device without adress)
  numberOfDevices = sensors.getDeviceCount();

  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
    }
    else
    {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }
  Serial.println("Sensors are initialized");
}

//initialize SD-module
void initializeSD()
{
 
  if (!SD.begin(CS_PIN))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("SD is initialized");
}

//initialize RTC-module
void initializeRTC()
{
  rtc.begin();
  delay(1000);
  if (!rtc.begin())
  {
    Serial.println("could not find RTC please check the wiring");
    return;
  }
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println("RTC is initialized");
  Serial.println("The current date and time is: ");
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print("\t");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
}

//webserver responses
void initializeDashboard()
{
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  WiFi.softAP(ssid, password);
  WiFi.setHostname("node1");
  WebSerial.begin(&server);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  WebSerial.print("AP IP address: ");
  WebSerial.println(IP);

  //use IP or iotsharing.local to access webserver
  if (MDNS.begin("lappies"))
  {
    Serial.println("MDNS responder started");
  }
  MDNS.addService("http", "tcp", 80);

  server.on("/logfiles", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/html", handleRoot());
  });
  server.onNotFound(handleNotFound);
  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
  });
  server.on("/Wtemperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", measureDallasTemp(0).c_str());
  });
  server.on("/Rtemperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", measureDallasTemp(1).c_str());
  });
  server.on("/Atemperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", measureDHTtemp().c_str());
  });
  server.on("/Ahumidity", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", measureDHThum().c_str());
  });

  server.on("/systemsettings", HTTP_GET, [](AsyncWebServerRequest * request) {
    PARAM_INPUT_1 = "dateTime";
    PARAM_INPUT_2 = "measureInterval";
    PARAM_INPUT_3 = "ssid";
    PARAM_INPUT_4 = "password";
    PARAM_INPUT_5 = "hostname";
    PARAM_INPUT_6 = "daysTillDelete";

    if (request->hasParam(PARAM_INPUT_1))
    {
      IM1 = request->getParam(PARAM_INPUT_1)->value();
      if (IM1 != "") {
        Year = IM1.substring(0, 4).toInt();
        Month = IM1.substring(5, 7).toInt();
        Day = IM1.substring(8, 10).toInt();
        Hour = IM1.substring(11, 13).toInt();
        Minute = IM1.substring(14).toInt();
        rtc.adjust(DateTime(Year, Month, Day, Hour, Minute, 0));
        Serial.println("Time Was Set to: ");
        Serial.println(IM1);
        WebSerial.println("Time Was Set to: ");
        WebSerial.println(IM1);
      }
    }
    else
    {
      IM1 = "";
    }
    if (request->hasParam(PARAM_INPUT_2))
    {
      IM2 = request->getParam(PARAM_INPUT_2)->value();



      if (IM2 != "") {
        Serial.println(IM2);
        interval = IM2.toInt();
        standardRoutineTimer = 0;
      }

    }
    else
    {
      IM2 = "";
    }
    if ((request->hasParam(PARAM_INPUT_3)) || (request->hasParam(PARAM_INPUT_4)))
    {
      if (request->getParam(PARAM_INPUT_3)->value() != "") {
        String ssidTemp = request->getParam(PARAM_INPUT_3)->value();
        strcpy(config.ssid, ssidTemp.c_str());
        saveConfiguration(filename, config);
        restart = true;
      }
      if (request->getParam(PARAM_INPUT_4)->value() != "") {
        String passwTemp = request->getParam(PARAM_INPUT_4)->value();
        strcpy(config.password, passwTemp.c_str());
        saveConfiguration(filename, config);
        restart = true;
      }
      if (restart) {
        request->redirect("/");
        ESP.restart();
        restart = false;
      }
    }
    else
    {
      IM2 = "";
    }
    if (request->hasParam(PARAM_INPUT_6)) {
      if (request->getParam(PARAM_INPUT_6)->value() != "") {
        daysTillDelete = request->getParam(PARAM_INPUT_6)->value().toInt();
        Serial.println(daysTillDelete);
      }
    }
    request->send(200, "text/html", style + "<h1>Setting were changed!</h1><br><a href=\"/\">Return to Home Page</a>");
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest * request) {
    PARAM_INPUT_1 = "syncData";
    PARAM_INPUT_2 = "viewLogs";
    PARAM_INPUT_3 = "delLogs";
    if (request->hasParam(PARAM_INPUT_1))
    {
      IM1 = request->getParam(PARAM_INPUT_1)->value();
      IP1 = PARAM_INPUT_1;
      logDate();
      getMeasurements();
      logInstance.remove(logInstance.length() - 1, 1);
      logInstance += "]";
      appendFile(nameOfFile, logInstance);
      downloadLog(request, mostRecentFile());
      logInstance = "[";
      nameOfFile = generateFileName();
      appendFile(nameOfFile, logInstance);
      Serial.println(IM1);
      request->redirect("/");
    }
    else
    {
      IM1 = "";
      IP1 = "";
    }
    if (request->hasParam(PARAM_INPUT_2))
    {
      request->redirect("\logfiles");
    }
    else
    {
      IM2 = "";
      IP2 = "";
    }
    if (request->hasParam(PARAM_INPUT_3))
    {
      IM3 = request->getParam(PARAM_INPUT_3)->value();
      IP3 = PARAM_INPUT_3;
      deleteSD();
      logInstance = "[";
      request->redirect("/");
    }
    else
    {
      IM3 = "";
      IP3 = "";
    }
  });
  server.on("/pumpsettings", HTTP_GET, [](AsyncWebServerRequest * request) {
    PARAM_INPUT_1 = "pumpPeriod11";
    PARAM_INPUT_2 = "pumpPeriod12";
    PARAM_INPUT_3 = "pumpPeriod21";
    PARAM_INPUT_4 = "pumpPeriod22";
    PARAM_INPUT_5 = "pumpPeriod31";
    PARAM_INPUT_6 = "pumpPeriod32";
    PARAM_INPUT_7 = "pumpPeriod41";
    PARAM_INPUT_8 = "pumpPeriod42";
    //pump period 1
    if (request->hasParam(PARAM_INPUT_1))
    {
      pumpPeriod_1.beginTime = request->getParam(PARAM_INPUT_1)->value();
      pumpPeriod_1.beginHour = pumpPeriod_1.beginTime.substring(0, 2).toInt();
      pumpPeriod_1.beginMinutes = pumpPeriod_1.beginTime.substring(3).toInt();
    }
    else
    {
      pumpPeriod_1.beginTime = "";
    }
    if (request->hasParam(PARAM_INPUT_2))
    {
      pumpPeriod_1.endTime = request->getParam(PARAM_INPUT_2)->value();
      pumpPeriod_1.endHour = pumpPeriod_1.endTime.substring(0, 2).toInt();
      pumpPeriod_1.endMinutes = pumpPeriod_1.endTime.substring(3).toInt();
    }
    else
    {
      pumpPeriod_1.endTime = "";
    }
    //pump period 2
    if (request->hasParam(PARAM_INPUT_3))
    {
      pumpPeriod_2.beginTime = request->getParam(PARAM_INPUT_3)->value();
      pumpPeriod_2.beginHour = pumpPeriod_2.beginTime.substring(0, 2).toInt();
      pumpPeriod_2.beginMinutes = pumpPeriod_2.beginTime.substring(3).toInt();
    }
    else
    {
      pumpPeriod_2.beginTime = "";
    }
    if (request->hasParam(PARAM_INPUT_4))
    {
      pumpPeriod_2.endTime = request->getParam(PARAM_INPUT_4)->value();
      pumpPeriod_2.endHour = pumpPeriod_2.endTime.substring(0, 2).toInt();
      pumpPeriod_2.endMinutes = pumpPeriod_2.endTime.substring(3).toInt();
    }
    else
    {
      pumpPeriod_2.endTime = "";
    }
    //pump period 3
    if (request->hasParam(PARAM_INPUT_5))
    {
      pumpPeriod_3.beginTime = request->getParam(PARAM_INPUT_5)->value();
      pumpPeriod_3.beginHour = pumpPeriod_3.beginTime.substring(0, 2).toInt();
      pumpPeriod_3.beginMinutes = pumpPeriod_3.beginTime.substring(3).toInt();
    }
    else
    {
      pumpPeriod_3.beginTime = "";
    }
    if (request->hasParam(PARAM_INPUT_6))
    {
      pumpPeriod_3.endTime = request->getParam(PARAM_INPUT_6)->value();
      pumpPeriod_3.endHour = pumpPeriod_3.endTime.substring(0, 2).toInt();
      pumpPeriod_3.endMinutes = pumpPeriod_3.endTime.substring(3).toInt();
    }
    else
    {
      pumpPeriod_3.endTime = "";
    }
    //pump period 4
    if (request->hasParam(PARAM_INPUT_7))
    {
      pumpPeriod_4.beginTime = request->getParam(PARAM_INPUT_7)->value();
      pumpPeriod_4.beginHour = pumpPeriod_4.beginTime.substring(0, 2).toInt();
      pumpPeriod_4.beginMinutes = pumpPeriod_4.beginTime.substring(3).toInt();
    }
    else
    {
      pumpPeriod_4.beginTime = "";
    }
    if (request->hasParam(PARAM_INPUT_8))
    {
      pumpPeriod_4.endTime = request->getParam(PARAM_INPUT_8)->value();
      pumpPeriod_4.endHour = pumpPeriod_4.endTime.substring(0, 2).toInt();
      pumpPeriod_4.endMinutes = pumpPeriod_4.endTime.substring(3).toInt();
    }
    else
    {
      pumpPeriod_4.endTime = "";
    }
    for (int i = 0; i <= 3600; i++) {
      turnPumpOn[i] = false;
    }
    request->send(200, "text/html", style + "<h1>Setting were changed!</h1><br><a href=\"/\">Return to Home Page</a>");

  });
  server.begin();
}

int minutesSinceMidnight(String input) {
  return input.substring(0, 2).toInt() * 60 + input.substring(3).toInt();
}

//check if the pump needs to be turned on
void pumpCheck() {
  DateTime now = rtc.now();
  char buf2[] = "hh:mm";

  bool state;


  for (int i = 0; i <= 3600; i++) {
    turnPumpOn[i] = false;
  }

  if (pumpPeriod_1.beginTime != "" || pumpPeriod_1.endTime != "") {
    pp11 = minutesSinceMidnight(pumpPeriod_1.beginTime);
    pp12 = minutesSinceMidnight(pumpPeriod_1.endTime);
    for (int i = pp11; i <= pp12; i++) {
      turnPumpOn[i] = true;
    }

  }

  if (pumpPeriod_2.beginTime != "" || pumpPeriod_2.endTime != "") {
    pp21 = minutesSinceMidnight(pumpPeriod_2.beginTime);
    pp22 = minutesSinceMidnight(pumpPeriod_2.endTime);
    for (int i = pp21; i <= pp22; i++) {
      turnPumpOn[i] = true;
    }
  }

  if (pumpPeriod_3.beginTime != "" || pumpPeriod_3.endTime != "") {
    pp31 = minutesSinceMidnight(pumpPeriod_3.beginTime);
    pp32 = minutesSinceMidnight(pumpPeriod_3.endTime);
    for (int i = pp31; i <= pp32; i++) {
      turnPumpOn[i] = true;
    }
  }

  if (pumpPeriod_4.beginTime != "" || pumpPeriod_4.endTime != "") {
    pp41 = minutesSinceMidnight(pumpPeriod_4.beginTime);
    pp42 = minutesSinceMidnight(pumpPeriod_4.endTime);
    for (int i = pp41; i <= pp42; i++) {
      turnPumpOn[i] = true;
    }
  }
  state = turnPumpOn[minutesSinceMidnight(now.toString(buf2))];

  digitalWrite(relayPin, state);

}

//these functions are used to measure the temperatures and currents

//measure temperature from probe
String measureDallasTemp(char index)
{
  char object;
  float temp;
  sensors.requestTemperatures(); // Send the command to get temperatures

  if (sensors.getAddress(tempDeviceAddress, index))
  {
    if (index == 0)
    {
      temp = sensors.getTempC(tempDeviceAddress);
    }
    else
    {
      temp = sensors.getTempC(tempDeviceAddress);
    }
    return String(temp);
  }
}

//measure ambient humidity
String measureDHThum()
{
  float humidity = dht.readHumidity();
  String hum;
  hum = String(humidity);
  return hum;
}

//measure ambient temperature
String measureDHTtemp()
{
  float temperature = dht.readTemperature();
  String temp;
  temp = String(temperature);
  return temp;
}

//get measurements
void getMeasurements()
{
  registerMeasurement("Twater", measureDallasTemp(0));
  registerMeasurement("Troot", measureDallasTemp(1));
  registerMeasurement("Tambient", measureDHTtemp());
  registerMeasurement("Hambient", measureDHThum());
  serializeJsonPretty(jsonDoc, logInstance);
  logInstance += ",";
}
//these functions are used to write data on the SD-card
String generateFileName()
{
  DateTime now = rtc.now();
  String generatedFileName = "/";
  generatedFileName += String(now.unixtime());
  generatedFileName += ".txt";
  return generatedFileName;
}

//append log file
void appendFile(String path, String message)
{
  
  File SDlog = SD.open(path, FILE_APPEND);
  Serial.print("Appending to file: ");
  Serial.println(String(SDlog.name()));
  WebSerial.print("Appending to file: ");
  WebSerial.println(String(SDlog.name()));

  if (!SDlog)
  {
    Serial.println("Failed to open file for appending");
    WebSerial.println("Failed to open file for appending");
    return;
  }
  if (SDlog.println(message))
  {
    Serial.println("Message appended");
    WebSerial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
    WebSerial.println("Append failed");
  }
  SDlog.close();
}

//delete log file
void deleteFile(String path) {
  Serial.printf("Deleting file: %s\n", path);
  if (SD.remove(path)) {
    Serial.println("File deleted");
    WebSerial.println("File deleted");
  } else {
    Serial.println("Delete failed");
    WebSerial.println("Delete failed");
  }
 // SD.close(path);
}

//delete all log files
void deleteSD() {
  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    WebSerial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    WebSerial.println("Not a directory");

    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      Serial.println(file.name());
      WebSerial.println(file.name());

      deleteFile(file.name());
    }
    file = root.openNextFile();
  }
  root.close();
}

//deletes files that are older than daysTillDelete
void deleteOldFiles() {
  String filename;
  DateTime now = rtc.now();
  File root = SD.open("/");
  if (!root) {
    return;
  }
  if (!root.isDirectory()) {
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      filename = file.name();
      filename = filename.substring(1, filename.length() - 4);
      Serial.println(filename);
      WebSerial.println(filename);

      if ((now.unixtime() - filename.toInt()) > (86400 * daysTillDelete)) {
        if (SD.remove(file.name())) {
          Serial.print(file.name());
          Serial.println(" deleted");
          WebSerial.print(file.name());
          WebSerial.println(" deleted");
        } else {
          Serial.println("Delete failed");
          WebSerial.println("Delete failed");

        }
      }
    }
    file = root.openNextFile();
  }
  root.close();
}


void logDate()
{
  //creating a datetime variable
  DateTime now = rtc.now();
  char buf2[] = "YY/MM/DD-hh:mm:ss";
  Serial.println(now.toString(buf2));
  WebSerial.println(now.toString(buf2));
  setupJSON(String(now.unixtime()));
}

//webinterface methods and functions
void notFound(AsyncWebServerRequest * request)
{
  request->send(404, "text/plain", "Not found");
}

//the following functions are used to print the log files
//on the SD-card to the webserver
String printDirectory(File dir, int numTabs)
{
  String response = "";
  dir.rewindDirectory();

  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++)
    {
      Serial.print('\t'); // we'll have a nice indentation
    }
    // Recurse for directories, otherwise print the file size
    if (entry.isDirectory())
    {
      printDirectory(entry, numTabs + 1);
    }
    else
    {
      response += String("<a href='") + String(entry.name()) + String("' download>") + String(entry.name()) + String("</a>") + String("</br>");
    }
    entry.close();
  }
  return style + String("<h1>Log Files:</h1></br>") + response;
}

String handleRoot()
{
  root = SD.open("/");
  String res = printDirectory(root, 0);
  root.close();
  return res;
}

bool loadFromSdCard(AsyncWebServerRequest * request)
{
  String path = request->url();
  String dataType = "text/plain";
  struct fileBlk
  {
    File dataFile;
  };
  fileBlk *fileObj = new fileBlk;

  if (path.endsWith("/"))
    path += "index.htm";
  if (path.endsWith(".src"))
    path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".htm"))
    dataType = "text/html";
  else if (path.endsWith(".css"))
    dataType = "text/css";
  else if (path.endsWith(".js"))
    dataType = "application/javascript";
  else if (path.endsWith(".png"))
    dataType = "image/png";
  else if (path.endsWith(".gif"))
    dataType = "image/gif";
  else if (path.endsWith(".jpg"))
    dataType = "image/jpeg";
  else if (path.endsWith(".ico"))
    dataType = "image/x-icon";
  else if (path.endsWith(".xml"))
    dataType = "text/xml";
  else if (path.endsWith(".pdf"))
    dataType = "application/pdf";
  else if (path.endsWith(".zip"))
    dataType = "application/zip";

  fileObj->dataFile = SD.open(path.c_str());
  if (fileObj->dataFile.isDirectory())
  {
    path += "/index.htm";
    dataType = "text/html";
    fileObj->dataFile = SD.open(path.c_str());
  }

  if (!fileObj->dataFile)
  {
    delete fileObj;
    return false;
  }

  if (request->hasParam("download"))
    dataType = "application/octet-stream";

  request->_tempObject = (void *)fileObj;
  request->send(dataType, fileObj->dataFile.size(), [request](uint8_t *buffer, size_t maxlen, size_t index) -> size_t {
    fileBlk *fileObj = (fileBlk *)request->_tempObject;
    size_t thisSize = fileObj->dataFile.read(buffer, maxlen);
    if ((index + thisSize) >= fileObj->dataFile.size())
    {
      fileObj->dataFile.close();
      request->_tempObject = NULL;
      delete fileObj;
    }
    return thisSize;
  });
//  dataFile.close();
  return true;
}

void handleNotFound(AsyncWebServerRequest * request)
{
  String path = request->url();
  if (path.endsWith("/"))
    path += "index.htm";
  if (loadFromSdCard(request))
  {
    return;
  }
  String message = "\nNo Handler\r\n";
  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (request->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nParameters: ";
  message += request->params();
  message += "\n";
  for (uint8_t i = 0; i < request->params(); i++)
  {
    AsyncWebParameter *p = request->getParam(i);
    message += String(p->name().c_str()) + " : " + String(p->value().c_str()) + "\r\n";
  }
  request->send(404, "text/plain", message);
  Serial.print(message);
  WebSerial.print(message);
}

//JSON functions
void registerMeasurement(String key, String value)
{
  JsonObject o = measurements.createNestedObject();
  o["key"] = key;
  o["value"] = value;
}

void setupJSON(String epoc)
{
  jsonDoc["timestamp"] = epoc;
  measurements = jsonDoc.createNestedArray("measurements");
}

//returns most recent log file
String mostRecentFile() {
  File root = SD.open("/");
  File file = root.openNextFile();
  String mostRecent;
  while (file) {
    if (!file.isDirectory()) {
      mostRecent = file.name();
    }
    file = root.openNextFile();
  }
  Serial.print("Most Recent Log File: ");
  Serial.println(mostRecent);
  WebSerial.print("Most Recent Log File: ");
  WebSerial.println(mostRecent);
  return mostRecent;
  root.close();
}

void downloadLog(AsyncWebServerRequest * request, String filename) {
  String path = filename;
  String dataType = "text/plain";
  struct fileBlk
  {
    File dataFile;
  };
  fileBlk *fileObj = new fileBlk;

  if (path.endsWith("/"))
    path += "index.htm";

  fileObj->dataFile = SD.open(path.c_str());
  if (fileObj->dataFile.isDirectory())
  {
    path += "/index.htm";
    dataType = "text/html";
    fileObj->dataFile = SD.open(path.c_str());
  }

  if (!fileObj->dataFile)
  {
    delete fileObj;
  }

  if (request->hasParam("download"))
    dataType = "application/octet-stream";

  request->_tempObject = (void *)fileObj;
  request->send(dataType, fileObj->dataFile.size(), [request](uint8_t *buffer, size_t maxlen, size_t index) -> size_t {
    fileBlk *fileObj = (fileBlk *)request->_tempObject;
    size_t thisSize = fileObj->dataFile.read(buffer, maxlen);
    if ((index + thisSize) >= fileObj->dataFile.size())
    {
      fileObj->dataFile.close();
      request->_tempObject = NULL;
      delete fileObj;
    }
  });
 // dataFile.close();
}

// Loads the configuration from a file
void loadConfiguration(const char *filename, Config & config) {
  File file = SPIFFS.open(filename);
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.println(F("Failed to read file, using default configuration"));
    WebSerial.println(F("Failed to read file, using default configuration"));
  }
  config.measureInterval = doc["measureInterval"] | 15;
  config.daysTillDelete = doc["daysTillDelete"] | 30;
  strlcpy(config.ssid,                      // <- destination
          doc["ssid"] | "Lappies",          // <- source
          sizeof(config.ssid));             // <- destination's capacity
  strlcpy(config.password,                  // <- destination
          doc["password"] | "12345678",      // <- source
          sizeof(config.password));         // <- destination's capacity
  strlcpy(config.hostname,                  // <- destination
          doc["hostname"] | "lappies",      // <- source
          sizeof(config.hostname));         // <- destination's capacity
  file.close();
}

// Saves the configuration to a file
void saveConfiguration(const char *filename, const Config & config) {
  SPIFFS.remove(filename);
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    WebSerial.println(F("Failed to create file"));
    return;
  }

  StaticJsonDocument<256> doc;

  doc["measureInterval"] = config.measureInterval;
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["hostname"] = config.hostname;

  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
    WebSerial.println(F("Failed to write to file"));
  }

  file.close();
}
