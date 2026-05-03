#include <WiFi.h>
#include <PubSubClient.h>

// ===== WIFI =====
const char* ssid = "TBW";
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

  String url = "http://smartwatermeter-production.up.railway.app/update?";
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

// ===== SMS ALERT =====
void sendSMS(String msg){
  sim800.println("AT+CMGF=1");
  delay(1000);

  sim800.println("AT+CMGS=\"087739853663\""); // GANTI NOMOR
  delay(1000);

  sim800.print(msg);
  delay(500);

  sim800.write(26);
  delay(3000);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(FLOW_SENSOR, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR), countPulse, FALLING);

  setup_wifi();

  client.setServer(mqtt_server, 1883);

  sim800.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("🚀 SYSTEM HYBRID READY");
}

// ===== LOOP =====
void loop() {

  if (millis() - lastTime >= 1000) {

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
    if (alarm) {
      digitalWrite(BUZZER, LOW);
    } else {
      digitalWrite(BUZZER, HIGH);
    }

    // ===== SMS =====
    if (alarm && !smsSent) {
      sendSMS("🚨 ALERT AIR TERDETEKSI!");
      smsSent = true;
    }

    if (!alarm) smsSent = false;

    Serial.print("Flow: ");
    Serial.print(flowRate);
    Serial.print(" | Total: ");
    Serial.println(total);

    // ===== AUTO SWITCH =====
    if (WiFi.status() == WL_CONNECTED) {

      reconnectMQTT();
      client.loop();

      client.publish("smartwater/flow", String(flowRate).c_str());
      client.publish("smartwater/total", String(total).c_str());
      client.publish("smartwater/alarm", alarm ? "1" : "0");

      Serial.println("📶 MODE: WIFI (MQTT)");

    } else {

      sendGSM(flowRate, total, alarm ? 1 : 0);

      Serial.println("📡 MODE: GSM");

    }

    pulse = 0;
    lastTime = millis();
  }
}