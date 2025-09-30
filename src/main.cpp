#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <time.h>

// === CONFIG WiFi FIJO ===
const char* ssid = "Galaxy A014364";       // <-- poné tu red WiFi
const char* password = "osec5490";  // <-- poné tu clave

// === SENSORES ===
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#define GAS_PIN 34
#define PIR_PIN 2
#define RAIN_PIN 35
#define SERVO_PIN 5
Servo myservo;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Teclado
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {12, 14, 27, 26};
byte colPins[COLS] = {25, 33, 32, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Datos
struct SensorData {
  float temperature;
  float humidity;
  int gasValue;
  int rainValue;
  bool motionDetected;
  String timestamp; // dd/mm/yyyy hh:mm:ss
};

SensorData currentData;
const int MAX_RECORDS = 1024;
SensorData historicalData[MAX_RECORDS];
int dataIndex = 0;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 5000;

WebServer server(80);

// === NTP ===
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; // Argentina (GMT-3)
const int daylightOffset_sec = 0;

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "00/00/0000 00:00:00";
  }
  char buf[20];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buf);
}

void saveHistoricalData() {
  historicalData[dataIndex] = currentData;
  dataIndex = (dataIndex + 1) % MAX_RECORDS;
}

// === Sensores ===
void initializeSensors() {
  dht.begin();
  pinMode(PIR_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(RAIN_PIN, INPUT);
  myservo.attach(SERVO_PIN);
}

void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t)) t = currentData.temperature;
  if (isnan(h)) h = currentData.humidity;

  currentData.temperature = t;
  currentData.humidity = h;
  currentData.gasValue = analogRead(GAS_PIN);
  currentData.rainValue = analogRead(RAIN_PIN);
  currentData.motionDetected = digitalRead(PIR_PIN);
  currentData.timestamp = getTimestamp();
}

// === Archivos SPIFFS ===
String getContentType(const String& filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

bool handleFileRead(const String& path) {
  String filepath = path;
  if (filepath == "/") filepath = "/index.html";
  if (!SPIFFS.exists(filepath)) return false;
  File file = SPIFFS.open(filepath, "r");
  server.streamFile(file, getContentType(filepath));
  file.close();
  return true;
}

// === Handlers ===
void handleRoot() {
  if (!handleFileRead("/index.html")) server.send(500, "text/plain", "index.html not found");
}

void handleStatic() {
  if (!handleFileRead(server.uri())) server.send(404, "text/plain", "File not found");
}

void handleSensorData() {
  StaticJsonDocument<256> doc;
  doc["temperature"] = currentData.temperature;
  doc["humidity"] = currentData.humidity;
  doc["gasValue"] = currentData.gasValue;
  doc["rainValue"] = currentData.rainValue;
  doc["motionDetected"] = currentData.motionDetected;
  doc["timestamp"] = currentData.timestamp;
  String resp;
  serializeJson(doc, resp);
  server.send(200, "application/json", resp);
}

void handleGraphData() {
  String sensor = server.hasArg("sensor") ? server.arg("sensor") : "temperature";
  String fechaFiltro = server.hasArg("date") ? server.arg("date") : "";

  StaticJsonDocument<8192> doc;
  JsonArray arr = doc.to<JsonArray>();

  int count = 0;
  for (int i = 0; i < MAX_RECORDS; i++) {
    int idx = (dataIndex + i) % MAX_RECORDS;
    if (historicalData[idx].timestamp.length() == 0) continue;

    if (fechaFiltro != "") {
      if (!historicalData[idx].timestamp.startsWith(fechaFiltro.substring(8,10) + "/" +
                                                    fechaFiltro.substring(5,7) + "/" +
                                                    fechaFiltro.substring(0,4))) {
        continue;
      }
    }

    JsonObject obj = arr.createNestedObject();
    obj["time"] = historicalData[idx].timestamp;
    if (sensor == "temperature") obj["value"] = historicalData[idx].temperature;
    else if (sensor == "humidity") obj["value"] = historicalData[idx].humidity;
    else if (sensor == "gas") obj["value"] = historicalData[idx].gasValue;
    else if (sensor == "rain") obj["value"] = historicalData[idx].rainValue;
    else if (sensor == "motion") obj["value"] = (historicalData[idx].motionDetected ? 1 : 0);
    else obj["value"] = 0;
    if (++count > 300) break;
  }

  String resp;
  serializeJson(arr, resp);
  server.send(200, "application/json", resp);
}

void handleControl() {
  if (server.hasArg("servo")) {
    int angle = constrain(server.arg("servo").toInt(), 0, 180);
    myservo.write(angle);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// === SETUP / LOOP ===
void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  lcd.init();
  lcd.backlight();
  lcd.print("Iniciando...");

  initializeSensors();

  // === WiFi fijo ===
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  lcd.clear();
  lcd.print("WiFi OK");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.onNotFound(handleStatic);
  server.on("/data", HTTP_GET, handleSensorData);
  server.on("/graph", HTTP_GET, handleGraphData);
  server.on("/control", HTTP_GET, handleControl);
  server.begin();

  lcd.clear();
  lcd.print("Sistema Listo");
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    readSensors();
    saveHistoricalData();
    lastSensorRead = now;
  }
}
