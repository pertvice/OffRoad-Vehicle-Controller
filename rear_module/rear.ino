// rear.ino - Rear Module ESP32 Code for Off-Road Vehicle Controller
// Controls: Sequential tail lights (6 high filaments + parking low + 2 reverse lamps)
// Features: 5 selectable patterns, Hazard > Turn > Brake priority
// Comms: MQTT over WiFi (commands from Raspberry Pi 5)

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --------------------- WiFi & MQTT Config ---------------------
const char* ssid = "YourWiFiSSID";          // ← Change to match your network
const char* password = "YourWiFiPassword";  // ← Change
const char* mqtt_server = "192.168.4.1";    // RPi5 hotspot IP or broker IP
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// --------------------- Pin Definitions (adjust as needed) ---------------------
// Relays for high filaments (sequential control) - active HIGH
const int RELAY_REAR_L1_HIGH = 2;   // Left side lamp 1 (innermost)
const int RELAY_REAR_L2_HIGH = 4;
const int RELAY_REAR_L3_HIGH = 5;   // Left side lamp 3 (outermost)
const int RELAY_REAR_R1_HIGH = 18;
const int RELAY_REAR_R2_HIGH = 19;
const int RELAY_REAR_R3_HIGH = 21;

// Parking low filament (all 6 lamps share one relay)
const int RELAY_PARKING_ALL  = 22;

// Reverse lamps (2 total)
const int RELAY_REVERSE_L    = 23;
const int RELAY_REVERSE_R    = 25;

// Optional: Fuel level sender (analog)
const int PIN_FUEL_LEVEL     = 34;  // 0-3.3V from sender/resistive probe

// --------------------- Sequential Pattern Definitions ---------------------
#define NUM_PATTERNS 5
int currentPattern = 1;             // 1 to 5, set via MQTT
unsigned long lastStepTime = 0;
const unsigned long STEP_INTERVAL = 400;  // ms per step - adjustable via menu later
int seqStep = 0;                    // Current animation step

bool hazardActive   = false;
bool turnLeftActive = false;
bool turnRightActive = false;
bool brakeActive    = false;
bool parkingOn      = false;
bool reverseOn      = false;

// --------------------- Setup ---------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  // All relays off
  pinMode(RELAY_REAR_L1_HIGH, OUTPUT); digitalWrite(RELAY_REAR_L1_HIGH, LOW);
  pinMode(RELAY_REAR_L2_HIGH, OUTPUT); digitalWrite(RELAY_REAR_L2_HIGH, LOW);
  pinMode(RELAY_REAR_L3_HIGH, OUTPUT); digitalWrite(RELAY_REAR_L3_HIGH, LOW);
  pinMode(RELAY_REAR_R1_HIGH, OUTPUT); digitalWrite(RELAY_REAR_R1_HIGH, LOW);
  pinMode(RELAY_REAR_R2_HIGH, OUTPUT); digitalWrite(RELAY_REAR_R2_HIGH, LOW);
  pinMode(RELAY_REAR_R3_HIGH, OUTPUT); digitalWrite(RELAY_REAR_R3_HIGH, LOW);
  pinMode(RELAY_PARKING_ALL, OUTPUT);  digitalWrite(RELAY_PARKING_ALL, LOW);
  pinMode(RELAY_REVERSE_L, OUTPUT);    digitalWrite(RELAY_REVERSE_L, LOW);
  pinMode(RELAY_REVERSE_R, OUTPUT);    digitalWrite(RELAY_REVERSE_R, LOW);

  // Fuel level as input
  analogReadResolution(12);

  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

// --------------------- WiFi Connect ---------------------
void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// --------------------- MQTT Reconnect ---------------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT connecting...");
    String clientId = "RearESP32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("rear/#");           // All rear commands
      client.subscribe("lights/hazard");
      client.subscribe("lights/turn_left");
      client.subscribe("lights/turn_right");
      client.subscribe("lights/brake");
      client.subscribe("lights/parking");
      client.subscribe("rear/pattern");
      client.subscribe("lights/reverse");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

// --------------------- MQTT Callback ---------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  String t = String(topic);

  if (t == "rear/pattern") {
    int p = message.toInt();
    if (p >= 1 && p <= NUM_PATTERNS) currentPattern = p;
  }
  else if (t == "lights/hazard")     hazardActive   = (message == "1");
  else if (t == "lights/turn_left")  turnLeftActive = (message == "1");
  else if (t == "lights/turn_right") turnRightActive = (message == "1");
  else if (t == "lights/brake")      brakeActive    = (message == "1");
  else if (t == "lights/parking")    parkingOn      = (message == "1");
  else if (t == "lights/reverse")    reverseOn      = (message == "1");
}

// --------------------- Main Loop ---------------------
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();

  // Parking lamps (steady low filament)
  digitalWrite(RELAY_PARKING_ALL, parkingOn ? HIGH : LOW);

  // Reverse lamps
  digitalWrite(RELAY_REVERSE_L, reverseOn ? HIGH : LOW);
  digitalWrite(RELAY_REVERSE_R, reverseOn ? HIGH : LOW);

  // Priority logic: Hazard > Turn > Brake
  if (hazardActive) {
    handleHazard();
  }
  else if (turnLeftActive || turnRightActive) {
    handleTurnSignal(now);
  }
  else if (brakeActive) {
    handleBrake();
  }
  else {
    // No active signal → all high filaments off
    allHighOff();
  }

  // Optional: Publish fuel level every 2 seconds
  static unsigned long lastFuel = 0;
  if (now - lastFuel > 2000) {
    lastFuel = now;
    int raw = analogRead(PIN_FUEL_LEVEL);
    float level = map(raw, 0, 4095, 0, 100);  // Adjust mapping to your sender
    client.publish("sensors/fuel_level", String(level).c_str());
  }

  delay(10);
}

