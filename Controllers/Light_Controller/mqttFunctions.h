#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include "credentials.h"

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/Lights/#";
const char willTopic[] = "Desoto/Lights/willTopic";
String willPayload = "Disconnected :(";

char mqttMessage[50];

void subscriptToTopics() {
  Serial.print("Subscribing to topic: ");
  Serial.println(topic);
  Serial.println();
  mqttClient.subscribe(topic);  // subscribe to a topic
}

void updateWillTopic() {
  // update willTopic so we know that it is connected
  String onConnectionPayload = "Connected!";
  mqttClient.beginMessage(willTopic, onConnectionPayload.length(), true, 1);
  mqttClient.print(onConnectionPayload);
  mqttClient.endMessage();
}

void configureBrokerClient() {
  mqttClient.setUsernamePassword(mqttUser, mqttPass);
  // set will message
  mqttClient.beginWill(willTopic, willPayload.length(), true, 1);
  mqttClient.print(willPayload);
  mqttClient.endWill();
}

bool connectToBroker() {
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  long startTime = millis();
  while (!mqttClient.connect(broker, port) && millis() - startTime < 10000) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
  }
  if (mqttClient.connected()) {
    Serial.println("You've reconnected!");
    updateWillTopic();
    subscriptToTopics();
    return true;
  }
  return false;
}

void readMessage() {
  int inx = 0;
  while (mqttClient.available()) {
    mqttMessage[inx] = (char)mqttClient.read();
    inx++;
    // Serial.print((char)mqttClient.read());
  }
  mqttMessage[inx] = '\0';  // null terminate
}