// front.ino - Front Module ESP32 Code for Off-Road Vehicle Controller
// Controls: headlights (Probe motors), halos, beams, fans, water pump, fuel pump, starter, wipers, blower
// Sensors: 3 pressure transducers (fuel 100 PSI, oil 150 PSI, trans/boost 150 PSI), RPM from distributor
// Comms: MQTT (WiFi) + placeholder for MCP2515 CAN (Killshot EFI)
// Libraries: Install via Arduino Library Manager: WiFi, PubSubClient, ArduinoJson

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --------------------- WiFi & MQTT Config ---------------------
const char* ssid = "YourWiFiSSID";          // ← Change
const char* password = "YourWiFiPassword";  // ← Change
const char* mqtt_server = "192.168.4.1";    // RPi5 hotspot IP or localhost if Pi hosts broker
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// --------------------- Pin Definitions (adjust as needed) ---------------------
// Relays (active HIGH, optocoupler modules)
const int RELAY_HEAD_OPEN_L   = 2;   // Left headlight motor open
const int RELAY_HEAD_CLOSE_L  = 4;
const int RELAY_HEAD_OPEN_R   = 5;
const int RELAY_HEAD_CLOSE_R  = 18;
const int RELAY_HALO_ALL      = 19;  // All 4 halos
const int RELAY_PARKING_ALL   = 21;  // Front parking lamps
const int RELAY_LOW_OUTER     = 22;  // Outer low beams L+R
const int RELAY_HIGH_OUTER    = 23;
const int RELAY_LOW_INNER     = 25;
const int RELAY_HIGH_INNER    = 26;
const int RELAY_FAN_LOW       = 27;
const int RELAY_FAN_HIGH      = 14;
const int RELAY_WATER_PUMP    = 12;
const int RELAY_FUEL_PUMP     = 13;
const int RELAY_STARTER       = 15;  // Ford solenoid relay
const int RELAY_WIPER_LOW     = 16;
const int RELAY_WIPER_HIGH    = 17;
const int RELAY_BLOWER_LOW    = 32;
const int RELAY_BLOWER_MED    = 33;
const int RELAY_BLOWER_HIGH   = 35;  // or use resistor taps if needed

// Sensors
const int PIN_COOLANT_TEMP    = 34;  // Analog (NTC or sender → voltage divider)
const int PIN_OIL_PRESS       = 35;  // 150 PSI transducer
const int PIN_FUEL_PRESS      = 32;  // 100 PSI transducer
const int PIN_TRANS_PRESS     = 33;  // 150 PSI transducer
const int PIN_RPM_INPUT       = 25;  // Interrupt from Gambler hall effect (or coil neg)

// Inputs
const int PIN_IGNITION_SW     = 26;  // Ignition switched 12V → opto or divider to 3.3V

// --------------------- Constants & Variables ---------------------
const unsigned long MOTOR_PULSE_TIME = 4500;   // ms for Probe motor open/close
const unsigned long FUEL_PRIME_TIME  = 6000;   // 6 seconds prime
const float FAN_LOW_SETPOINT  = 80.0;          // °C (menu adjustable via MQTT)
const float FAN_HIGH_SETPOINT = 95.0;

bool headlampsOn = false;
bool highbeamOn = false;
bool haloOn = false;
bool parkingOn = false;
bool fanOverride = false;
bool waterManual = false;
bool fuelManual = false;

unsigned long motorStartTime = 0;
bool motorOpening = false;
bool motorClosing = false;

unsigned long primeStartTime = 0;
bool priming = false;

volatile unsigned long lastRPM micros = 0;
volatile int rpm = 0;

// Pressure ranges
const float FUEL_RANGE_PSI = 100.0;
const float OIL_RANGE_PSI  = 150.0;
const float TRANS_RANGE_PSI = 150.0;

// --------------------- Setup ---------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  // Relays as output, off
  pinMode(RELAY_HEAD_OPEN_L, OUTPUT); digitalWrite(RELAY_HEAD_OPEN_L, LOW);
  // ... repeat for all relays ...

  // Sensors
  analogReadResolution(12);  // ESP32 default is 12-bit

  // RPM interrupt (falling edge from hall)
  pinMode(PIN_RPM_INPUT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_RPM_INPUT), rpmISR, FALLING);

  // Ignition input
  pinMode(PIN_IGNITION_SW, INPUT);

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
    String clientId = "FrontESP32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("front/#");          // All front commands
      client.subscribe("lights/head_on");
      client.subscribe("engine/fan_override");
      // ... more topics
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

// --------------------- MQTT Callback (commands from Pi) ---------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  String t = String(topic);

  if (t == "lights/head_on") {
    headlampsOn = (message == "1");
    if (headlampsOn) openHeadlights();
    else closeHeadlights();
  }
  else if (t == "lights/highbeam") highbeamOn = (message == "1");
  else if (t == "lights/halo") haloOn = (message == "1");
  else if (t == "lights/parking") parkingOn = (message == "1");
  else if (t == "engine/fan_override") fanOverride = (message == "1");
  else if (t == "engine/water_manual") waterManual = (message == "1");
  else if (t == "engine/fuel_manual") fuelManual = (message == "1");
  // Add fan setpoint update, wiper, blower levels, etc.
}

