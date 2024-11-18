#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "driver/ledc.h"

// Replace with your WiFi credentials
const char*  ssid = "Home";
const char*  password = "Suni@1704";

// Replace with your MQTT broker details
const char* mqttServer = "192.168.1.14";
const int mqttPort = 1883;


// Replace with your subtopics
const char* mqttSubtopics[] = {"Morning", "Afternoon", "Evening"};

// Pins for LEDs
const int ledPins[] = {4,5,14};  // Replace with your desired pin numbers
const int buzzerPin = 15;
const int switchPin = 12;



unsigned long startmillis = 0;  // Renamed from timerStart to avoid conflict
const int timerDuration = 60000;  // 1 minute in milliseconds
const int repeatDuration = 60000; // 2 min 

// Initialize the LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Create an instance of the MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

bool timerExpired = false;
unsigned long timerStartTime = 0;



void setup() {
  for (int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
  }

  pinMode(buzzerPin, OUTPUT);
 


  // Start the serial communication
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Connect to MQTT broker
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setKeepAlive(60); // Set keep-alive interval to 60 seconds
  while (!client.connected()) {
    if (client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT broker");
      for (int i = 0; i < 3; i++) {
        client.subscribe(mqttSubtopics[i]);
      }
       client.loop();
    } 
    else {
      Serial.println("Failed to connect to MQTT broker. Retrying...");
      delay(2000);
    }

  }
   lcd.init();
  lcd.backlight();
  lcd.clear();
}


int subtopicIndex = -1;
char lastTopic[32];


void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to a string
  payload[length] = '\0';
  String message = String((char*)payload);

   strcpy(lastTopic, topic);

  // Find the index of the subtopic
  int subtopicIndex = -1;
  for (int i = 0; i < 3; i++) {
    if (strcmp(topic, mqttSubtopics[i]) == 0) {
      subtopicIndex = i;
      break;
    }
  }

  if (subtopicIndex != -1) {
    // Parse the message to extract time and medicine name
    int separatorIndex = message.indexOf(',');
    String time = message.substring(0, separatorIndex);
    String medicineName = message.substring(separatorIndex + 1);

    // Display the medicine name on the LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Medicine " + String(subtopicIndex + 1) + ": " + medicineName);
    Serial.println("Medicine " + String(subtopicIndex + 1) + ": " + medicineName);

    // Turn on the LED for the corresponding subtopic
    digitalWrite(ledPins[subtopicIndex], HIGH);
    tone(buzzerPin,1000);
    startmillis = millis();  // Record the start time of the 1-minute timer
    
    while ((millis() - startmillis) < timerDuration) {
      // Check if the switch is open and closed within the 1-minute timer
      // If yes, consider medicine not taken
      if (digitalRead(switchPin) == LOW) {
        delay(100);  // Debounce
        if (digitalRead(switchPin) == HIGH) {
          // Medicine not taken, handle accordingly
          Serial.println("Medicine taken within 1 minute");
          break;
        }
      }
    }

    // After 1 minute, turn off the LED and buzzer
    digitalWrite(ledPins[subtopicIndex], LOW);
    tone(buzzerPin,0);
     timerExpired = true;
    timerStartTime = millis();
  }
}

void loop() {
  if(!client.connected()){
    reconnect();
  }
  client.loop();
  if (timerExpired) {
     unsigned long currentTime = millis();
     unsigned long elapsedTime = millis() - timerStartTime;
      Serial.println("Time threshold reached");
      Serial.print("Elapsed Time: ");
      Serial.println(elapsedTime);
      Serial.print("Repeat Duration: ");
      Serial.println(repeatDuration);

      for (int i = 0; i < 3; i++) {
      if (strcmp(lastTopic, mqttSubtopics[i]) == 0) {
      subtopicIndex = i;
      break;
     }
     }

     if ( elapsedTime >= repeatDuration) {
      Serial.println("time passed 15");
      // If 15 minutes have passed, reset the system
     
      if (subtopicIndex != -1) {
        
        digitalWrite(ledPins[subtopicIndex], HIGH);
        tone(buzzerPin, 1000);
        delay(30000);
        tone(buzzerPin, 0);
        digitalWrite(ledPins[subtopicIndex], LOW);
       
      }
      
       resetSystem();
    } else {
      // Check if the switch is pressed during the 15-minute period
      if (digitalRead(switchPin) == LOW) {
        delay(100);  // Debounce
        if (digitalRead(switchPin) == HIGH) {
          // Medicine taken
          Serial.println("Medicine taken within 15 minutes");
          client.publish("alert", "Medicine taken within 15 minutes");
          // Clear the flag and reset the system
          resetSystem();
        }
      }
    }
  }
}

  void resetSystem() {
  // Turn off LEDs and buzzer, clear LCD, reset flags
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  tone(buzzerPin, 0);
  lcd.clear();
  timerExpired = false;
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT broker");
      for (int i = 0; i < 3; i++) {
        client.subscribe(mqttSubtopics[i]);
      }
    } else {
      Serial.println("Failed to connect to MQTT broker. Retrying...");
      delay(2000);
    }
  }
}

