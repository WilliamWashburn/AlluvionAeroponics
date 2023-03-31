#ifndef MQTT_H_INCLUDED
#define MQTT_H_INCLUDED

#include <ArduinoMqttClient.h>
#include "credentials.h"  //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below

extern int feedbackPin;

void connectToBroker(MqttClient* mqttClient);
void updateWillTopic(MqttClient* mqttClient);
void printUpdateToHomeAssistant(MqttClient* mqttClient, char* mqttMessageForBroker, bool printToSerial = true);
void printUpdateToHomeAssistant(MqttClient* mqttClient, String mqttMessageForBroker, bool printToSerial = true);
char* readMessage(MqttClient*);
bool updateIfSpraying(MqttClient* mqttClient, int solenoidSpraying);
void beginMessageForSolenoidFeedback(MqttClient* mqttClient, int solenoidSpraying);

#endif