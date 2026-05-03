#include <WiFi.h>
#include <PubSubClient.h>

// ===== WIFI =====
const char* ssid = "salahtotal";
const char* password = "Warungkopi";

// ===== MQTT =====
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);

// ===== SIM800L =====
HardwareSerial sim800(1);

// ===== PIN =====
#define FLOW_SENSOR 27
#define BUZZER 25

unsigned long buzzerTimer = 0;
bool buzzerState = false;

volatile int pulse = 0;
float flowRate = 0;
float total = 0;

unsigned long lastTime = 0;
unsigned long flowStartTime = 0;

// ===== BATAS =====
float batasFlow = 15.0;
int batasDurasi = 20;

bool sedangMengalir = false;

// ===== SMS =====
bool smsSent = false;

// ===== AUTO SWITCH =====
unsigned long lastSwitchTime = 0;
bool lastModeWiFi = true;

// ===== INTERRUPT =====
void IRAM_ATTR countPulse() {
  pulse++;
}

// ===== WIFI =====
void setup_wifi() {
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
  } else {
    Serial.println("\nWiFi Failed");
  }
}

// ===== MQTT =====
void reconnectMQTT() {
  if (!client.connected()) {
    if (client.connect("ESP32_HYBRID")) {
      Serial.println("MQTT Connected");
    }
  }
}

// ===== GSM HTTP =====
void sendGSM(float flow, float total, int alarm) {

  Serial.println("📡 GSM SEND");

  sim800.println("AT");
  delay(500);

  sim800.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  delay(1000);

  sim800.println("AT+SAPBR=3,1,\"APN\",\"internet\"");
  delay(1000);

  sim800.println("AT+SAPBR=1,1");
  delay(3000);

  sim800.println("AT+HTTPINIT");
  delay(1000);

  String url = "http://smartwatermeter-production.up.railway.app?";
  url += "flow=" + String(flow);
  url += "&total=" + String(total);
  url += "&alarm=" + String(alarm);

  sim800.print("AT+HTTPPARA=\"URL\",\"");
  sim800.print(url);
  sim800.println("\"");

  delay(1000);

  sim800.println("AT+HTTPACTION=0");
  delay(5000);

  sim800.println("AT+HTTPTERM");
}

// ===== SMS =====
void sendSMS(String msg){
  sim800.println("AT+CMGF=1");
  delay(1000);

  sim800.println("AT+CMGS=\"+6287739853663\""); // GANTI NOMOR
  delay(1000);

  sim800.print(msg);
  delay(500);

  sim800.write(26);
  delay(3000);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

pinMode(BUZZER, OUTPUT);
digitalWrite(BUZZER, HIGH); // default mati

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR), countPulse, FALLING);

  setup_wifi();

  client.setServer(mqtt_server, 1883);

  sim800.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("🚀 SYSTEM HYBRID READY");

}

// ===== LOOP =====
void loop() {

  if (millis() - lastTime >= 1000) {

    // ===== HITUNG FLOW =====
    flowRate = pulse / 7.5;
    total += (flowRate / 60.0);

    // ===== DETEKSI DURASI =====
    if (flowRate > 0) {
      if (!sedangMengalir) {
        flowStartTime = millis();
        sedangMengalir = true;
      }
    } else {
      sedangMengalir = false;
    }

    int durasi = (millis() - flowStartTime) / 1000;

    // ===== ALARM =====
    bool alarm = (
      flowRate > batasFlow ||
      (sedangMengalir && durasi > batasDurasi)
    );

    // ===== BUZZER =====
unsigned long now = millis();

// ===== NORMAL =====
if (!alarm) {
  digitalWrite(BUZZER, HIGH); // mati (untuk active buzzer)
}

// ===== BAHAYA =====
else if (flowRate > batasFlow) {

  // beep cepat
  if (now - buzzerTimer > 150) {
    buzzerState = !buzzerState;
    digitalWrite(BUZZER, buzzerState ? LOW : HIGH);
    buzzerTimer = now;
  }
}

// ===== SIAGA =====
else {

  // beep pelan
  if (now - buzzerTimer > 700) {
    buzzerState = !buzzerState;
    digitalWrite(BUZZER, buzzerState ? LOW : HIGH);
    buzzerTimer = now;
  }
}

    // ===== SMS =====
    if (alarm && !smsSent) {
      sendSMS("🚨 ALERT AIR TERDETEKSI!");
      smsSent = true;
    }
    if (!alarm) smsSent = false;

    // ===== AUTO SWITCH =====
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    int rssi = wifiConnected ? WiFi.RSSI() : -100;

    if (wifiConnected && !client.connected()) {
      reconnectMQTT();
    }

    bool mqttOK = client.connected();

    bool targetModeWiFi = (
      wifiConnected &&
      mqttOK &&
      rssi > -75
    );

    // ===== ANTI FLIP MODE =====
    if (targetModeWiFi != lastModeWiFi && millis() - lastSwitchTime > 10000) {
      lastModeWiFi = targetModeWiFi;
      lastSwitchTime = millis();
    }

    bool useWiFi = lastModeWiFi;

    // ===== DEBUG =====
    Serial.print("Flow: ");
    Serial.print(flowRate);
    Serial.print(" | Total: ");
    Serial.print(total);
    Serial.print(" | RSSI: ");
    Serial.print(rssi);
    Serial.print(" | MODE: ");

    // ===== KIRIM DATA =====
    if (useWiFi) {

      client.loop();

      client.publish("smartwater/flow", String(flowRate).c_str());
      client.publish("smartwater/total", String(total).c_str());
      client.publish("smartwater/alarm", alarm ? "1" : "0");

      Serial.println("📶 WIFI (MQTT)");

    } else {

      sendGSM(flowRate, total, alarm ? 1 : 0);

      Serial.println("📡 GSM");

    }

    pulse = 0;
    lastTime = millis();
  }
}