// --------------------- All High Filaments Off ---------------------
void allHighOff() {
  digitalWrite(RELAY_REAR_L1_HIGH, LOW);
  digitalWrite(RELAY_REAR_L2_HIGH, LOW);
  digitalWrite(RELAY_REAR_L3_HIGH, LOW);
  digitalWrite(RELAY_REAR_R1_HIGH, LOW);
  digitalWrite(RELAY_REAR_R2_HIGH, LOW);
  digitalWrite(RELAY_REAR_R3_HIGH, LOW);
}

// --------------------- Hazard: All flash together ---------------------
void handleHazard() {
  static bool flashState = false;
  static unsigned long lastFlash = 0;
  if (millis() - lastFlash >= 400) {  // \~2.5 Hz flash
    flashState = !flashState;
    lastFlash = millis();
    digitalWrite(RELAY_REAR_L1_HIGH, flashState);
    digitalWrite(RELAY_REAR_L2_HIGH, flashState);
    digitalWrite(RELAY_REAR_L3_HIGH, flashState);
    digitalWrite(RELAY_REAR_R1_HIGH, flashState);
    digitalWrite(RELAY_REAR_R2_HIGH, flashState);
    digitalWrite(RELAY_REAR_R3_HIGH, flashState);
  }
}

// --------------------- Brake: All high steady ---------------------
void handleBrake() {
  digitalWrite(RELAY_REAR_L1_HIGH, HIGH);
  digitalWrite(RELAY_REAR_L2_HIGH, HIGH);
  digitalWrite(RELAY_REAR_L3_HIGH, HIGH);
  digitalWrite(RELAY_REAR_R1_HIGH, HIGH);
  digitalWrite(RELAY_REAR_R2_HIGH, HIGH);
  digitalWrite(RELAY_REAR_R3_HIGH, HIGH);
}

// --------------------- Turn Signal Sequentials ---------------------
void handleTurnSignal(unsigned long now) {
  if (now - lastStepTime < STEP_INTERVAL) return;
  lastStepTime = now;

  bool left = turnLeftActive;
  bool right = turnRightActive;

  seqStep = (seqStep + 1) % 8;  // Cycle through animation

  allHighOff();  // Clear previous

  switch (currentPattern) {
    case 1: // Pattern 1: Progressive Fill
      if (seqStep == 0) setSide(left, 1, true);
      if (seqStep == 1 || seqStep == 2) setSide(left, 1, true); setSide(left, 2, true);
      if (seqStep >= 3 && seqStep <= 6) setSide(left, 1, true); setSide(left, 2, true); setSide(left, 3, true);
      if (right) { /* mirror for right side */ }
      break;

    case 2: // Pattern 2: Single Chase
      if (seqStep % 3 == 0) setSide(left, 1, true);
      if (seqStep % 3 == 1) setSide(left, 2, true);
      if (seqStep % 3 == 2) setSide(left, 3, true);
      break;

    case 3: // Pattern 3: Reverse Sweep (outer → inner)
      if (seqStep == 0) setSide(left, 3, true);
      if (seqStep == 1) setSide(left, 3, true); setSide(left, 2, true);
      if (seqStep >= 2) setSide(left, 3, true); setSide(left, 2, true); setSide(left, 1, true);
      break;

    case 4: // Pattern 4: Flash 3× then Progressive
      static int flashCount = 0;
      if (seqStep < 6) {  // First 3 flashes (6 half-cycles)
        bool flash = (seqStep % 2 == 0);
        setSide(left, 1, flash); setSide(left, 2, flash); setSide(left, 3, flash);
      } else {
        // Then progressive like pattern 1
        setSide(left, 1, true); setSide(left, 2, true); setSide(left, 3, true);
      }
      break;

    case 5: // Pattern 5: Knight-Rider style wave
      if (seqStep == 0 || seqStep == 6) setSide(left, 1, true);
      if (seqStep == 1 || seqStep == 5) { setSide(left, 1, true); setSide(left, 2, true); }
      if (seqStep == 2 || seqStep == 4) { setSide(left, 1, true); setSide(left, 2, true); setSide(left, 3, true); }
      if (seqStep == 3) setSide(left, 3, true);
      break;
  }

  // Mirror for right side if active
  if (right) {
    // Duplicate logic but for right pins (R1,R2,R3)
    // You can refactor into a function setSide(bool isLeft, int lamp, bool state)
  }
}

// Helper to set one side's lamps
void setSide(bool isLeft, int lampNum, bool state) {
  int pin;
  if (isLeft) {
    if (lampNum == 1) pin = RELAY_REAR_L1_HIGH;
    else if (lampNum == 2) pin = RELAY_REAR_L2_HIGH;
    else pin = RELAY_REAR_L3_HIGH;
  } else {
    if (lampNum == 1) pin = RELAY_REAR_R1_HIGH;
    else if (lampNum == 2) pin = RELAY_REAR_R2_HIGH;
    else pin = RELAY_REAR_R3_HIGH;
  }
  digitalWrite(pin, state ? HIGH : LOW);
}
