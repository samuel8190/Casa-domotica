#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <SPIFFS.h>

// ========== CONFIG SENSORES / PINES ==========
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#define GAS_PIN 34
#define RAIN_PIN 35
#define PIR_PIN 2
#define SERVO_PIN 5
#define LED_PIN 15
#define LED_ROJO_PIN 23 // Nuevo pin para el LED rojo

#define UMBRAL_LLUVIA 50 // porcentaje para activar servo

Servo myservo;
LiquidCrystal_I2C lcd(0x3F, 16, 2); // LCD con dirección 0x3F

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};
byte rowPins[ROWS] = {12, 14, 27, 26};
byte colPins[COLS] = {25, 33, 32, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================== VARIABLES CONTRASEÑA ==================
String inputCode = "";
const String correctCode = "1245";
bool accessGranted = false;

// Web server
WebServer server(80);

// Datos
struct SensorData {
    float temperature = 0.0;
    float humidity = 0.0;
    int gas = 0;
    int water = 0;
    bool motion = false;
    String tender = "Exterior";
    String timestamp;
};

const int MAX_RECORDS = 200;
SensorData history[MAX_RECORDS];
int histIndex = 0;

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 5000; // ms

// ---------- Helpers ----------
void sendCORSHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

String getTimestamp() {
    unsigned long s = millis() / 1000;
    unsigned long h = (s / 3600) % 24;
    unsigned long m = (s / 60) % 60;
    unsigned long sec = s % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, sec);
    return String(buf);
}

int readGasMapped() {
    int raw = analogRead(GAS_PIN);
    return map(raw, 0, 4095, 0, 5000);
}

int readWaterMapped() {
    int raw = analogRead(RAIN_PIN);
    int pct = map(raw, 0, 4095, 100, 0);
    return constrain(pct, 0, 100);
}

void saveHistory(const SensorData &d) {
    history[histIndex] = d;
    histIndex = (histIndex + 1) % MAX_RECORDS;
}

// ---------- SPIFFS ----------
String getContentType(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css")) return "text/css";
    if (filename.endsWith(".js")) return "application/javascript";
    if (filename.endsWith(".json")) return "application/json";
    if (filename.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

bool handleFileRead(String path) {
    if (path.endsWith("/")) path += "index.html";
    if (!SPIFFS.exists(path)) {
        Serial.println("Archivo no encontrado: " + path);
        return false;
    }
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, getContentType(path));
    file.close();
    return true;
}

void handleRoot() {
    if (!handleFileRead("/index.html")) {
        server.send(404, "text/plain", "index.html not found");
    }
}

// ========== RUTAS ==========
void handleOptions() {
    sendCORSHeaders();
    server.send(200, "text/plain", "");
}

void handleData() {
    SensorData d = history[(histIndex + MAX_RECORDS - 1) % MAX_RECORDS];
    if (d.timestamp == "") {
        d.temperature = dht.readTemperature();
        d.humidity = dht.readHumidity();
        d.gas = readGasMapped();
        d.water = readWaterMapped();
        d.motion = digitalRead(PIR_PIN) == HIGH;

        // Control del LED de movimiento
        if (d.motion) {
            digitalWrite(LED_ROJO_PIN, HIGH);
        } else {
            digitalWrite(LED_ROJO_PIN, LOW);
        }
        
        // Control de lluvia → servo si no se usó acceso por clave
        if (!accessGranted) {
            if (d.water >= UMBRAL_LLUVIA) {
                myservo.write(0);
                d.tender = "Cubierto";
            } else {
                myservo.write(90);
                d.tender = "Exterior";
            }
        }

        d.timestamp = getTimestamp();
    }

    StaticJsonDocument<256> doc;
    doc["temp"] = d.temperature;
    doc["hum"] = d.humidity;
    doc["gas"] = d.gas;
    doc["water"] = d.water;
    doc["motion"] = d.motion;
    doc["tender"] = d.tender;
    doc["timestamp"] = d.timestamp;

    String out;
    serializeJson(doc, out);
    sendCORSHeaders();
    server.send(200, "application/json", out);
}

void handleGraph() {
    StaticJsonDocument<4096> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < MAX_RECORDS; i++) {
        int idx = (histIndex + i) % MAX_RECORDS;
        if (history[idx].timestamp == "") continue;
        JsonObject p = arr.createNestedObject();
        p["time"] = history[idx].timestamp;
        p["temp"] = history[idx].temperature;
        p["hum"] = history[idx].humidity;
        p["gas"] = history[idx].gas;
        p["water"] = history[idx].water;
        p["motion"] = history[idx].motion ? 1 : 0;
        p["tender"] = (history[idx].tender == "Exterior") ? 1 : 0;
    }
    String out; serializeJson(doc, out);
    sendCORSHeaders();
    server.send(200, "application/json", out);
}

