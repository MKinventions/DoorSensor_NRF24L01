#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

bool doorStateLed = false;
bool nodeStateLed = false;

RF24 radio(CE, CSN);
const byte address[6] = "10001";

// Constants
const char slaveID[] = "10D";
const int firmwareVersion = 13; // version:1.2
const int doorAddr = 0x11;
const int restartCounterAddr = 0x12; // EEPROM address for restart counter
const int openDurationAddr = 0x13;

// Variables
uint8_t doorOpenCounter = 0;
uint8_t restartCounter = 0;
bool previousDoorState = LOW;

unsigned long startOpenTimer = 0;
unsigned long openDuration = 0;
bool doorPreviouslyOpen = false; // Track previous state

// Timing variables
unsigned long lastSystemInfoTime = 0;
const unsigned long systemInfoInterval = 30000; // 30 seconds

void NRF24_Init();
void sendDoorStatus(int doorSensor);
void sendSystemInfo();
void sendData(const char *text);
void handleSystemInfo();

void setup()
{
  Serial.begin(9600);
  pinMode(DOOR, INPUT_PULLUP);
  pinMode(DOOR_LED, OUTPUT);
  pinMode(NODE_LED, OUTPUT);

  // Read and update Restart Counter
  EEPROM.get(restartCounterAddr, restartCounter);
  if (restartCounter == 0xFF)
    restartCounter = 0; // Initialize if uninitialized
  EEPROM.put(restartCounterAddr, ++restartCounter);
  Serial.println("Device Restart Count: " + String(restartCounter));

  // Read and Initialize Door Counter
  EEPROM.get(doorAddr, doorOpenCounter);
  if (doorOpenCounter == 0xFF)
    doorOpenCounter = 0; // Initialize if uninitialized
  Serial.println("Restored Door Count: " + String(doorOpenCounter));

  // Read stored duration from EEPROM
  EEPROM.get(openDurationAddr, openDuration);
  // Validate EEPROM data (if uninitialized, set to 0)
  if (openDuration == 0xFFFFFFFF || openDuration > 86400)
  { // 86400 = 24 hours in seconds
    openDuration = 0;
    EEPROM.put(openDurationAddr, openDuration);
  }
  Serial.println("Read Duration from EEPROM: " + String(openDuration) + "s");

  NRF24_Init();
}

void loop()
{
  int doorSensor = !digitalRead(DOOR); // Invert due to INPUT_PULLUP
  doorStateLed = (doorSensor == 1) ? true : false;
  digitalWrite(DOOR_LED, doorStateLed);

  // Detect door opening (Rising Edge)
  if (doorSensor == HIGH && previousDoorState == LOW)
  {
    doorStateLed = true;
    EEPROM.put(doorAddr, ++doorOpenCounter); // Increment and store only if changed
    Serial.println("Door Opened! Count: " + String(doorOpenCounter));

    startOpenTimer = millis(); // Start timing
    doorPreviouslyOpen = true;
  }

  // Detect door closing (Falling Edge)
  else if (doorSensor == LOW && previousDoorState == HIGH)
  {
    openDuration = (millis() - startOpenTimer) / 1000; // Calculate duration in seconds
    Serial.println("Door Closed! Duration: " + String(openDuration) + "s");

    // Store duration in EEPROM
    EEPROM.put(openDurationAddr, openDuration);
    Serial.println("Duration saved to EEPROM!");
    /*******************************************/
    StaticJsonDocument<32> doc;
    doc["DU"] = openDuration;  
    char jsonBuffer[32];
    serializeJson(doc, jsonBuffer);
    sendData(jsonBuffer);
    /*******************************************/

    startOpenTimer = 0;
    doorPreviouslyOpen = false;
  }

  previousDoorState = doorSensor; // Update previous state

  digitalWrite(NODE_LED, nodeStateLed);
  sendDoorStatus(doorSensor);
  delay(100);

  // Handle system info sending at defined interval
  handleSystemInfo();
}

void handleSystemInfo()
{
  // Send System Info every 10 seconds
  if (millis() - lastSystemInfoTime >= systemInfoInterval)
  {
    lastSystemInfoTime = millis();
    sendSystemInfo();
  }
}

void sendDoorStatus(int doorSensor)
{
  StaticJsonDocument<32> doc;
  doc["ID"] = slaveID;
  doc["DS"] = doorSensor;
  doc["DC"] = doorOpenCounter;

  char jsonBuffer[32];
  serializeJson(doc, jsonBuffer);
  sendData(jsonBuffer);
}

void sendSystemInfo()
{
  StaticJsonDocument<32> doc;
  doc["FW"] = firmwareVersion;
  doc["RC"] = restartCounter;
  doc["DU"] = openDuration;

  char jsonBuffer[32];
  serializeJson(doc, jsonBuffer);
  sendData(jsonBuffer);
}

void sendData(const char *text)
{
  static unsigned long previousMillis = 0;
  const long interval = 100; // Minimum interval between transmissions

  if (strlen(text) > 31) // Allow up to 31 characters + null terminator
  {
    Serial.println("Payload Size Limit Exceeded! [" + String(strlen(text)) + "] Bytes");
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    if (radio.write(text, strlen(text) + 1)) // +1 to include null terminator
    {
      nodeStateLed = true;
      Serial.println("Sent: [" + String(strlen(text) + 1) + "] Bytes -> " + String(text));
    }
    else
    {
      nodeStateLed = false;
      Serial.println("Send Failed! Reconnecting...");
      // softwareReset();
    }
  }
}

void NRF24_Init()
{
  if (!radio.begin())
  {
    Serial.println("NRF24L01 Initialization Failed!");
    return;
  }
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();
  Serial.println("NRF24L01 Initialized Successfully!");
}
