#define RELAISPIN 16
#define POTI A0

#include <ESP8266WiFi.h>
#include <espnow.h>

uint8_t newMacAddress[] = { 0x96, 0x3B, 0xC7, 0x34, 0x69, 0x02 };
uint8_t receiverAddress[] = { 0x94, 0x3C, 0xC6, 0x33, 0x68, 0x01 };

// esp_now_peer_info_t peerInfo;

// void OnDataSent(uint8_t* mac_addr, uint8_t sendStatus) {
//   if (sendStatus == 0) {
//   } else {
//     digitalWrite(LED_BUILTIN, HIGH);
//     delay(500);
//   }
// }

void onoff(uint8_t* macAddr, uint8_t* incomingData, uint8_t len) {
  if (incomingData[0] == 'E') {
    digitalWrite(RELAISPIN, HIGH);
  }
  if (incomingData[0] == 'A') {
    digitalWrite(RELAISPIN, LOW);
  }
}

void setup() {
  pinMode(RELAISPIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  WiFi.mode(WIFI_STA);
  wifi_set_macaddr(STATION_IF, newMacAddress);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

  // memcpy(peerInfo.peer_addr, receiverAddress, 6);
  // peerInfo.channel = 0;
  // peerInfo.encrypt = false;

  // esp_now_add_peer(&peerInfo);
  esp_now_add_peer(receiverAddress, ESP_NOW_ROLE_COMBO, 0, NULL, 0);

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  // esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(onoff);
}

void loop() {
  uint8_t potiwert = analogRead(POTI) / 5;
  // char mess[] = "Hello";
  esp_now_send(receiverAddress, (uint8_t*)&potiwert, sizeof(potiwert));
  delay(200);
}