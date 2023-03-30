#include "credentials.h"  //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below
#include "ntpFunctions.h"

#include <TimeLib.h>
#include <TimeAlarms.h>

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoMqttClient.h>

int solenoidPins[] = { 15, 33, 27 };
int feedbackPin = 32;
int nbrOfSolenoids = 3;

const char* ssid = mySSID;
const char* password = myPASSWORD;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "homeassistant.local";
int port = 1883;
const char topic[] = "Desoto/Aero3/#";                      //for reading from
const char sprayTopic1[] = "Desoto/Aero3/solenoid1/state";  //for updating when sprayed
const char sprayTopic2[] = "Desoto/Aero3/solenoid2/state";
const char sprayTopic3[] = "Desoto/Aero3/solenoid3/state";

//for turning off individual zones
bool cycleSolenoid1 = true;
bool cycleSolenoid2 = true;
bool cycleSolenoid3 = true;

int wateringDurations[] = { 7, 7, 7 };

AlarmId wateringAlarmID;
long wateringDelay = 60;  //default

void setup() {

  Serial.begin(115200);

  for (int i = 0; i < nbrOfSolenoids; i++) {
    pinMode(solenoidPins[i], OUTPUT);
    pinMode(feedbackPin, INPUT);
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
  configTime(0, 0, NTP_SERVER);  // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", TZ_INFO, 1);
  getNTPtime(10);
  setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon + 1, 1900 + timeinfo.tm_year);  // tm_mon is 0-11 so add 1, tm_year is years since 1900 so add to 1900
  Serial.println("Time: " + String(hour()) + ":" + String(minute()) + ":" + String(second()));
  Serial.println("Date: " + String(month()) + "/" + String(day()) + "/" + String(year()));

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

void checkWiFiConnection() {
  static int connectCount = 0;
  if (!WiFi.isConnected()) {
    WiFi.reconnect();
    connectCount++;
    Serial.println("Connection lost. Reconnecting");
    //if we had to reconnect more than 10 times and we have been alive for more than 10 minutes (so that we arent constantly restarting)
    if (connectCount > 10 && millis() > 10*60*1000) {
      Serial.println("Reconnected too many times. Restarting");
      delay(2000);
      ESP.restart(); //if we have had to reconnect a bunch, just restart the controller
    }
  }
}

void loop() {
  Alarm.delay(0);  //needed to service alarms

  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();

  //even though the wifi class should auto reconnect if disconnected, im adding this here to make sure
  checkWiFiConnection();
}

void printUpdateToHomeAssistant(char* mqttMessageForBroker, bool printToSerial = true){
  mqttClient.beginMessage("Desoto/Aero3/statusUpdate");
  mqttClient.print(mqttMessageForBroker);
  mqttClient.endMessage();

  if (printToSerial) {
    Serial.println(mqttMessageForBroker);
  }
}

void printUpdateToHomeAssistant(String mqttMessageForBroker, bool printToSerial = true) {
  mqttClient.beginMessage("Desoto/Aero3/statusUpdate");
  mqttClient.print(mqttMessageForBroker);
  mqttClient.endMessage();

  if (printToSerial) {
    Serial.println(mqttMessageForBroker);
  }
}

void cycleThroughSolenoids() {
  //Send update to MQTT
  mqttClient.beginMessage("Desoto/Aero3/watering/lastTriggered");
  mqttClient.print("hello!");
  mqttClient.endMessage();

  static int whichSolenoidToStart = 0;
  int whichSolenoid; //initialize

  for (int count = 0; count < nbrOfSolenoids; count++) { 
    whichSolenoid = (whichSolenoidToStart + count) % nbrOfSolenoids;
    if (whichSolenoid == 0 && cycleSolenoid1 == false) {
      Serial.println("Dont cycle solenoid 1");
    } else if (whichSolenoid == 1 && cycleSolenoid2 == false) {
      Serial.println("Dont cycle solenoid 2");
    } else if (whichSolenoid == 2 && cycleSolenoid3 == false) {
      Serial.println("Dont cycle solenoid 3");
    } else {
      digitalWrite(solenoidPins[whichSolenoid], HIGH);
      delayWhileSpraying(wateringDurations[whichSolenoid] * 1000, whichSolenoid + 1);
      digitalWrite(solenoidPins[whichSolenoid], LOW);
      customDelay(1000);
    }
  }
  whichSolenoidToStart++; //increment
  whichSolenoidToStart%=nbrOfSolenoids; //wrap around if greater than nbrOfSolenoids
}

void delayWhileSpraying(long timeToDelayMilli, int solenoidSpraying) {
  long startTime = millis();
  static bool updateMessageSent = false;  //flag so that once it starts spraying, it only sends 1 mqtt message
  while (millis() - startTime < timeToDelayMilli) {
    //all of these functions need to be quick and none blocking.
    Alarm.delay(0);     //service alarms
    mqttClient.poll();  //check for mqtt messages

    if (!updateMessageSent) {
      if (updateIfSpraying(solenoidSpraying)) {
        updateMessageSent = true;
        Serial.println("updateMessageSent set true");
      }
    }
  }
  if (!updateMessageSent) {
    Serial.println("No spraying detected!");
    beginMessageForSolenoidFeedback(solenoidSpraying);
    mqttClient.print("No spraying detected!"); //if supposed to spray but doesn't
    mqttClient.endMessage();
  } else {
    beginMessageForSolenoidFeedback(solenoidSpraying);
    mqttClient.print("Not spraying"); //when the spraying stops
    mqttClient.endMessage();
    updateMessageSent = false;  //reset flag after delay
    Serial.println("updateMessageSent reset to false");
    Serial.println();
  }
}

void customDelay(long timeToDelayMilli) {
  long startTime = millis();
  while (millis() - startTime < timeToDelayMilli) {
    Alarm.delay(0);     //service alarms
    mqttClient.poll();  //check for mqtt messages
  }
}

void beginMessageForSolenoidFeedback(int solenoidSpraying) {
  if (solenoidSpraying == 1) {
    mqttClient.beginMessage(sprayTopic1);
  } else if (solenoidSpraying == 2) {
    mqttClient.beginMessage(sprayTopic2);
  } else if (solenoidSpraying == 3) {
    mqttClient.beginMessage(sprayTopic3);
  }
}

bool updateIfSpraying(int solenoidSpraying) {
  if (digitalRead(feedbackPin)) {  //if spraying
    beginMessageForSolenoidFeedback(solenoidSpraying);
    mqttClient.print("Spraying");
    mqttClient.endMessage();
    Serial.println("mqtt message sent");
    return true;
  }
  return false;
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
char mqttMessage[50]; //stores mqtt message
void readMessage() {
  int inx = 0;
  while (mqttClient.available()) {
    mqttMessage[inx] = (char)mqttClient.read();
    inx++;
    // Serial.print((char)mqttClient.read());
  }
  mqttMessage[inx] = '\0';  // null terminate
}

String mqttTopic; //the mqtt topic that the controller is receiving
bool parsingError = false;  //flag for parsing error. Either topic or message not recognized

void onMqttMessage(int messageSize) {
  long startResponse = millis(); //start timer. For seeing how long the response took
  mqttTopic = mqttClient.messageTopic();
  Serial.println();
  Serial.println("MQTT mqttTopic: " + String(mqttTopic));

  //SOLENOID COMMANDS
  if (mqttTopic == "Desoto/Aero3/solenoid1/command") {
    readMessage();
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      printUpdateToHomeAssistant("Should turn on solenoid 1");
      turnOnSolenoid1();
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      printUpdateToHomeAssistant("Should turn off solenoid 1");
      turnOffSolenoid1();
    } else parsingError = true;
  } else if (mqttTopic == "Desoto/Aero3/solenoid2/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      printUpdateToHomeAssistant("Should turn on solenoid 2");
      turnOnSolenoid2();
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      printUpdateToHomeAssistant("Should turn off solenoid 2");
      turnOffSolenoid2();
    } else parsingError = true;
  } else if (mqttTopic == "Desoto/Aero3/solenoid3/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      printUpdateToHomeAssistant("Should turn on solenoid 3");
      turnOnSolenoid3();
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      printUpdateToHomeAssistant("Should turn off solenoid 3");
      turnOffSolenoid3();
    } else parsingError = true;
  }
  
  //WATERING CYCLES
  else if (mqttTopic == "Desoto/Aero3/watering/solenoid1/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      printUpdateToHomeAssistant("solenoid 1 cycle is on!");
      cycleSolenoid1 = true;
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      printUpdateToHomeAssistant("solenoid 1 cycle is off!");
      cycleSolenoid1 = false;
    } else parsingError = true;
  } else if (mqttTopic == "Desoto/Aero3/watering/solenoid2/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      printUpdateToHomeAssistant("solenoid 2 cycle is on!");
      cycleSolenoid2 = true;
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      printUpdateToHomeAssistant("solenoid 2 cycle is off!");
      cycleSolenoid2 = false;
    } else parsingError = true;
  } else if (mqttTopic == "Desoto/Aero3/watering/solenoid3/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      printUpdateToHomeAssistant("solenoid 3 cycle is on!");
      cycleSolenoid3 = true;
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      printUpdateToHomeAssistant("solenoid 3 cycle is off!");
      cycleSolenoid3 = false;
    } else parsingError = true;
  }
  
  //WATERING DURATIONS
  else if (mqttTopic == "Desoto/Aero3/watering/solenoid1/duration") {
    readMessage();
    Serial.println(mqttMessage);
    int newDuration = atoi(mqttMessage);
    printUpdateToHomeAssistant("Updating solenoid 1 watering duration to " + String(newDuration) + " seconds");
    wateringDurations[0] = newDuration;

  } else if (mqttTopic == "Desoto/Aero3/watering/solenoid2/duration") {
    readMessage();
    Serial.println(mqttMessage);
    int newDuration = atoi(mqttMessage);
    printUpdateToHomeAssistant("Updating solenoid 2 watering duration to " + String(newDuration) + " seconds");
    wateringDurations[1] = newDuration;
  } else if (mqttTopic == "Desoto/Aero3/watering/solenoid3/duration") {
    printUpdateToHomeAssistant("Should update solenoid 3 water duration");
    readMessage();
    Serial.println(mqttMessage);
    int newDuration = atoi(mqttMessage);
    printUpdateToHomeAssistant("Updating solenoid 3 watering duration to " + String(newDuration) + " seconds");
    wateringDurations[2] = newDuration;
  }
  
  //WATERING DELAY
  else if (mqttTopic == "Desoto/Aero3/watering/delay") {
    readMessage();
    Serial.println(mqttMessage);
    wateringDelay = atoi(mqttMessage);
    printUpdateToHomeAssistant("Updating delay to :" + String(wateringDelay));
    Alarm.write(wateringAlarmID, wateringDelay);
  }

  // mqttTopicS TO IGNORE
  else if (mqttTopic == "Desoto/Aero3/willmqttTopic") {
    Serial.println("Ignored mqttTopic");
  } else if (mqttTopic == "Desoto/Aero3/statusUpdate") {
    Serial.println("Ignored mqttTopic");
  } else if (mqttTopic == "Desoto/Aero3/solenoid1/state") {
    Serial.println("Ignored mqttTopic");
  } else if (mqttTopic == "Desoto/Aero3/solenoid2/state") {
    Serial.println("Ignored mqttTopic");
  } else if (mqttTopic == "Desoto/Aero3/solenoid3/state") {
    Serial.println("Ignored mqttTopic");
  }
  
  else {
    printUpdateToHomeAssistant("Update not recognized: ");
    printUpdateToHomeAssistant(mqttTopic);
  }

  //IF MESSAGE IN A mqttTopic WAS NOT RECOGNIZED
  if (parsingError) {
    printUpdateToHomeAssistant("message not recognized for mqttTopic: " + String(mqttTopic));
    printUpdateToHomeAssistant(" and message: " + String(mqttMessage));
    parsingError = false;
  }

  Serial.println("Parse/response time: " + String(millis() - startResponse) + " milliseconds");
  Serial.println();
}