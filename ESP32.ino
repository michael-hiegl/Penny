#define LED_GREEN 2
#define TASTLED 26
#define POTI 34
#define TASTER 39
#define MAX_PIN_LENGTH 18
#define DEBOUNCE_DELAY 50
#define PCF8574_ADDRESS 0x20
#define LOOP_PERIOD 35              // Display updates every 35 ms
#define MAX_ANALOG_INPUT_VALUE 1024 // Max Input Value of the Potentiometer
#define M_SIZE 1.3333               // Factor to scale the Analog Meter over the whole TFT
#define TFT_GREY 0x5AEB

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

const uint8_t newMacAddress[] = {0x94, 0x3C, 0xC6, 0x33, 0x68, 0x01};   // Macadresse, die esp32 zugewiesen wird
const uint8_t receiverAddress[] = {0x96, 0x3B, 0xC7, 0x34, 0x69, 0x02}; // Macadresse von esp8266

esp_now_peer_info_t peerInfo; // struct mit informationen über esp8266 wird erzeugt
uint16_t potiwert;

const String masterPin = "09913615516";

Preferences preferences;
TFT_eSPI tft = TFT_eSPI();

float ltx = 0;                 // Saved x coord of bottom of needle
uint16_t osx = 120, osy = 120; // Saved x & y coords
uint32_t updateTime = 0;       // time for next update
int old_analog = -999;         // Value last displayed

// Functions //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void messageReceived(const uint8_t *macAddr, const uint8_t *incomingData, int len)
{
  potiwert = (incomingData[0] << 8) + incomingData[1];
}

void sendRelayOpen(bool value)
{
  char message_on[] = "E";
  char message_off[] = "A";

  if (!value)
  {
    esp_now_send(receiverAddress, (uint8_t *)message_off, sizeof(message_off));
  }
  else
  {
    esp_now_send(receiverAddress, (uint8_t *)message_on, sizeof(message_off));
  }
}

void matrixLoop()
{
  static String inputPin = "";
  static char prevPressedKey = 0;
  static unsigned long lastDebounceTime = 0;
  static bool inChangePin = false;
  char pressedKey = getPressedKey();

  if (prevPressedKey == pressedKey)
    return;
  prevPressedKey = pressedKey;

  if ((millis() - lastDebounceTime) <= DEBOUNCE_DELAY)
    return;
  lastDebounceTime = millis();

  // Not a valid key for pin
  if (pressedKey == 0)
    return;

  // TODO: Add indicator for successfully register a new key

  if (pressedKey == '*')
  {
    // Reset pin
    inputPin = "";
    updatePin(inputPin);

    return;
  }
  else if (pressedKey == '#')
  {
    if (!inChangePin)
    {
      if (inputPin == masterPin || inputPin == getPin())
      {
        sendRelayOpen(true);
        printMessage("Relay geoeffnet", TFT_GREEN);
      }
      else if (inputPin == "")
      {
        sendRelayOpen(false);
        printMessage("Relay geschlossen", TFT_BLACK);
      }
      else
      {
        printMessage("Falscher Pin", TFT_RED);
      }
    }
    else
    {
      static String checkChangePin = "";

      if (checkChangePin != "")
      {
        if (checkChangePin == inputPin)
        {
          setPin(inputPin); // save user pin to flash
          printMessage("Pin erfolgreich geaendert", TFT_GREEN);
        }
        else
        {
          // Pin are not the same
          printMessage("Pin stimmt nicht überein", TFT_RED);
        }
        checkChangePin = "";

        // Exit pin change state
        inChangePin = false;
      }
      else
      {
        printMessage("Pin erneut eingeben", TFT_BLACK);
        checkChangePin = inputPin;
      }
    }

    // Reset pin
    inputPin = "";
    updatePin(inputPin);

    return;
  }
  else if (pressedKey == 'A' || pressedKey == 'B' || pressedKey == 'C' || pressedKey == 'D')
  {
    if (pressedKey == 'B')
    {
      inChangePin = true;
      printMessage("Neuen Code eingeben", TFT_BLACK);
    }

    return;
  }

  if (inputPin.length() >= MAX_PIN_LENGTH) // Max Pin Length reached
    return;

  inputPin += pressedKey;
  Serial.println(inputPin);
  updatePin(inputPin);
  if (!inChangePin)
    printMessage("", TFT_WHITE); // Message löschen
}

void setPin(String pin)
{
  preferences.begin("lock", false); // start preferences with namespace "schloss" and read/write pernission

  if (pin != preferences.getString("pin", "")) // check if pin is different to avoid unnecessary writes
  {
    preferences.putString("pin", pin); // save pin
  }

  preferences.end(); // close preferences
}

String getPin()
{
  String pinTemp;

  preferences.begin("lock", true);            // start preferences with namespace "schloss" and read pernission
  pinTemp = preferences.getString("pin", ""); // read pin
  preferences.end();                          // close preferences
  return pinTemp;
}

