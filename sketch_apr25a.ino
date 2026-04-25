#include <WiFi.h>
#include <PubSubClient.h>

// ===== WIFI =====
const char* ssid = "TBW";
const char* password = "Warungkopi";

// ===== MQTT =====
const char* mqtt_server = "broker.emqx.io";

// ===== PIN =====
#define FLOW_SENSOR 27
#define BUZZER 25

WiFiClient espClient;
PubSubClient client(espClient);

// ===== FLOW =====
volatile int pulse = 0;
float flowRate = 0;
float total = 0;

unsigned long lastTime = 0;
unsigned long flowStartTime = 0;

// ===== BATAS =====
float batasFlow = 10.0;     // L/min
int batasDurasi = 10;       // detik

bool sedangMengalir = false;

// ===== BUZZER =====
unsigned long lastBeepTime = 0;
bool buzzerState = false;
int intervalBeep = 100;

// ===== INTERRUPT =====
void IRAM_ATTR countPulse() {
  pulse++;
}

// ===== WIFI CONNECT =====
void setup_wifi() {
  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
}

// ===== MQTT CONNECT =====
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");

    if (client.connect("ESP32_WATER")) {
      Serial.println("Connected");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(FLOW_SENSOR, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR), countPulse, FALLING);

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  Serial.println("SYSTEM READY 🚀");
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // ===== HITUNG FLOW =====
  if (millis() - lastTime >= 1000) {

    flowRate = pulse / 7.5;   // kalibrasi sensor
    total += (flowRate / 60.0);

    Serial.print("Flow: ");
    Serial.print(flowRate);
    Serial.print(" L/min | Total: ");
    Serial.print(total);

    // ===== DETEKSI ALIRAN =====
    if (flowRate > 0) {
      if (!sedangMengalir) {
        flowStartTime = millis();
        sedangMengalir = true;
      }
    } else {
      sedangMengalir = false;
    }

    int durasi = (millis() - flowStartTime) / 1000;

    Serial.print(" | Durasi: ");
    Serial.print(durasi);
    Serial.println(" s");

    // ===== LOGIKA ALARM =====
    bool alarm = (
      flowRate > batasFlow ||
      (sedangMengalir && durasi > batasDurasi)
    );

    // ===== MQTT PUBLISH =====
    client.publish("smartwater/flow", String(flowRate).c_str());
    client.publish("smartwater/total", String(total).c_str());
    client.publish("smartwater/alarm", alarm ? "1" : "0");

    // ===== BUZZER =====
    if (alarm) {
      if (millis() - lastBeepTime >= intervalBeep) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER, buzzerState);
        lastBeepTime = millis();
      }
    } else {
      digitalWrite(BUZZER, HIGH);
      buzzerState = false;
    }

    pulse = 0;
    lastTime = millis();
  }
}