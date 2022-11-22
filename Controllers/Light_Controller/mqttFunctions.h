#include <WiFi.h>
#include <ArduinoMqttClient.h>

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/Lights/#";

char mqttMessage[50];

void connectToBroker() {
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);
  mqttClient.setUsernamePassword(mqttUser, mqttPass);
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1)
      ;
  }
  Serial.println("You're connected to the MQTT broker!");
  Serial.println();
}

void subscriptToTopics() {
  Serial.print("Subscribing to topic: "); Serial.println(topic); Serial.println();
  // subscribe to a topic
  mqttClient.subscribe(topic);
void readMessage() {
  int inx = 0;
  while (mqttClient.available()) {
    mqttMessage[inx] = (char)mqttClient.read();
    inx++;
    // Serial.print((char)mqttClient.read());
  }
  mqttMessage[inx] = '\0';  //null terminate
}