char getPressedKey()
{
  for (char row = 0; row < 4; row++)
  {
    // Send command to set row pin to 0
    Wire.beginTransmission(PCF8574_ADDRESS);
    Wire.write(0xFF & ~(1 << (4 + row)));
    Wire.endTransmission();

    // Check for pressed key in row
    Wire.requestFrom(PCF8574_ADDRESS, 1);
    if (!Wire.available())
      continue;

    char data = Wire.read();
    if ((data & 0x0F) != 0b1111)
    {
      switch (data)
      {
      case 0xEE:
        return '1';
      case 0xED:
        return '2';
      case 0xEB:
        return '3';
      case 0xE7:
        return 'A';

      case 0xDE:
        return '4';
      case 0xDD:
        return '5';
      case 0xDB:
        return '6';
      case 0xD7:
        return 'B';

      case 0xBE:
        return '7';
      case 0xBD:
        return '8';
      case 0xBB:
        return '9';
      case 0xB7:
        return 'C';

      case 0x7E:
        return '*';
      case 0x7D:
        return '0';
      case 0x7B:
        return '#';
      case 0x77:
        return 'D';

      default:
        Serial1.print("Received data: ");
        Serial.println(data, HEX);
        break;
      }
    }
  }

  return 0;
}

void TFT_Init()
{
  tft.init();
  tft.setRotation(1);        // Rotates the display by 90 degrees
  tft.fillScreen(TFT_BLACK); // Fill the Backround with Black
  analogMeter();

  updateTime = millis();
}

void DisplayValue(u_int16_t PotiValue)
{
  int value = map(PotiValue, 0, MAX_ANALOG_INPUT_VALUE, 0, 100); // Maps the Value of the Potentiometer for the "plotNeedle" function

  // Updates the Needle after every 35 seconds, which is defined by "LOOP_PERIOD"
  if (updateTime <= millis())
  {
    updateTime = millis() + LOOP_PERIOD;
    plotNeedle(value);
  }
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void analogMeter() // Funktion to plot the Basic Analog Meter
{

  // Meter outline
  tft.fillRect(0, 0, M_SIZE * 239, 240, TFT_GREY);
  tft.fillRect(5, 3, M_SIZE * 230, 230, TFT_WHITE);
  tft.drawLine(0, 160, 420, 160, TFT_GREY);

  tft.setTextColor(TFT_BLACK); // Text colour
  tft.drawString("Code:", 10, 170, 4);

  // Draw ticks every 5 degrees from -50 to +50 degrees (100 deg. FSD swing)
  for (int i = -50; i < 51; i += 5)
  {
    // Long scale tick length
    int tl = 15;

    // Coordinates of tick to draw
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (M_SIZE * 100 + tl) + M_SIZE * 120;
    uint16_t y0 = sy * (M_SIZE * 100 + tl) + M_SIZE * 140;
    uint16_t x1 = sx * M_SIZE * 100 + M_SIZE * 120;
    uint16_t y1 = sy * M_SIZE * 100 + M_SIZE * 140;

    // Coordinates of next tick for zone fill
    float sx2 = cos((i + 5 - 90) * 0.0174532925);
    float sy2 = sin((i + 5 - 90) * 0.0174532925);
    int x2 = sx2 * (M_SIZE * 100 + tl) + M_SIZE * 120;
    int y2 = sy2 * (M_SIZE * 100 + tl) + M_SIZE * 140;
    int x3 = sx2 * M_SIZE * 100 + M_SIZE * 120;
    int y3 = sy2 * M_SIZE * 100 + M_SIZE * 140;

    // GREEN zone limits
    if (i >= -50 && i < -25)
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREEN);
    }

    // Yellow zone limits
    if (i >= -25 && i < 0)
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_YELLOW);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
    }

    // Orange zone limits
    if (i >= 0 && i < 25)
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_ORANGE);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_ORANGE);
    }

    // Red zone limits
    if (i >= 25 && i < 50)
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_RED);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_RED);
    }

    // Short scale tick length
    if (i % 25 != 0)
      tl = 8;

    // Recalculate coords incase tick lenght changed
    x0 = sx * (M_SIZE * 100 + tl) + M_SIZE * 120;
    y0 = sy * (M_SIZE * 100 + tl) + M_SIZE * 140;
    x1 = sx * M_SIZE * 100 + M_SIZE * 120;
    y1 = sy * M_SIZE * 100 + M_SIZE * 140;

    // Draw tick
    tft.drawLine(x0, y0, x1, y1, TFT_BLACK);

    // Check if labels should be drawn, with position tweaks
    if (i % 25 == 0)
    {
      // Calculate label positions
      x0 = sx * (M_SIZE * 100 + tl + 10) + M_SIZE * 120;
      y0 = sy * (M_SIZE * 100 + tl + 10) + M_SIZE * 140;
      switch (i / 25)
      {
      case -2: // at -50 degrees
        tft.drawCentreString("0%", x0, y0 - 12, 2);
        break;
      case -1: // at -25 degrees
        tft.drawCentreString("25%", x0, y0 - 9, 2);
        break;
      case 0: // at 0 degrees
        tft.drawCentreString("50%", x0, y0 - 7, 2);
        break;
      case 1: // at 25 degrees
        tft.drawCentreString("75%", x0, y0 - 9, 2);
        break;
      case 2: // at 50 degrees
        tft.drawCentreString("100%", x0, y0 - 12, 2);
        break;
      }
    }

    // Now draw the arc of the scale
    sx = cos((i + 5 - 90) * 0.0174532925);
    sy = sin((i + 5 - 90) * 0.0174532925);
    x0 = sx * M_SIZE * 100 + M_SIZE * 120;
    y0 = sy * M_SIZE * 100 + M_SIZE * 140;
    // Draw scale arc, don't draw the last part
    if (i < 50)
      tft.drawLine(x0, y0, x1, y1, TFT_BLACK);
  }

  tft.drawCentreString("Poti Value", M_SIZE * 120, M_SIZE * 70, 4); // Draw Center "Poti Value" in the Middle of the Screen

  plotNeedle(0); // Put meter needle at 0
}

