// esp32_can_test.ino - Basic CAN send/receive test for ESP32
// Uses built-in TWAI (ESP32 CAN controller)

#include <CAN.h>

#define CAN_TX_PIN 5   // Change to your TX pin
#define CAN_RX_PIN 4   // Change to your RX pin

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 CAN Test");

  CAN.setPins(CAN_RX_PIN, CAN_TX_PIN);
  if (!CAN.begin(500E3)) {
    Serial.println("CAN init failed");
    while (1);
  }
  Serial.println("CAN initialized @ 500 kbps");
}

void loop() {
  // Send example message every 2 seconds
  CAN.beginPacket(0x100);
  CAN.write(0xAA);
  CAN.write(0xBB);
  CAN.endPacket();
  Serial.println("Sent CAN ID 0x100 Data: AA BB");

  // Receive
  int packetSize = CAN.parsePacket();
  if (packetSize) {
    Serial.print("Received ID 0x");
    Serial.print(CAN.packetId(), HEX);
    Serial.print(" Data: ");
    while (CAN.available()) {
      Serial.print(CAN.read(), HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  delay(2000);
}
