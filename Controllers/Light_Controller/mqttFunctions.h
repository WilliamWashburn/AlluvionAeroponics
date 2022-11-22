#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include "credentials.h"

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/Lights/#";

char mqttMessage[50];

void subscriptToTopics() {
  Serial.print("Subscribing to topic: ");
  Serial.println(topic);
  Serial.println();
  mqttClient.subscribe(topic);  // subscribe to a topic
}

bool connectToBroker() {
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);
  mqttClient.setUsernamePassword(mqttUser, mqttPass);
  mqttClient.onMessage(onMqttMessage);  // set the message receive callback
  long startBroker = millis();
  while (!mqttClient.connect(broker, port) && millis() - startBroker < 10000) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
  }
  if (mqttClient.connected()) {
    Serial.println("You're connected to the MQTT broker!");
    Serial.println();
    subscriptToTopics();
    return true;
  }
  return false;
  //if its still not connected, we'll let this be handled by checkBrokerConnection()
}

void readMessage() {
  int inx = 0;
  while (mqttClient.available()) {
    mqttMessage[inx] = (char)mqttClient.read();
    inx++;
    // Serial.print((char)mqttClient.read());
  }
  mqttMessage[inx] = '\0';  //null terminate
}

bool checkBrokerConnection() {
  if (!mqttClient.connected()) {
    if (!mqttClient.connect(broker, port)) {
      Serial.print("MQTT connection failed! Error code = ");
      Serial.println(mqttClient.connectError());
      return false;
    }
    if (mqttClient.connected()) {
      Serial.println("You've reconnnected to the broker!");
      subscriptToTopics();
    }
  }
  return true;
}