void plotNeedle(int value)
{
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  String RightString = String(mapFloat(value, 0, 100, 0, 3.3)) + "V";
  tft.drawRightString(RightString, 50, 120, 2);

  if (value < -10)
    value = -10; // Limit value to emulate needle end stops
  if (value > 110)
    value = 110;

  // Move the needle until new value reached

  old_analog = value; // Update immediately if delay is 0

  float sdeg = map(old_analog, -10, 110, -150, -30); // Map value to angle
  // Calculate tip of needle coords
  float sx = cos(sdeg * 0.0174532925);
  float sy = sin(sdeg * 0.0174532925);

  // Calculate x delta of needle start (does not start at pivot point)
  float tx = tan((sdeg + 90) * 0.0174532925);

  // Erase old needle image
  tft.drawLine(M_SIZE * (120 + 20 * ltx - 1), M_SIZE * (140 - 20), osx - 1, osy, TFT_WHITE);
  tft.drawLine(M_SIZE * (120 + 20 * ltx), M_SIZE * (140 - 20), osx, osy, TFT_WHITE);
  tft.drawLine(M_SIZE * (120 + 20 * ltx + 1), M_SIZE * (140 - 20), osx + 1, osy, TFT_WHITE);

  // Re-plot text under needle
  tft.setTextColor(TFT_BLACK);
  tft.drawCentreString("Poti Value", M_SIZE * 120, M_SIZE * 70, 4);

  // Store new needle end coords for next erase
  ltx = tx;
  osx = M_SIZE * (sx * 98 + 120);
  osy = M_SIZE * (sy * 98 + 140);

  // draws 3 lines to thicken needle
  tft.drawLine(M_SIZE * (120 + 20 * ltx - 1), M_SIZE * (140 - 20), osx - 1, osy, TFT_RED);
  tft.drawLine(M_SIZE * (120 + 20 * ltx), M_SIZE * (140 - 20), osx, osy, TFT_MAGENTA);
  tft.drawLine(M_SIZE * (120 + 20 * ltx + 1), M_SIZE * (140 - 20), osx + 1, osy, TFT_RED);
}

void updatePin(String Input_Pin)
{
  tft.fillRect(90, 165, 220, 30, TFT_WHITE); // Erase old code
  String DisplayString = "";
  if (Input_Pin.length() == 0) // Return when String is empty
    return;

  for (int i = 0; i < Input_Pin.length() - 1; i++) // Set "*" for every char in the String exepct the last one
  {
    DisplayString += "*";
  }

  DisplayString += Input_Pin.charAt(Input_Pin.length() - 1); // Add last char of the String

  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString(DisplayString, 90, 170, 4); // Display String
}

void printMessage(String Message, uint16_t color)
{
  tft.fillRect(10, 200, 300, 30, TFT_WHITE); // Erase old Message
  tft.setTextColor(color, TFT_WHITE);
  tft.drawString(Message, 10, 205, 4);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);
  // pinMode(TASTLED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(TASTER, INPUT);
  WiFi.mode(WIFI_STA);                          // esp32 wird in station modus versetzt
  esp_wifi_set_mac(WIFI_IF_STA, newMacAddress); // esp32 wird neue Mac zugewiesen

  if (esp_now_init() != ESP_OK)
  {
    digitalWrite(LED_GREEN, LOW);
  }
  else
  {
    digitalWrite(LED_GREEN, HIGH);
  }

  memcpy(peerInfo.peer_addr, receiverAddress, 6); // kopieren der esp8266-Mac in peer_addr
  peerInfo.channel = 0;                           // WLAN channel auf 0 setzen
  peerInfo.encrypt = false;                       // verschlüsselung für nachrichten deaktivieren

  esp_now_add_peer(&peerInfo); // esp8266 wird als kommunikationspartner hinzugefügt

  // esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(messageReceived);

  // Initialize communication for keypad
  Wire.begin(21, 22);

  TFT_Init();
}

void loop()
{
  static uint16_t currentValue = 0;
  if (currentValue != potiwert)
  {
    currentValue = potiwert;
    DisplayValue(potiwert);
  }

  matrixLoop();
}