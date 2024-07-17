#include <WiFi.h>
#include <ThingsBoard.h>
#include <Arduino_MQTT_Client.h>

//Setting the pins for the ultrasonic sensors
int triggerPins[] = {2,5};
int echoPins[] = {3,6};
int ledPin = 10; //Setting the pin for the led

//Setting the variables for handling sensors
int triggerPin;
int echoPin;
int filled;
int numSensors = sizeof(triggerPins) / sizeof(triggerPins[0]);
long timeReading, distance;

//Setting the variables for timings
long ledSetTime;
long sleepTime = 5000;
long lastReading;
long reservedDuration = 30000;

bool ledState = false; //LED state

//Variables for WiFi and ThingsBoard
constexpr char WIFI_SSID[] = "";
constexpr char WIFI_PASSWORD[] = "";

constexpr char TOKEN[] = "";
constexpr char THINGSBOARD_SERVER[] = "";
constexpr uint16_t THINGSBOARD_PORT = 45883U;
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

constexpr uint32_t SERIAL_DEBUG_BAUD = 9600U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

//Callback function for setting the reserved state
RPC_Response rpcSetReservedCallback(const RPC_Data &data) {
  Serial.println("Received the setReserved RPC command");

  ledState = data;
  ledSetTime = millis();

  digitalWrite(ledPin, ledState ? HIGH : LOW);

  Serial.print("LED is now ");
  Serial.println(ledState ? "ON" : "OFF");

  if (!tb.sendAttributeData("reserved", ledState)) {
    Serial.println("Error sending reserved");
  } else {
    Serial.println("reserved sent successfully");
  }

  return RPC_Response("reserved", (int)ledState);
}

const std::array<RPC_Callback, 1U> callbacks = {
  RPC_Callback{ "setReserved", rpcSetReservedCallback}
};

//Function for connecting to WiFi
void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected.");
}
//Function for reconnecting to WiFi
bool reconnect() {
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED){
    return true;
  }
  connectToWiFi();
  return true;
}

//Function for connecting to ThingsBoard
void connectToThingsBoard() {
  Serial.print("Connecting to ThingsBoard...");
  while (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" connected.");
}

void setup(){
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);

  pinMode(ledPin, OUTPUT); //Setting the led pin as output

  for(int i = 0; i < numSensors; i = i + 1){ //Setting the trigger and echo pins as output and input
    triggerPin = triggerPins[i];
    echoPin = echoPins[i];
    
    pinMode(triggerPin, OUTPUT);
    pinMode(echoPin, INPUT);
  }

  connectToWiFi();

  connectToThingsBoard();

  if (!tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend())) {
    Serial.println("Failed to subscribe for RPC");
    return;
  }

  if (!tb.sendAttributeData("totalSeats", numSensors)) { //Sending the total number of seats
    Serial.println("Error sending seat number");
  } else {
    Serial.println("Seat number sent successfully");
  }

  if (!tb.sendAttributeData("filledSeats", 0)) { //Sending the total number of seats
    Serial.println("Error setting default for filledSeats");
  } else {
    Serial.println("Set filledSeats successfully");
  }

  if (!tb.sendAttributeData("reserved", false)) { //Sending the total number of seats
    Serial.println("Error setting default for reserved");
  } else {
    Serial.println("Reserved set successfully");
  }
}

void loop(){
  if (!tb.connected()) {
    connectToThingsBoard();
  }
  reconnect();

  if (millis() - lastReading > sleepTime){ //Reading the sensors, if enough time has passed
    filled = 0;
    for(int i = 0; i < numSensors; i = i + 1){
      triggerPin = triggerPins[i];
      echoPin = echoPins[i];

      digitalWrite(triggerPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(triggerPin, LOW);

      timeReading = pulseIn(echoPin, HIGH);
      distance = (timeReading / 2) / 28;
      Serial.print(distance);
      Serial.println(" cm");
      if (distance < 30){
        filled = filled + 1;
      }
    }
    Serial.println(filled);
    lastReading = millis();
    if (ledState && filled > 0){ //Clearing the reserved state if a seat is filled
      ledState = false;
      digitalWrite(ledPin, LOW);
      if (!tb.sendAttributeData("reserved", ledState)) {
        Serial.println("Error sending reserved");
      } else {
        Serial.println("reserved sent successfully");
      }
    }
    if (!tb.sendAttributeData("filledSeats", filled)) { //Sending the number of filled seats
      Serial.println("Failed to send filled seats");
    } else {
      Serial.println("Sent seats successfully");
    }
  }

  if (ledState && (millis() - ledSetTime) > reservedDuration) { //Clearing the reserved state after a certain time
    ledState = false;
    digitalWrite(ledPin, LOW);
    if (!tb.sendAttributeData("reserved", ledState)) {
      Serial.println("Error sending reserved");
    } else {
      Serial.println("reserved sent successfully");
    }
  }
  tb.loop();
}
