#include "DHT.h"
#include <Wire.h>
#include <BH1750.h>
#include <ESP32Servo.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ---------- PINOVI ----------
#define DHTPIN 4
#define DHTTYPE DHT22

#define IR_PIN 15
#define MQ2_PIN 34
#define BUZZER_PIN 26
#define FAN_PIN 33
#define HALL_PIN 32

#define LED_GREEN 16
#define LED_RED 17

#define SERVO_PIN 14
#define RAMP_OPEN 90
#define RAMP_CLOSE 0

#define SDA_PIN 13
#define RST_PIN 27

// ---------- PRAGOVI ----------
#define TEMP_LIMIT 35.0
#define GAS_LIMIT 2500
#define FAN_TEMP_LIMIT 25.0


// ---------- WIFI + SERVER ----------
const char* ssid = "wifi2";
const char* password = "Lozinka1234";


const char* serverUrl = "http://10.64.173.122:5000/api/sensor";
const char* checkCardUrl = "http://10.64.173.122:5000/check_card";


MFRC522 mfrc522(SS_PIN, RST_PIN);
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
Servo rampServo;

// ---------- RAMPA TIMER ----------
unsigned long rampOpenTime = 0;
const unsigned long RAMP_TIMEOUT = 10000; // 10 s
bool rampIsOpen = false;

// ---------- SLANJE TIMER ----------
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL_MS = 5000; // 5 s

// ---------- RFID WHITELIST ----------
byte allowedUIDs[][4] = {
  {0xDE, 0xAD, 0xBE, 0xEF},
  {0x12, 0x34, 0x56, 0x78},
  {0x1A, 0xAD, 0xAD, 0x89}   //MojaKarta TEST ZA LOKALNO
};

const int allowedCount = sizeof(allowedUIDs) / sizeof(allowedUIDs[0]);