// --------------------- RPM Interrupt ---------------------
void IRAM_ATTR rpmISR() {
  unsigned long now = micros();
  if (now - lastRPMmicros > 10000) {  // debounce
    rpm = 60000000UL / (now - lastRPMmicros);  // RPM assuming 1 pulse/rev
    lastRPMmicros = now;
  }
}

// --------------------- Motor Control ---------------------
void openHeadlights() {
  digitalWrite(RELAY_HEAD_OPEN_L, HIGH);
  digitalWrite(RELAY_HEAD_OPEN_R, HIGH);
  motorStartTime = millis();
  motorOpening = true;
  motorClosing = false;
  digitalWrite(RELAY_PARKING_ALL, HIGH);  // Parking auto-on
  digitalWrite(RELAY_HALO_ALL, HIGH);     // Halo auto-on if desired
}

void closeHeadlights() {
  digitalWrite(RELAY_HEAD_CLOSE_L, HIGH);
  digitalWrite(RELAY_HEAD_CLOSE_R, HIGH);
  motorStartTime = millis();
  motorClosing = true;
  motorOpening = false;
}

// --------------------- Main Loop ---------------------
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();

  // Motor timeout (internal limit switches stop them)
  if ((motorOpening || motorClosing) && (now - motorStartTime >= MOTOR_PULSE_TIME)) {
    digitalWrite(RELAY_HEAD_OPEN_L, LOW);
    digitalWrite(RELAY_HEAD_OPEN_R, LOW);
    digitalWrite(RELAY_HEAD_CLOSE_L, LOW);
    digitalWrite(RELAY_HEAD_CLOSE_R, LOW);
    motorOpening = motorClosing = false;
  }

  // Fuel prime on ignition on
  bool ignitionOn = digitalRead(PIN_IGNITION_SW) == HIGH;  // Adjust logic level
  if (ignitionOn && !priming && !client.connected()) {  // First detect
    digitalWrite(RELAY_FUEL_PUMP, HIGH);
    primeStartTime = now;
    priming = true;
  }
  if (priming && (now - primeStartTime >= FUEL_PRIME_TIME)) {
    priming = false;
  }

  // Engine running logic
  bool engineRunning = (rpm > 300);  // From distributor
  if (engineRunning || fuelManual) {
    digitalWrite(RELAY_FUEL_PUMP, HIGH);
  } else if (!priming && !ignitionOn) {
    digitalWrite(RELAY_FUEL_PUMP, LOW);
  }

  // Water pump
  if (engineRunning || waterManual) {
    digitalWrite(RELAY_WATER_PUMP, HIGH);
  } else {
    digitalWrite(RELAY_WATER_PUMP, LOW);
  }

  // Fans (example temp logic - replace with real sender read)
  float coolantTemp = 75.0;  // Placeholder - read from analog
  bool fanLow = (coolantTemp >= FAN_LOW_SETPOINT) || fanOverride;
  bool fanHigh = (coolantTemp >= FAN_HIGH_SETPOINT) || fanOverride;
  digitalWrite(RELAY_FAN_LOW, fanLow ? HIGH : LOW);
  digitalWrite(RELAY_FAN_HIGH, fanHigh ? HIGH : LOW);

  // Pressure readings & publish every 500 ms
  static unsigned long lastSensor = 0;
  if (now - lastSensor > 500) {
    lastSensor = now;

    float fuelPsi  = readPressure(PIN_FUEL_PRESS, FUEL_RANGE_PSI);
    float oilPsi   = readPressure(PIN_OIL_PRESS, OIL_RANGE_PSI);
    float transPsi = readPressure(PIN_TRANS_PRESS, TRANS_RANGE_PSI);

    StaticJsonDocument<256> doc;
    doc["fuel_pressure"] = fuelPsi;
    doc["oil_pressure"] = oilPsi;
    doc["trans_pressure"] = transPsi;
    doc["rpm"] = rpm;

    char buffer[256];
    serializeJson(doc, buffer);
    client.publish("sensors/front", buffer);
  }

  // Headlight beams
  digitalWrite(RELAY_LOW_OUTER, headlampsOn ? HIGH : LOW);
  digitalWrite(RELAY_LOW_INNER, headlampsOn ? HIGH : LOW);
  digitalWrite(RELAY_HIGH_OUTER, highbeamOn ? HIGH : LOW);
  digitalWrite(RELAY_HIGH_INNER, highbeamOn ? HIGH : LOW);

  // Halo & parking
  digitalWrite(RELAY_HALO_ALL, haloOn ? HIGH : LOW);
  digitalWrite(RELAY_PARKING_ALL, parkingOn ? HIGH : LOW);

  delay(10);  // Light loop delay
}

// --------------------- Pressure Read Helper ---------------------
float readPressure(int pin, float rangePsi) {
  int raw = analogRead(pin);
  float volts = (raw / 4095.0) * 3.3;
  // 0.5V → 0 PSI, 4.5V → rangePsi
  float psi = ((volts - 0.5) / 4.0) * rangePsi;
  return constrain(psi, 0.0, rangePsi);
}
