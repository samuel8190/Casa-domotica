#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <DHT.h>

// ================= CONFIGURACIÓN SENSORES =================

// DHT22 (Temperatura y Humedad)
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// Sensor de Gas MQ135
#define GAS_PIN 34

// Sensor de Movimiento PIR
#define PIR_PIN 2

// Sensor de Lluvia
#define RAIN_PIN 35

// Servomotor
#define SERVO_PIN 5
Servo myservo;

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Teclado Matricial 4x4
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

// ================= VARIABLES GLOBALES =================

// WiFi y Servidor Web
WebServer server(80);
bool wifiConnected = false;

// Datos de sensores
struct SensorData {
  float temperature;
  float humidity;
  int gasValue;
  int rainValue;
  bool motionDetected;
  String timestamp;
};

SensorData currentData;
const int MAX_RECORDS = 100;
SensorData historicalData[MAX_RECORDS];
int dataIndex = 0;

// Variables de control
char lastKey = '\0';
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 5000;

// ================= SETUP =================

void setup() {
  Serial.begin(115200);
  
  // Inicializar EEPROM
  EEPROM.begin(512);
  
  // Inicializar sensores
  initializeSensors();
  
  // Inicializar LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");
  
  // Configurar WiFi Manager
  setupWiFiManager();
  
  // Configurar servidor web
  setupWebServer();
  
  lcd.clear();
  lcd.print("Sistema Listo!");
  delay(2000);
}

