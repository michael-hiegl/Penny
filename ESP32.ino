#define LED_GREEN 2
#define TASTLED 26
#define POTI 34
#define TASTER 39

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

const uint8_t newMacAddress[] = { 0x94, 0x3C, 0xC6, 0x33, 0x68, 0x01 };    // Macadresse, die esp32 zugewiesen wird
const uint8_t receiverAddress[] = { 0x96, 0x3B, 0xC7, 0x34, 0x69, 0x02 };  // Macadresse von esp8266

esp_now_peer_info_t peerInfo;  // struct mit informationen über esp8266 wird erzeugt
float potiwert;

// Functions //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
//   if (sendStatus == 0) {
//     digitalWrite(TASTLED, LOW);
//     delay(200);
//     digitalWrite(TASTLED, HIGH);
//   } else {
//     digitalWrite(TASTLED, HIGH);
//   }
// }

void messageReceived(const uint8_t* macAddr, const uint8_t* incomingData, int len) {
  potiwert = *incomingData / 61.8;
}

void lock() {
  char message_on[] = "E";
  char message_off[] = "A";

  if (analogRead(POTI) <= 2047) {
    esp_now_send(receiverAddress, (uint8_t*)message_off, sizeof(message_off));
  } else {
    esp_now_send(receiverAddress, (uint8_t*)message_on, sizeof(message_off));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  // pinMode(TASTLED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(TASTER, INPUT);
  WiFi.mode(WIFI_STA);                           // esp32 wird in station modus versetzt
  esp_wifi_set_mac(WIFI_IF_STA, newMacAddress);  // esp32 wird neue Mac zugewiesen

  if (esp_now_init() != ESP_OK) {
    digitalWrite(LED_GREEN, LOW);
  } else {
    digitalWrite(LED_GREEN, HIGH);
  }

  memcpy(peerInfo.peer_addr, receiverAddress, 6);  // kopieren der esp8266-Mac in peer_addr
  peerInfo.channel = 0;                            // WLAN channel auf 0 setzen
  peerInfo.encrypt = false;                        // verschlüsselung für nachrichten deaktivieren

  esp_now_add_peer(&peerInfo);  // esp8266 wird als kommunikationspartner hinzugefügt

  // esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(messageReceived);
}

void loop() {
  // char message_on[] = "E";
  // char message_off[] = "A";

  // if (analogRead(POTI) <= 2047) {
  //   esp_now_send(receiverAddress, (uint8_t *)message_off, sizeof(message_off));
  // } else {
  //   esp_now_send(receiverAddress, (uint8_t *)message_on, sizeof(message_off));
  // }

  if (digitalRead(TASTER) == HIGH) {
    lock();
  } else {
    Serial.println(potiwert);
    delay(500);
  }



  // esp_now_send(receiverAddress, (uint8_t *)message_on, sizeof(message_off));
  // delay(1000);
  // esp_now_send(receiverAddress, (uint8_t *)message_off, sizeof(message_off));
  // delay(1000);
}