bool isUIDAllowed() {
  for (int i = 0; i < allowedCount; i++) {
    bool match = true;
    for (byte j = 0; j < 4; j++) {
      if (mfrc522.uid.uidByte[j] != allowedUIDs[i][j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

String getUIDString() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// ---------- RAMPA ----------
void openRamp() {
  rampServo.write(RAMP_OPEN);
  rampIsOpen = true;
  rampOpenTime = millis();
  Serial.println("RAMPA OTVORENA");
}

void closeRamp(const char* reason) {
  rampServo.write(RAMP_CLOSE);
  rampIsOpen = false;

  // default stanje: crveno
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW);

  Serial.println(reason);
}

bool checkCardViaFlask(const String& uidHex) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi nije spojen - ne mogu provjeriti karticu.");
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(8000); // 8s 

  if (!http.begin(client, checkCardUrl)) {
    Serial.println("http.begin() za check_card nije uspio.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  String json = "{\"uid\":\"" + uidHex + "\"}";
  int httpCode = http.POST(json);

  Serial.println("---- CHECK CARD ----");
  Serial.print("POST: "); Serial.println(json);
  Serial.print("HTTP code: "); Serial.println(httpCode);

  if (httpCode < 0) {
    Serial.print("HTTP error: ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    client.stop();
    return false;
  }

  String resp = http.getString();
  Serial.print("Response: "); Serial.println(resp);

  http.end();
  client.stop();

  // 200 = allowed, 403 = denied
  return (httpCode == 200);
}
// ---------- HTTP SEND (SENZORI) ----------
void sendSensors(float temperature,
                 float humidity,
                 int gas,
                 float lux,
                 int irState,
                 int hallState,
                 int parking,
                 int fanState) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi nije spojen - preskačem slanje.");
    return;
  }

  String json = "{";
  json += "\"temperature\":" + String(temperature, 2) + ",";
  json += "\"humidity\":" + String(humidity, 2) + ",";
  json += "\"gas\":" + String(gas) + ",";
  json += "\"lux\":" + String(lux, 1) + ",";
  json += "\"ir_state\":" + String(irState) + ",";
  json += "\"parking\":" + String(parking) + ",";
  json += "\"hall_state\":" + String(hallState) + ",";
  json += "\"ventilator\":" + String(fanState);
  json += "}";

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(15000);

  if (!http.begin(client, serverUrl)) {
    Serial.println("http.begin() nije uspio!");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(json);

  Serial.println("---- SLANJE SENZORA ----");
  Serial.print("POST JSON: "); Serial.println(json);
  Serial.print("HTTP code: "); Serial.println(httpCode);
  Serial.print("ESP IP: "); Serial.println(WiFi.localIP());

    if (httpCode < 0) {
    Serial.print("HTTP error: ");
    Serial.println(http.errorToString(httpCode));
  } else {
    String resp = http.getString();
    Serial.print("Response: "); Serial.println(resp);
  }

  http.end();
  client.stop();
}

int lastIrState = -1;
void setup() {
  Serial.begin(115200);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Spajanje na WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi spojen!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Senzori / pinovi
  dht.begin();
  pinMode(IR_PIN, INPUT);
  pinMode(HALL_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);

  // I2C (BH1750)
  Wire.begin(21, 22);
  lightMeter.begin();

  // RFID
  SPI.begin(18, 19, 23, 13);   // SCK, MISO, MOSI, SS
  mfrc522.PCD_Init();         // init RC522
  mfrc522.PCD_DumpVersionToSerial();

  // Servo
  rampServo.attach(SERVO_PIN);
  rampServo.write(RAMP_CLOSE);

  Serial.println("Sve je pokrenuto.");
  Serial.println("Prisloni RFID karticu...");
  }
void loop() {
  bool alarm = false;

if (!rampIsOpen && mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

  String uidStr = getUIDString();
  Serial.print("Kartica ocitana UID: ");
  Serial.println(uidStr);

  bool allowed = checkCardViaFlask(uidStr);

  if (allowed) {
    Serial.println("PRISTUP ODOBREN (FLASK)");
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
    openRamp();

    digitalWrite(BUZZER_PIN, HIGH);
    delay(600);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    Serial.println("PRISTUP ODBIJEN (FLASK)");
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(500);
}

  // DHT 
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Greška pri čitanju DHT22!");
  } else {
    // Ventilator stanje
    int fanState = (temperature > FAN_TEMP_LIMIT) ? 1 : 0;
    digitalWrite(FAN_PIN, fanState ? HIGH : LOW);

    // Temperaturni alarm
    if (temperature > TEMP_LIMIT) {
      Serial.println("⚠️ ALARM: VISOKA TEMPERATURA!");
      alarm = true;
    }
  }

  //  IR (parking) 
int irState = digitalRead(IR_PIN);
int parking = (irState == LOW) ? 1 : 0;

// ispis samo kad se promijeni
if (irState != lastIrState) {
  if (irState == LOW) Serial.println("Parking: ZAUZETO");
  else Serial.println("Parking: SLOBODNO");
  lastIrState = irState;
}

  //  MQ2 
  int gas = analogRead(MQ2_PIN);
  if (gas > GAS_LIMIT) {
    Serial.println("⚠️ ALARM: VISOKA KONCENTRACIJA PLINA!");
    alarm = true;
  }

  //  BH1750 
  float lux = lightMeter.readLightLevel();
  if (lux < 0) {
    Serial.println("Greška pri čitanju BH1750!");
    lux = 0; // fallback
  }

  //  HALL 
  int hallState = digitalRead(HALL_PIN);

  // zatvori rampu kad vozilo prođe 
  if (rampIsOpen && hallState == LOW) {
    closeRamp("Hall senzor: VOZILO PROŠLO");
  }

  // auto zatvaranje
  if (rampIsOpen && millis() - rampOpenTime >= RAMP_TIMEOUT) {
    closeRamp("Timer istekao – RAMPA ZATVORENA");
  }

  //  BUZZER (alarm) 
  // alarm buzzer 
  digitalWrite(BUZZER_PIN, alarm ? HIGH : LOW);

  //  PERIODIČNO SLANJE 
  // računa fanState i ako DHT faila, šalje fanState=0 + temp/hum=nan? -> pošaljimo 0 umjesto nan
  float tSend = isnan(temperature) ? 0.0 : temperature;
  float hSend = isnan(humidity) ? 0.0 : humidity;
  int fanSend = (!isnan(temperature) && temperature > FAN_TEMP_LIMIT) ? 1 : 0;

  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    sendSensors(
  tSend,
  hSend,
  gas,
  lux,
  irState,
  hallState,
  parking,
  fanSend
);

    lastSend = millis();
  }
  delay(200); // mali delay da ne spama Serial
}
