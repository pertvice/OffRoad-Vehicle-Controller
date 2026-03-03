#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi/MQTT setup...
// Pin defs for relays, ADC for transducers (e.g. GPIO32 for fuel_pressure 100PSI)
// Example ADC read:
float readPressure(int pin, float range) {
  int raw = analogRead(pin);
  float v = (raw / 4095.0) * 3.3;
  return ((v - 0.5) / 4.0) * range;
}

// In loop: publish "sensors/fuel_pressure" etc.
