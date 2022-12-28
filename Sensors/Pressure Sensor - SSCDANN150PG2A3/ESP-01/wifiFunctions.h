#include <ESP8266WiFi.h> //for wifi
#include <PubSubClient.h> //for mqtt
#include <ArduinoJson.h> //for json

const char* mqtt_server = "homeassistant.local";

const char* mqtt_user = "pressureSensor";

char willTopic[50];                //for the MQTT willTopic
char sensorReadingTopic[50];       //where the sensor readings get published
char pressureConfigTopic[100];     //for MQTT discovery in home assistant
char temperatureConfigTopic[100];  //for MQTT discovery in home assistant
char statusConfigTopic[100];       //for MQTT discovery in home assistant
char pressureStateTopic[100];      //for MQTT discovery in home assistant
char temperatureStateTopic[100];   //for MQTT discovery in home assistant
char statusStateTopic[100];        //for MQTT discovery in home assistant

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup_mqtt() {
  //build willTopic
  strcpy(willTopic, "sensors/");
  strcat(willTopic, whichSensor);
  strcat(willTopic, "/status");

  //build sensor reading topic
  strcpy(sensorReadingTopic, "sensors/");
  strcat(sensorReadingTopic, whichSensor);
  strcat(sensorReadingTopic, "/readings");

  //pressure configuration
  strcpy(pressureConfigTopic, "homeassistant/sensor/");
  strcat(pressureConfigTopic, whichSensor);
  strcat(pressureConfigTopic, "/pressure/config");

  //temperature configuration
  strcpy(temperatureConfigTopic, "homeassistant/sensor/");
  strcat(temperatureConfigTopic, whichSensor);
  strcat(temperatureConfigTopic, "/temperature/config");

  //status configuration
  strcpy(statusConfigTopic, "homeassistant/binary_sensor/");
  strcat(statusConfigTopic, whichSensor);
  strcat(statusConfigTopic, "/status/config");

  //pressure state topic
  strcpy(pressureStateTopic, "sensors/");
  strcat(pressureStateTopic, whichSensor);
  strcat(pressureStateTopic, "/readings");

  //temperature state topic
  strcpy(temperatureStateTopic, "sensors/");
  strcat(temperatureStateTopic, whichSensor);
  strcat(temperatureStateTopic, "/readings");

  //status state topic
  strcpy(statusStateTopic, "sensors/");
  strcat(statusStateTopic, whichSensor);
  strcat(statusStateTopic, "/status");
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    static String clientId = "ESP8266Client-" + String(random(0xffff), HEX);

    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass, willTopic, 0, true, "OFF")) {
      Serial.println("connected");
      // Once connected, publish an announcement...

      client.publish(willTopic, "ON");

      //configure mqtt discovery for homeassistant
      DynamicJsonDocument MQTTDiscoveryMessage(2048);

      //for pressure
      MQTTDiscoveryMessage["name"] = String(whichSensor) + "_pressure";     //The name of the MQTT sensor. This is what will be used in home assistant!
      MQTTDiscoveryMessage["state_topic"] = pressureStateTopic;             //The MQTT topic subscribed to receive sensor values.
      MQTTDiscoveryMessage["uniq_id"] = String(whichSensor) + "_pressure";  //An ID that uniquely identifies this sensor. If two sensors have the same unique ID, Home Assistant will raise an exception.
      MQTTDiscoveryMessage["dev_cla"] = "pressure";                         //The type/class of the sensor to set the icon in the frontend.
      MQTTDiscoveryMessage["unit_of_measurement"] = "psi";                  //Defines the units of measurement of the sensor, if any.
      MQTTDiscoveryMessage["val_tpl"] = "{{ value_json.pressure }}";        //Defines a template to extract the value. If the template throws an error, the current state will be used instead.
      MQTTDiscoveryMessage["state_class"] = "measurement";                  //The state_class of the sensor.
      MQTTDiscoveryMessage["device"]["identifiers"][0] = whichSensor;       //Information about the device this sensor is a part of to tie it into the device registry. Only works through MQTT discovery and when unique_id is set. At least one of identifiers or connections must be present to identify the device.
      MQTTDiscoveryMessage["device"]["name"] = whichSensor;                 //The name of the device.
      MQTTDiscoveryMessage["device"]["model"] = whichSensor;                //The model of the device.
      MQTTDiscoveryMessage["device"]["manufacturer"] = "William";           //The manufacturer of the device.

      client.beginPublish(pressureConfigTopic, measureJson(MQTTDiscoveryMessage), true);
      serializeJson(MQTTDiscoveryMessage, client);
      client.endPublish();

      //for temperature
      MQTTDiscoveryMessage.clear();  //wipes the memory
      MQTTDiscoveryMessage["name"] = String(whichSensor) + "_temperature";
      MQTTDiscoveryMessage["state_topic"] = temperatureStateTopic;
      MQTTDiscoveryMessage["uniq_id"] = String(whichSensor) + "_temperature";
      MQTTDiscoveryMessage["dev_cla"] = "temperature";
      MQTTDiscoveryMessage["unit_of_measurement"] = "°F";
      MQTTDiscoveryMessage["val_tpl"] = "{{ value_json.temperature }}";
      MQTTDiscoveryMessage["state_class"] = "measurement";
      MQTTDiscoveryMessage["device"]["identifiers"][0] = whichSensor;
      MQTTDiscoveryMessage["device"]["name"] = whichSensor;
      MQTTDiscoveryMessage["device"]["model"] = whichSensor;
      MQTTDiscoveryMessage["device"]["manufacturer"] = "William";

      client.beginPublish(temperatureConfigTopic, measureJson(MQTTDiscoveryMessage), true);
      serializeJson(MQTTDiscoveryMessage, client);
      client.endPublish();

      //connection status
      MQTTDiscoveryMessage.clear();  //wipes the memory
      MQTTDiscoveryMessage["name"] = String(whichSensor) + "_status";
      MQTTDiscoveryMessage["state_topic"] = statusStateTopic;
      MQTTDiscoveryMessage["uniq_id"] = String(whichSensor) + "_status";
      MQTTDiscoveryMessage["dev_cla"] = "connectivity";
      MQTTDiscoveryMessage["device"]["identifiers"][0] = whichSensor;
      MQTTDiscoveryMessage["device"]["name"] = whichSensor;
      MQTTDiscoveryMessage["device"]["model"] = whichSensor;
      MQTTDiscoveryMessage["device"]["manufacturer"] = "William";

      client.beginPublish(statusConfigTopic, measureJson(MQTTDiscoveryMessage), true);
      serializeJson(MQTTDiscoveryMessage, client);
      client.endPublish();

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}