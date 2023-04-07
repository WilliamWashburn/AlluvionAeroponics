#include <Arduino.h>
#include <ArduinoMqttClient.h>

#include "mqttFunctions.h"

const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/Aero3/#";                      //for reading from
const char sprayTopic1[] = "Desoto/Aero3/solenoid1/state";  //for updating when sprayed
const char sprayTopic2[] = "Desoto/Aero3/solenoid2/state";
const char sprayTopic3[] = "Desoto/Aero3/solenoid3/state";

const char willTopic[] = "Desoto/Aero3/willTopic"; //willTopic

int feedbackPin = 32;

void connectToBroker(MqttClient* mqttClient){
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);
  mqttClient->setUsernamePassword(mqttUser, mqttPass);
  
  // set will message
  String willPayload = "Disconnected :("; //will Payload
  mqttClient->beginWill(willTopic, willPayload.length(), true, 1);
  mqttClient->print(willPayload);
  mqttClient->endWill();

  if (!mqttClient->connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient->connectError());
  }
  else {
    Serial.println("You're connected to the MQTT broker!");
    Serial.println();
    Serial.print("Subscribing to topic: ");
    Serial.println(topic);
    Serial.println();

    //subscribe to topic
    mqttClient->subscribe(topic);
  }
}
void updateWillTopic(MqttClient* mqttClient) {
  // update willTopic so we know that it is connected
  String onConnectionPayload = "Connected!";
  mqttClient->beginMessage(willTopic, onConnectionPayload.length(), true, 1);
  mqttClient->print(onConnectionPayload);
  mqttClient->endMessage();
}

void printUpdateToHomeAssistant(MqttClient* mqttClient, char* mqttMessageForBroker, bool printToSerial){
  mqttClient->beginMessage("Desoto/Aero3/statusUpdate");
  mqttClient->print(mqttMessageForBroker);
  mqttClient->endMessage();

  if (printToSerial) {
    Serial.println(mqttMessageForBroker);
  }
}

void printUpdateToHomeAssistant(MqttClient* mqttClient, String mqttMessageForBroker, bool printToSerial) {
  mqttClient->beginMessage("Desoto/Aero3/statusUpdate");
  mqttClient->print(mqttMessageForBroker);
  mqttClient->endMessage();

  if (printToSerial) {
    Serial.println(mqttMessageForBroker);
  }
}

char mqttMessage[50]; //stores mqtt message
char* readMessage(MqttClient* mqttClient) {
  int inx = 0;
  while (mqttClient->available()) {
    mqttMessage[inx] = (char)mqttClient->read();
    inx++;
    // Serial.print((char)mqttClient.read());
  }
  mqttMessage[inx] = '\0';  // null terminate
  return mqttMessage;
}

void beginMessageForSolenoidFeedback(MqttClient* mqttClient, int solenoidSpraying) {
  if (solenoidSpraying == 1) {
    mqttClient->beginMessage(sprayTopic1);
  } else if (solenoidSpraying == 2) {
    mqttClient->beginMessage(sprayTopic2);
  } else if (solenoidSpraying == 3) {
    mqttClient->beginMessage(sprayTopic3);
  }
}

bool updateIfSpraying(MqttClient* mqttClient, int solenoidSpraying) {
  if (digitalRead(feedbackPin)) {  //if spraying
    beginMessageForSolenoidFeedback(mqttClient, solenoidSpraying);
    mqttClient->print("Spraying");
    mqttClient->endMessage();
    Serial.println("mqtt message sent");
    return true;
  }
  return false;
}