void initializeSensors() {
  dht.begin();
  pinMode(PIR_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(RAIN_PIN, INPUT);
  myservo.attach(SERVO_PIN);
}

void setupWiFiManager() {
  WiFiManager wm;
  
  // Configurar timeout
  wm.setConfigPortalTimeout(180);
  
  if (!wm.autoConnect("CasaDomotica_AP")) {
    Serial.println("Failed to connect and hit timeout");
    lcd.clear();
    lcd.print("Error WiFi");
    lcd.setCursor(0, 1);
    lcd.print("Reiniciando...");
    delay(3000);
    ESP.restart();
  }
  
  wifiConnected = true;
  lcd.clear();
  lcd.print("WiFi Conectado!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(2000);
}

void setupWebServer() {
  // Rutas del servidor web
  server.on("/", handleRoot);
  server.on("/data", handleSensorData);
  server.on("/historical", handleHistoricalData);
  server.on("/control", handleControl);
  server.on("/calendar", handleCalendar);
  server.on("/graph", handleGraphData);
  
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

// ================= LOOP PRINCIPAL =================

void loop() {
  server.handleClient();
  
  unsigned long currentMillis = millis();
  
  // Leer sensores cada intervalo
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    readSensors();
    saveHistoricalData();
    lastSensorRead = currentMillis;
  }
  
  // Leer teclado
  readKeypad();
  
  // Actualizar LCD
  updateLCD();
}

// ================= FUNCIONES SENSORES =================

void readSensors() {
  // Leer DHT22
  currentData.temperature = dht.readTemperature();
  currentData.humidity = dht.readHumidity();
  
  // Leer otros sensores
  currentData.gasValue = analogRead(GAS_PIN);
  currentData.rainValue = analogRead(RAIN_PIN);
  currentData.motionDetected = digitalRead(PIR_PIN);
  
  // Timestamp
  currentData.timestamp = getTimestamp();
  
  // Verificar errores DHT
  if (isnan(currentData.temperature) || isnan(currentData.humidity)) {
    currentData.temperature = 0.0;
    currentData.humidity = 0.0;
  }
}

void saveHistoricalData() {
  historicalData[dataIndex] = currentData;
  dataIndex = (dataIndex + 1) % MAX_RECORDS;
}

String getTimestamp() {
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  char timestamp[20];
  snprintf(timestamp, sizeof(timestamp), "%02lu:%02lu:%02lu", 
           hours % 24, minutes % 60, seconds % 60);
  return String(timestamp);
}

void readKeypad() {
  char key = keypad.getKey();
  if (key) {
    lastKey = key;
    handleKeypadAction(key);
  }
}

void handleKeypadAction(char key) {
  switch(key) {
    case 'A':
      myservo.write(90); // Abrir
      break;
    case 'B':
      myservo.write(0); // Cerrar
      break;
    case 'C':
      // Mostrar IP en LCD
      lcd.clear();
      lcd.print("IP:");
      lcd.setCursor(0, 1);
      lcd.print(WiFi.localIP());
      delay(3000);
      break;
    case 'D':
      // Alternar modo
      break;
  }
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(currentData.temperature, 1);
  lcd.print("C G:");
  lcd.print(currentData.gasValue);
  
  lcd.setCursor(0, 1);
  if (currentData.gasValue > 1500) {
    lcd.print("ALERTA GAS!   ");
  } else if (currentData.rainValue < 2000) {
    lcd.print("LLUVIA!       ");
  } else if (currentData.motionDetected) {
    lcd.print("MOVIMIENTO!   ");
  } else {
    lcd.print("T:");
    lcd.print(currentData.temperature, 1);
    lcd.print(" H:");
    lcd.print(currentData.humidity, 0);
  }
}

// ================= SERVIDOR WEB =================

void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Casa Domótica</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
      body { font-family: Arial; margin: 20px; background: #f0f0f0; }
      .card { background: white; padding: 20px; margin: 10px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
      .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
      .sensor-value { font-size: 24px; font-weight: bold; color: #2c3e50; }
      .alert { color: #e74c3c; }
      .normal { color: #27ae60; }
      button { padding: 10px 20px; margin: 5px; background: #3498db; color: white; border: none; border-radius: 5px; cursor: pointer; }
      button:hover { background: #2980b9; }
      #chartContainer { width: 100%; height: 400px; }
    </style>
  </head>
  <body>
    <h1>🏠 Casa Domótica Inteligente</h1>
    
    <div class="grid">
      <!-- Datos en Tiempo Real -->
      <div class="card">
        <h2>📊 Datos en Tiempo Real</h2>
        <div id="realTimeData">
          <p>🌡️ Temperatura: <span class="sensor-value" id="temp">--</span> °C</p>
          <p>💧 Humedad: <span class="sensor-value" id="hum">--</span> %</p>
          <p>⚠️ Gas: <span class="sensor-value" id="gas">--</span></p>
          <p>🌧️ Lluvia: <span class="sensor-value" id="rain">--</span></p>
          <p>🚶 Movimiento: <span class="sensor-value" id="motion">--</span></p>
        </div>
        <button onclick="updateData()">🔄 Actualizar</button>
      </div>

      <!-- Control de Actuadores -->
      <div class="card">
        <h2>🎮 Control</h2>
        <button onclick="controlServo(90)">🚪 Abrir Servo</button>
        <button onclick="controlServo(0)">🚪 Cerrar Servo</button>
      </div>

      <!-- Calendario para Datos Históricos -->
      <div class="card">
        <h2>📅 Datos Históricos</h2>
        <input type="date" id="historyDate">
        <select id="sensorType">
          <option value="temperature">🌡️ Temperatura</option>
          <option value="humidity">💧 Humedad</option>
          <option value="gas">⚠️ Gas</option>
          <option value="rain">🌧️ Lluvia</option>
        </select>
        <button onclick="loadHistoricalData()">📈 Ver Gráfico</button>
      </div>
    </div>

    <!-- Gráfico -->
    <div class="card">
      <h2>📈 Gráfico de Datos</h2>
      <canvas id="sensorChart"></canvas>
    </div>

    <script>
      let sensorChart;
      
      // Actualizar datos en tiempo real
      function updateData() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            document.getElementById('temp').textContent = data.temperature.toFixed(1);
            document.getElementById('hum').textContent = data.humidity.toFixed(1);
            document.getElementById('gas').textContent = data.gasValue;
            document.getElementById('rain').textContent = data.rainValue;
            document.getElementById('motion').textContent = data.motionDetected ? 'DETECTADO' : 'NO';
          });
      }

      // Controlar servomotor
      function controlServo(angle) {
        fetch('/control?servo=' + angle);
      }

      // Cargar datos históricos
      function loadHistoricalData() {
        const date = document.getElementById('historyDate').value;
        const sensor = document.getElementById('sensorType').value;
        
        if (!date) {
          alert('Selecciona una fecha');
          return;
        }

        fetch('/graph?date=' + date + '&sensor=' + sensor)
          .then(response => response.json())
          .then(data => {
            updateChart(data, sensor);
          });
      }

      // Actualizar gráfico
      function updateChart(data, sensorType) {
        const ctx = document.getElementById('sensorChart').getContext('2d');
        
        if (sensorChart) {
          sensorChart.destroy();
        }

        const labels = data.map(item => item.time);
        const values = data.map(item => item.value);
        const label = getSensorLabel(sensorType);

        sensorChart = new Chart(ctx, {
          type: 'line',
          data: {
            labels: labels,
            datasets: [{
              label: label,
              data: values,
              borderColor: '#3498db',
              backgroundColor: 'rgba(52, 152, 219, 0.1)',
              tension: 0.4
            }]
          },
          options: {
            responsive: true,
            scales: {
              y: {
                beginAtZero: true
              }
            }
          }
        });
      }

      function getSensorLabel(type) {
        const labels = {
          temperature: 'Temperatura (°C)',
          humidity: 'Humedad (%)',
          gas: 'Nivel de Gas',
          rain: 'Nivel de Lluvia'
        };
        return labels[type] || 'Sensor';
      }

      // Actualizar automáticamente cada 5 segundos
      setInterval(updateData, 5000);
      updateData(); // Primera carga
    </script>
  </body>
  </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleSensorData() {
  StaticJsonDocument<500> doc;
  doc["temperature"] = currentData.temperature;
  doc["humidity"] = currentData.humidity;
  doc["gasValue"] = currentData.gasValue;
  doc["rainValue"] = currentData.rainValue;
  doc["motionDetected"] = currentData.motionDetected;
  doc["timestamp"] = currentData.timestamp;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleControl() {
  if (server.hasArg("servo")) {
    int angle = server.arg("servo").toInt();
    myservo.write(angle);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleGraphData() {
  String sensorType = server.hasArg("sensor") ? server.arg("sensor") : "temperature";
  
  StaticJsonDocument<2000> doc;
  JsonArray data = doc.to<JsonArray>();
  
  // Simular datos históricos (en un sistema real, buscarías en EEPROM/SD)
  for (int i = 0; i < 24; i++) {
    JsonObject point = data.createNestedObject();
    point["time"] = String(i) + ":00";
    
    // Datos de ejemplo - reemplazar con tus datos reales
    if (sensorType == "temperature") {
      point["value"] = 20 + random(0, 100) / 10.0;
    } else if (sensorType == "humidity") {
      point["value"] = 40 + random(0, 300) / 10.0;
    } else if (sensorType == "gas") {
      point["value"] = random(100, 2000);
    } else if (sensorType == "rain") {
      point["value"] = random(0, 4095);
    }
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleHistoricalData() {
  server.send(200, "application/json", "{\"message\":\"Función en desarrollo\"}");
}
void handleCalendar() {
  server.send(200, "application/json", "{\"message\":\"Función en desarrollo\"}");
}