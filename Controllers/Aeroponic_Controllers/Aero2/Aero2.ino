#include <credentials.h> //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below
#include <TimeLib.h>
#include <TimeAlarms.h>

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoMqttClient.h>

int solenoidPins[] = { 15, 33, 27 };
int nbrOfPins = 3;

const char* ssid = mySSID;
const char* password = myPASSWORD;

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "pool.ntp.org", -14400, 60000);  //we are -5 UTC for EST and -4 UTC for EDT. This is -18000 and -14400 seconds (instead of hours)

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/Aero2/#";

//for turning off individual zones
bool cycleSolenoid1 = true;
bool cycleSolenoid2 = true;
bool cycleSolenoid3 = true;

int wateringDurations[] = { 7, 7, 7 };

AlarmId wateringAlarmID;
long wateringDelay = 60;  //default

void setup() {

  Serial.begin(115200);

  for (int i = 0; i < nbrOfPins; i++) {
    pinMode(solenoidPins[i], OUTPUT);
    digitalWrite(solenoidPins[i], LOW);
  }

  //CONNECT TO WIFI
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int startTime = millis();
  while (millis() - startTime < 60000 && WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to wifi");
  }

  //CONNECT TO NTP
  timeClient.begin();
  timeClient.update();
  Serial.println(timeClient.getFormattedTime());
  int currentHour = timeClient.getHours();
  int currentMin = timeClient.getMinutes();
  int currentSec = timeClient.getSeconds();
  setTime(currentHour, currentMin, currentSec, 10, 7, 22);  // set time to Saturday 8:29:00am Jan 1 2011

  //CONNECT TO MQTT BROKER
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
  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);
  Serial.print("Subscribing to topic: ");
  Serial.println(topic);
  Serial.println();
  // subscribe to a topic
  mqttClient.subscribe(topic);

  //SET ALARMS
  wateringAlarmID = Alarm.timerRepeat(wateringDelay, cycleThroughSolenoids);
  // cycleThroughSolenoids();
  // Serial.println("Value: " + String(Alarm.read(wateringAlarmID)));
}

void loop() {
  checkWifiConnection();
  Alarm.delay(0);  //needed to service alarms

  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();
}

void cycleThroughSolenoids() {
  //Send update to MQTT
  mqttClient.beginMessage("Desoto/Aero2/watering/lastTriggered");
  mqttClient.print("hello!");
  mqttClient.endMessage();

  for (int i = 0; i < nbrOfPins; i++) {
    if (i == 0 && cycleSolenoid1 == false) {
      Serial.println("Dont cycle solenoid 1");
    } else if (i == 1 && cycleSolenoid2 == false) {
      Serial.println("Dont cycle solenoid 2");
    } else if (i == 2 && cycleSolenoid3 == false) {
      Serial.println("Dont cycle solenoid 3");
    } else {
      digitalWrite(solenoidPins[i], HIGH);
      Alarm.delay(wateringDurations[i] * 1000);  //need to fix: prevents commands
      digitalWrite(solenoidPins[i], LOW);
      Alarm.delay(500);
    }
  }
}

//TURN ON SOLENOID
void turnOnSolenoid1() {
  digitalWrite(solenoidPins[0], HIGH);
}
void turnOnSolenoid2() {
  digitalWrite(solenoidPins[1], HIGH);
}
void turnOnSolenoid3() {
  digitalWrite(solenoidPins[2], HIGH);
}


//TURN OFF SOLENOID
void turnOffSolenoid1() {
  digitalWrite(solenoidPins[0], LOW);
}
void turnOffSolenoid2() {
  digitalWrite(solenoidPins[1], LOW);
}
void turnOffSolenoid3() {
  digitalWrite(solenoidPins[2], LOW);
}

//MQTT
void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  if (topic == "Desoto/Aero2/solenoid1/command") {
    Serial.println("Should toggle solenoid1");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      turnOnSolenoid1();
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      turnOffSolenoid1();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero2/solenoid2/command") {
    Serial.println("Should toggle solenoid2");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      turnOnSolenoid2();
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      turnOffSolenoid2();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero2/solenoid3/command") {
    Serial.println("Should toggle solenoid 3");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      turnOnSolenoid3();
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      turnOffSolenoid3();
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero2/watering/solenoid1/command") {
    Serial.println("Should toggle solenoid 1 water cycle");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      Serial.println("solenoid 1 cycle is on!");
      cycleSolenoid1 = true;
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      Serial.println("solenoid 1 cycle is off!");
      cycleSolenoid1 = false;
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero2/watering/solenoid2/command") {
    Serial.println("Should toggle solenoid 2 water cycle");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      Serial.println("solenoid 2 cycle is on!");
      cycleSolenoid2 = true;
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      Serial.println("solenoid 2 cycle is off!");
      cycleSolenoid2 = false;
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero2/watering/solenoid3/command") {
    Serial.println("Should toggle solenoid 3 water cycle");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);
    if (strcmp(message, "{\"state\":\"on\"}") == 0) {
      Serial.println("solenoid 3 cycle is on!");
      cycleSolenoid3 = true;
    } else if (strcmp(message, "{\"state\":\"off\"}") == 0) {
      Serial.println("solenoid 3 cycle is off!");
      cycleSolenoid3 = false;
    } else {
      Serial.print("message not recognized for topic: ");
      Serial.print(topic);
      Serial.print(" and message: ");
      Serial.println(message);
    }
  } else if (topic == "Desoto/Aero2/watering/solenoid1/duration") {
    Serial.println("Should update solenoid 1 water duration");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(atoi(message));

    wateringDurations[0] = atoi(message);

  } else if (topic == "Desoto/Aero2/watering/solenoid2/duration") {
    Serial.println("Should update solenoid 2 water duration");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);

    wateringDurations[1] = atoi(message);
  } else if (topic == "Desoto/Aero2/watering/solenoid3/duration") {
    Serial.println("Should update solenoid 3 water duration");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);

    wateringDurations[2] = atoi(message);
  } else if (topic == "Desoto/Aero2/watering/delay") {
    Serial.println("Should update the watering delay");
    char message[50];
    int inx = 0;
    while (mqttClient.available()) {
      message[inx] = (char)mqttClient.read();
      inx++;
      // Serial.print((char)mqttClient.read());
    }
    message[inx] = '\0';  //null terminate
    Serial.println(message);

    wateringDelay = atoi(message);
    Serial.println("Updating delay to :" + String(wateringDelay));
    Alarm.write(wateringAlarmID, wateringDelay);
  } else {
    Serial.print("Update not recognized: ");
    Serial.println(topic);
  }
}

//WIFI
void checkWifiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Disconnected from wifi!");

    WiFi.begin(ssid, password);

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
}