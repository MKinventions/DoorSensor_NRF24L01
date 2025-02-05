// #define ESP8266
#define NANO
// #define UNO

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// Conditional compilation to define the LED pin
#ifdef ARDUINO_AVR_NANO        // For Arduino Nano (ATmega328)
#define CE 9
#define CSN 10
#define DOOR A0
#elif defined(ARDUINO_AVR_UNO) // For Arduino Uno (ATmega328)
#define CE 9
#define CSN 10
#define DOOR A0
#else
#define CE 9
#define CSN 10
#define DOOR 8
#endif


RF24 radio(CE, CSN); // CE, CSN
const byte address[6] = "10001";

void NRF24_Init();
void sendData(const char *text);

void softwareReset()
{
  asm volatile("jmp 0"); // Jump to address 0 (resets the MCU)
}

unsigned long lastSystemInfoTime = 0;
const unsigned long systemInfoInterval = 30000; // 30 seconds

// ##########################################
#define DOOR_COUNT_ADDR 10
uint8_t doorOpenCount = 0;
bool previousDoorState = LOW; // Track previous door state
bool doorWasOpen = false;     // Tracks if the door was open before closing

void setup()
{
  Serial.begin(9600);
  Serial.println("");
  pinMode(DOOR, INPUT_PULLUP);

  doorOpenCount = EEPROM.read(DOOR_COUNT_ADDR);
  Serial.println("OpenCount:" + String(doorOpenCount));
  NRF24_Init();
}

void loop()
{

  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 200;

  int doorState = !digitalRead(DOOR); // Invert because of INPUT_PULLUP

  if (doorState == HIGH)
  {                     // If door is OPEN
    doorWasOpen = true; // Mark that door was open
  }

  if (doorState == LOW && previousDoorState == HIGH && doorWasOpen)
  { // Detect falling edge
    if (millis() - lastDebounceTime > debounceDelay)
    {
      lastDebounceTime = millis();

      doorOpenCount++; // Count only when door closes
      EEPROM.update(DOOR_COUNT_ADDR, doorOpenCount);

      Serial.println("Door Closed! Incrementing count.");
      Serial.println("Updated OpenCount: " + String(doorOpenCount));

      doorWasOpen = false; // Reset flag after counting
    }
  }

  StaticJsonDocument<32> node;
  node["ID"] = "10D";
  node["DS"] = doorState;
  node["DC"] = doorOpenCount;

  char jsonBuffer[32];
  serializeJson(node, jsonBuffer);
  sendData(jsonBuffer);

  // Send systemInfo JSON every 30 seconds
  if (millis() - lastSystemInfoTime >= systemInfoInterval)
  {
    lastSystemInfoTime = millis();

    StaticJsonDocument<32> systemInfo;
    systemInfo["FW"] = 10;
    systemInfo["RC"] = 255;
    // Convert JSON to a string
    char jsonBuffer3[32]; // Ensure it's <= 32 bytes
    // size_t jsonLength3 = serializeJson(systemInfo, jsonBuffer3);
    sendData(jsonBuffer3);
  }
}

void sendData(const char *text)
{
  // Static variable to store the last time the data was sent
  static unsigned long previousMillis = 0;
  // Interval in milliseconds (e.g., 1000 ms = 1 second)
  static const long interval = 100;

  // Check if the payload size exceeds 31 bytes (32 bytes includes the null terminator)
  if (strlen(text) > 31) // Allow up to 31 characters + null terminator
  {
    Serial.println("Payload Size Limit Exceeded! [" + String(strlen(text)) + "] Bytes");
    while (1)
      ; // Halt the program if payload is too large
  }
  else
  {
    // Get the current time using millis()
    unsigned long currentMillis = millis();

    // Check if enough time has passed since the last send
    if (currentMillis - previousMillis >= interval)
    {
      // Store the current time to calculate the next interval
      previousMillis = currentMillis;

      // Send the data, including the null terminator
      bool success = radio.write(text, strlen(text) + 1); // +1 to include null terminator

      if (success)
      {
        Serial.println("Payload Size: [" + String(strlen(text) + 1) + "] Bytes");
        // Serial.println("Send Success!");
        Serial.println(text);
      }
      else
      {
        static int i = 0;
        Serial.println("Send Failed or Master disconnected!");
        Serial.print("connecting");
        while (i < 120)
        {
          i++;
          Serial.print(".");
          delay(1000);

          if (i == 10)
          {

            softwareReset();
          }
        }
      }
    }
  }
}

void NRF24_Init()
{
  if (!radio.begin())
  {
    Serial.println("NRF24L01 Initialisation Failed!.");
  }

  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();
  Serial.println("NRF24L01 Initialisation Success!.");
}