// ================== FUNCIONES CONTRASEÑA ==================
void resetInput() {
    inputCode = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ingrese contrasena");
    lcd.setCursor(0, 1);
}

void checkCode() {
    lcd.clear();
    if (inputCode == correctCode) {
        lcd.setCursor(0, 0);
        lcd.print("Bienvenido!!!");
        myservo.write(90); // abrir puerta
        accessGranted = true;
    } else {
        lcd.setCursor(0, 0);
        lcd.print("Intentelo de nuevo"); // Mensaje en la primera fila
        lcd.setCursor(0, 1);
        lcd.print("Intente de nuevo"); // Mensaje en la segunda fila
        delay(2000);
        resetInput();
    }
}

// ========== SETUP / LOOP ==========
void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(PIR_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(LED_ROJO_PIN, OUTPUT); // Configura el pin del LED rojo como salida
    digitalWrite(LED_PIN, LOW);
    digitalWrite(LED_ROJO_PIN, LOW); // Asegúrate de que el LED rojo comience apagado

    myservo.attach(SERVO_PIN);
    myservo.write(0); // puerta cerrada al inicio
    dht.begin();

    Wire.begin();
    lcd.init();
    lcd.backlight();
    resetInput();

    if (!SPIFFS.begin(true)) {
        Serial.println("Error montando SPIFFS");
        lcd.clear();
        lcd.print("SPIFFS error");
        delay(2000);
    }

    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    if (!wm.autoConnect("CasaDomotica_AP")) {
        Serial.println("No conectado - reiniciando");
        lcd.clear();
        lcd.print("WiFi Error");
        delay(2000);
        ESP.restart();
    }

    lcd.clear();
    lcd.print("WiFi listo");

    server.on("/", HTTP_GET, handleRoot);
    server.on("/data", HTTP_GET, handleData);
    server.on("/graph", HTTP_GET, handleGraph);

    server.onNotFound([]() {
        if (!handleFileRead(server.uri())) {
            sendCORSHeaders();
            server.send(404, "text/plain", "Not found");
        }
    });

    server.begin();

    delay(1200);
}

void loop() {
    server.handleClient();

    // ========= SENSOR UPDATE =========
    unsigned long now = millis();
    if (now - lastSensorRead >= SENSOR_INTERVAL) {
        lastSensorRead = now;
        SensorData d;
        d.temperature = dht.readTemperature();
        d.humidity = dht.readHumidity();
        if (isnan(d.temperature)) d.temperature = 0.0;
        if (isnan(d.humidity)) d.humidity = 0.0;
        d.gas = readGasMapped();
        d.water = readWaterMapped();
        
        // Lee el estado del sensor PIR
        d.motion = digitalRead(PIR_PIN) == HIGH;

        // Controla el LED rojo según el movimiento
        if (d.motion) {
            digitalWrite(LED_ROJO_PIN, HIGH);
        } else {
            digitalWrite(LED_ROJO_PIN, LOW);
        }
        
        // Control de lluvia → servo si no se usó acceso por clave
        if (!accessGranted) {
            if (d.water >= UMBRAL_LLUVIA) {
                myservo.write(0);
                d.tender = "Cubierto";
            } else {
                myservo.write(90);
                d.tender = "Exterior";
            }
        }

        d.timestamp = getTimestamp();
        saveHistory(d);
    }

    // ========= CONTRASEÑA =========
    char k = keypad.getKey();
    if (k) {
        if (k == '*') {
            resetInput();
        } else if (k == '0') {
            checkCode();
        } else if (!accessGranted && isDigit(k)) {
            if (inputCode.length() < 4) {
                inputCode += k;
                lcd.setCursor(inputCode.length() - 1, 1);
                lcd.print("*");
            }
        }
    }
}