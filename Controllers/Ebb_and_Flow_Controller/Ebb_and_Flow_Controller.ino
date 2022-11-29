#include "credentials.h"  //you need to create this file and #define mySSID and myPASSWORD. or comment this out and fill in below
#include "mqttFunctions.h"

#include <WiFi.h>

#include <TimeLib.h>
#include <TimeAlarms.h>

#include <ArduinoMqttClient.h>

//WIFI
const char* ssid = mySSID;
const char* password = myPASSWORD;

//ALARMS
AlarmID_t watering1AlarmID;
AlarmID_t watering2AlarmID;
AlarmID_t watering3AlarmID;
AlarmID_t watering4AlarmID;

//CONVERSIONS
long HOURTOMILLISEC = 60L * 60L * 1000L;
long MINTOMILLISEC = 60L * 1000L;

//PIN DEFINITIONS
int pumpPin = 26;
int solenoidPins[] = { 13, 12, 27, 14, 33, 15, 32 };  //solenoid pins

//SOLENOID INFORMATION
const int nbrOfSolenoids = 4;                                                                                                                                                            //number of solenoids connected
long levelDelays[] = { 8, 8, 8, 8, 8, 8, 8 };                                                                                                                                            //hours. how long to wait between watering
long levelLastWatered[] = { -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC, -24 * HOURTOMILLISEC };  //milliseconds. the last time that the level was watered, -24 makes it water immediately
long levelWaterDurations[] = { 6, 4, 6, 12, 12, 12, 12 };                                                                                                                                //minutes. how long each level should water for
long levelDrainDurations[] = { 21, 12, 12, 12, 22, 22, 22 };                                                                                                                             //minutes. how long each level should drain for
int pumpPWMTime = 8000;                                                                                                                                                                  //milliseconds. How long the pump should pwm for

bool waterLevelFlags[nbrOfSolenoids + 1];  //flags to water levels. The last index is for watering all the levels

//---------------SETUP--------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Beginning...");

  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);  //turn pump off

  for (int i = 0; i < nbrOfSolenoids; i++) {
    pinMode(solenoidPins[i], OUTPUT);
    digitalWrite(solenoidPins[i], LOW);  //turn solenoids off
  }

  //initialize flags
  for (int i = 0; i < nbrOfSolenoids + 1; i++) {
    waterLevelFlags[i] = false;
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

  //CONNECT TO MQTT BROKER
  mqttClient.onMessage(onMqttMessage);  // set the message receive callback
  connectToBroker();

  //SET ALARMS
  //ill need to get the ID of the alarm to be able to update it
  watering1AlarmID = Alarm.alarmRepeat(9, 00, 0, waterLevels);
  watering2AlarmID = Alarm.alarmRepeat(12, 00, 0, waterLevels);
  watering3AlarmID = Alarm.alarmRepeat(17, 00, 0, waterLevels);
  watering4AlarmID = Alarm.alarmRepeat(22, 00, 0, waterLevels);
}

//--------------LOOP---------------
void loop() {
  Alarm.delay(0);  //needed to service alarms

  mqttClient.poll();  // call poll() regularly to allow the library to receive MQTT messages and send MQTT keep alives which avoids being disconnected by the broker

  if (!mqttClient.connected()) connectToBroker();
  if (WiFi.status() != WL_CONNECTED) connectToWifi();

  checkWateringFlags();
}

void checkWateringFlags() {
  for (int i = 0; i < nbrOfSolenoids; i++) {
    if (waterLevelFlags[i]) {
      Serial.println("Watering level " + String(i));
      waterLevel(i + 1);
      waterLevelFlags[i] = false;
    }
  }
  //this last flag is for watering all the levels
  if (waterLevelFlags[nbrOfSolenoids]) {
    Serial.println("Watering all levels ");
    waterLevels();
    waterLevelFlags[nbrOfSolenoids] = false;
  }
}

void turnOffSolenoids() {
  for (int i = 0; i < nbrOfSolenoids; i++) {
    digitalWrite(solenoidPins[i], LOW);
  }
}

void printInfo() {
  //Print time left
  for (int level = 0; level < nbrOfSolenoids; level++) {
    Serial.print("Time Left for level " + String(level) + " : ");
    Serial.print(((levelDelays[level] * HOURTOMILLISEC) - (millis() - levelLastWatered[level])) / (1000L * 60L));
    Serial.println(" minutes");
  }

  //print last watered
  Serial.print("Last Watered: ");
  for (int level = 0; level < nbrOfSolenoids; level++) {
    Serial.print(levelLastWatered[level]);
  }
  Serial.println(" milliseconds");
}

void turnOnSolenoid1() {
  digitalWrite(solenoidPins[0], HIGH);
}
void turnOffSolenoid1() {
  digitalWrite(solenoidPins[0], LOW);
}
bool solenoid1State() {
  return digitalRead(solenoidPins[0]);
}

void turnOnSolenoid2() {
  digitalWrite(solenoidPins[1], HIGH);
}
void turnOffSolenoid2() {
  digitalWrite(solenoidPins[1], LOW);
}
bool solenoid2State() {
  return digitalRead(solenoidPins[1]);
}

void turnOnSolenoid3() {
  digitalWrite(solenoidPins[2], HIGH);
}
void turnOffSolenoid3() {
  digitalWrite(solenoidPins[2], LOW);
}
bool solenoid3State() {
  return digitalRead(solenoidPins[2]);
}

void turnOnSolenoid4() {
  digitalWrite(solenoidPins[3], HIGH);
}
void turnOffSolenoid4() {
  digitalWrite(solenoidPins[3], LOW);
}
bool solenoid4State() {
  return digitalRead(solenoidPins[3]);
}


void turnOnPump() {
  digitalWrite(pumpPin, HIGH);
}
void turnOffPump() {
  digitalWrite(pumpPin, LOW);
}
bool pumpState() {
  return digitalRead(pumpPin);
}

void waterLevel1() {
  waterLevel(1);
  drainLevel(1);
}
void waterLevel2() {
  waterLevel(2);
  drainLevel(2);
}
void waterLevel3() {
  waterLevel(3);
  drainLevel(3);
}
void waterLevel4() {
  waterLevel(4);
  drainLevel(4);
}

void drainLevels() {
  for (int i = 1; i <= nbrOfSolenoids; i++) {
    drainLevel(i);
  }
}
void waterLevels() {
  for (int i = 1; i <= nbrOfSolenoids; i++) {
    waterLevel(i);
  }
  drainLevels();
}

//this is a mock delay function that still does whatever we need to do such as maintain connection with mqtt
void myDelay(long delayTime, bool shouldPrint = false) {
  long startTime = millis();
  while (millis() - startTime < delayTime) {
    //pass
    static long printTime = 0;
    if (millis() - printTime > 1000 && shouldPrint == true) {
      Serial.println("Time left: " + String((delayTime - (millis() - startTime)) / 1000) + " seconds");
      printTime = millis();
    }
    mqttClient.poll();
  }
}

long draingTime = 120000;  //milliseconds
void drainLevel(int sol) {
  digitalWrite(solenoidPins[sol - 1], HIGH);
  long startTime = millis();
  while (millis() - startTime < draingTime) {
    //pass
    static long printTime = 0;
    if (millis() - printTime > 1000) {
      printToBroker(String((draingTime - (millis() - startTime)) / 1000L) + " seconds in draining");
      printTime = millis();
    }
    mqttClient.poll();
  }
  digitalWrite(solenoidPins[sol - 1], LOW);
}

void pwmPump() {
  long pwmStartTime = millis();
  Serial.println("toggling pump");
  while (millis() - pwmStartTime < pumpPWMTime) {
    digitalWrite(pumpPin, LOW);
    delay(200);  // how long to stay off

    digitalWrite(pumpPin, HIGH);
    delay(100);  // how long to stay on
  }
}

void waterLevel(int level) {
  turnOffSolenoids();  //just incase one is open

  //build mqtt topic for updating state of watering
  char updateTopic[50] = "Desoto/EbbNFlow/watering/";
  char levelNbrAsChar[4];
  strcat(updateTopic, itoa(level, levelNbrAsChar, 10));
  strcat(updateTopic, "/wateringUpdate");

  levelInx = level - 1;  //adjust so level 1 is index 0
  Serial.println("Starting to water for " + String(levelWaterDurations[levelInx] * 60L) + " seconds");
  levelLastWatered[levelInx] = millis();  //update record of timing

  //update to mqtt
  mqttClient.beginMessage(updateTopic);
  mqttClient.print("starting to water");  //triggers message in telegram through node red on home assistant
  mqttClient.endMessage();

  //open solenoid
  digitalWrite(solenoidPins[levelInx], HIGH);

  //PWM the pump
  pwmPump();

  //turn solenoid on and wait
  digitalWrite(pumpPin, HIGH);
  long wateringStartTime = millis();
  while (millis() - wateringStartTime < levelWaterDurations[levelInx] * MINTOMILLISEC) {
    int secondsLeft = (int)((levelWaterDurations[levelInx] * MINTOMILLISEC) - (millis() - wateringStartTime)) / 1000;
    static int prevSecondsLeft = 0;
    if (secondsLeft != prevSecondsLeft) {
      printToBroker(String(secondsLeft) + " seconds left in watering " + String(level));
      prevSecondsLeft = secondsLeft;
    }
    mqttClient.poll();
  }

  //turn off pump
  digitalWrite(pumpPin, LOW);

  mqttClient.beginMessage(updateTopic);
  mqttClient.print("starting to drain");
  mqttClient.endMessage();
  long startDrain = millis();
  while (millis() - startDrain < levelDrainDurations[levelInx] * MINTOMILLISEC) {
    //pass
    int secondsLeft = (int)((levelDrainDurations[levelInx] * MINTOMILLISEC) - (millis() - startDrain)) / 1000;
    static int prevSecondsLeft = 0;
    if (secondsLeft != prevSecondsLeft) {
      printToBroker(String(secondsLeft) + " seconds left in draining" + String(level));
      prevSecondsLeft = secondsLeft;
    }
    mqttClient.poll();
  }
  //turn off solenoid
  digitalWrite(solenoidPins[levelInx], LOW);

  //send ending message to MQTT
  mqttClient.beginMessage(updateTopic);
  mqttClient.print("draining ended");
  mqttClient.endMessage();

  printToBroker("Finished watering level " + String(level));
}


void printTimeAndDate() {
  Serial.println("Time: " + String(hour()) + ":" + String(minute()) + ":" + String(second()));
  Serial.println("Date: " + String(month()) + "/" + String(day()) + "/" + String(year()));
}

void saveTime(char* inputString) {
  char timeStr[50];
  strcpy(timeStr, inputString);
  Serial.println("We received: ");
  Serial.println(timeStr);  //expecting of form "2022-11-23, 13:34"
  char* yearHA = strtok(timeStr, "-");
  char* monthHA = strtok(NULL, "-");
  char* dayHA = strtok(NULL, ",");
  char* hourHA = strtok(NULL, ":");
  char* minuteHA = strtok(NULL, ":");
  setTime(atoi(hourHA), atoi(minuteHA), 0, atoi(dayHA), atoi(monthHA), atoi(yearHA));
  printTimeAndDate();
}

void waterLevelXsetFlag(int levelNbr) {
  waterLevelFlags[levelNbr - 1] = true;  //level 1 should be index 0

  Serial.println("The flags are now");
  for (int i = 0; i < nbrOfSolenoids + 1; i++) {
    Serial.print(String(waterLevelFlags[i]));
    Serial.print(",");
  }
  Serial.println();
}

long convertTimeToSecondsAfterMidnight(char* hourTime, char* minuteTime, char secondTime[] = "0") {
  return atoi(hourTime) * 3600L + atoi(minuteTime) * 60L + atoi(secondTime);
}

long parseTime(char* timeString) {
  //parse
  char hourOff[10];
  char minOff[10];
  strcpy(hourOff, strtok(timeString, ":"));
  strcpy(minOff, strtok(NULL, ":"));
  Serial.println("time parsed - " + String(hourOff) + ":" + String(minOff));

  return convertTimeToSecondsAfterMidnight(hourOff, minOff);
}

bool parsingError = false;  //flag for parsing error. Either topic or message not recognized

//everything in this function should be executed quickly. If there is any need for a delay, think about setting a flag in this function and responding in the loop
void onMqttMessage(int messageSize) {
  long startResponse = millis();

  String topic = mqttClient.messageTopic();
  Serial.println();
  Serial.println(topic);

  //MANUALLY CONTROL SOLENOIDS/PUMP
  if (topic == "Desoto/EbbNFlow/solenoids/1/command") {
    readMessage();
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!solenoid1State()) {  //if not on
        printToBroker("Should turn on solenoid 1");
        turnOnSolenoid1();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (solenoid1State()) {  //if on
        printToBroker("Should turn off solenoid 1");
        turnOffSolenoid1();
      }
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/solenoids/2/command") {
    readMessage();
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!solenoid2State()) {  //if not on
        printToBroker("Should turn on solenoid 2");
        turnOnSolenoid2();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (solenoid2State()) {  //if on
        printToBroker("Should turn off solenoid 2");
        turnOffSolenoid2();
      }
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/solenoids/3/command") {
    readMessage();
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!solenoid3State()) {  //if not on
        printToBroker("Should turn on solenoid 3");
        turnOnSolenoid3();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (solenoid3State()) {  //if on
        printToBroker("Should turn off solenoid 3");
        turnOffSolenoid3();
      }
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/solenoids/4/command") {
    readMessage();
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!solenoid4State()) {  //if not on
        printToBroker("Should turn on solenoid 4");
        turnOnSolenoid4();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (solenoid4State()) {  //if on
        printToBroker("Should turn off solenoid 4");
        turnOffSolenoid4();
      }
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/pump/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "{\"state\":\"on\"}") == 0) {
      if (!pumpState()) {  //if not on
        printToBroker("Should turn pump on");
        turnOnPump();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"off\"}") == 0) {
      if (pumpState()) {  //if on
        printToBroker("Should turn pump off");
        turnOffPump();
      }
    } else if (strcmp(mqttMessage, "{\"state\":\"start\"}") == 0) {
      if (!pumpState()) {  //if on
        printToBroker("Should start the pump");
        pwmPump();
        turnOnPump();
      }
    } else parsingError = true;
  }

  //MANUALLY TRIGGGER WATERINGS
  else if (topic == "Desoto/EbbNFlow/watering/1/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "start") == 0) {
      printToBroker("Should water level 1");
      waterLevelXsetFlag(1);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/2/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "start") == 0) {
      printToBroker("Should water level 2");
      waterLevelXsetFlag(2);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/3/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "start") == 0) {
      printToBroker("Should water level 3");
      waterLevelXsetFlag(3);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/4/command") {
    readMessage();
    Serial.println(mqttMessage);
    if (strcmp(mqttMessage, "start") == 0) {
      printToBroker("Should water level 4");
      waterLevelXsetFlag(4);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/all/command") {
    readMessage();
    if (strcmp(mqttMessage, "start") == 0) {
      printToBroker("Should water all levels");
      waterLevelXsetFlag(nbrOfSolenoids + 1);  //this will set the flag of index: nbrOfSolenoids
    } else parsingError = true;
  }

  // UPDATE WATER DURATIONS
  else if (topic == "Desoto/EbbNFlow/watering/1/duration") {
    readMessage();
    levelWaterDurations[0] = atoi(mqttMessage);
    printToBroker("Updated watering duration 1 to " + String(levelWaterDurations[0] + " minutes"));
  } else if (topic == "Desoto/EbbNFlow/watering/2/duration") {
    readMessage();
    levelWaterDurations[1] = atoi(mqttMessage);
    printToBroker("Updated watering duration 2 to " + String(levelWaterDurations[1] + " minutes"));
  } else if (topic == "Desoto/EbbNFlow/watering/3/duration") {
    readMessage();
    levelWaterDurations[2] = atoi(mqttMessage);
    printToBroker("Updated watering duration 3 to " + String(levelWaterDurations[2] + " minutes"));
  } else if (topic == "Desoto/EbbNFlow/watering/4/duration") {
    readMessage();
    levelWaterDurations[3] = atoi(mqttMessage);
    printToBroker("Updated watering duration 4 to " + String(levelWaterDurations[3] + " minutes"));
  }

  //UPDATE WATERING SCHEDULE
  else if (topic == "Desoto/EbbNFlow/watering/schedule/watering1") {
    readMessage();
    long scheduledTime = parseTime(mqttMessage);
    printToBroker("Should update level 1 watering schedule to " + String(scheduledTime));
    Alarm.write(watering1AlarmID, scheduledTime);
  } else if (topic == "Desoto/EbbNFlow/watering/schedule/watering2") {
    readMessage();
    long scheduledTime = parseTime(mqttMessage);
    printToBroker("Should update level 2 watering schedule to " + String(scheduledTime));
    Alarm.write(watering2AlarmID, scheduledTime);
  } else if (topic == "Desoto/EbbNFlow/watering/schedule/watering3") {
    readMessage();
    long scheduledTime = parseTime(mqttMessage);
    printToBroker("Should update level 3 watering schedule to " + String(scheduledTime));
    Alarm.write(watering3AlarmID, scheduledTime);
  } else if (topic == "Desoto/EbbNFlow/watering/schedule/watering4") {
    readMessage();
    long scheduledTime = parseTime(mqttMessage);
    printToBroker("Should update level 4 watering schedule to " + String(scheduledTime));
    Alarm.write(watering4AlarmID, scheduledTime);
  }

  //TRIGGER DRAINING
  else if (topic == "Desoto/EbbNFlow/watering/drain") {
    printToBroker("Should drain levels");
    readMessage();
    printToBroker(mqttMessage);
    drainLevels();
  }

  // FINAL DRAIN TIME
  else if (topic == "Desoto/EbbNFlow/watering/finalDrainTime") {
    readMessage();
    draingTime = int(atof(mqttMessage) * MINTOMILLISEC);
    printToBroker("Should update drain time to " + String(draingTime) + " milliseconds");
  }

  //UPDATE TIME AND DATE
  else if (topic == "homeassistant/dateAndTime") {
    readMessage();
    printToBroker("Received message: " + String(mqttMessage));
    saveTime(mqttMessage);
  }

  // WATERING SCHEDULE STATUS
  else if (topic == "Desoto/EbbNFlow/watering/1/status") {
    readMessage();
    if (strcmp(mqttMessage, "on") == 0) {
      printToBroker("Enabling watering alarm 1");
      Alarm.enable(watering1AlarmID);
    } else if (strcmp(mqttMessage, "off") == 0) {
      printToBroker("Disabling watering alarm 1");
      Alarm.disable(watering1AlarmID);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/2/status") {
    readMessage();
    if (strcmp(mqttMessage, "on") == 0) {
      printToBroker("Enabling watering alarm 2");
      Alarm.enable(watering2AlarmID);
    } else if (strcmp(mqttMessage, "off") == 0) {
      printToBroker("Disabling watering alarm 2");
      Alarm.disable(watering2AlarmID);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/3/status") {
    readMessage();
    if (strcmp(mqttMessage, "on") == 0) {
      printToBroker("Enabling watering alarm 3");
      Alarm.enable(watering3AlarmID);
    } else if (strcmp(mqttMessage, "off") == 0) {
      printToBroker("Disabling watering alarm 3");
      Alarm.disable(watering3AlarmID);
    } else parsingError = true;
  } else if (topic == "Desoto/EbbNFlow/watering/4/status") {
    readMessage();
    if (strcmp(mqttMessage, "on") == 0) {
      printToBroker("Enabling watering alarm 4");
      Alarm.enable(watering4AlarmID);
    } else if (strcmp(mqttMessage, "off") == 0) {
      printToBroker("Disabling watering alarm 4");
      Alarm.disable(watering4AlarmID);
    } else parsingError = true;
  }

  // TOPICS TO IGNORE
  else if (topic == "Desoto/EbbNFlow/willTopic") {
    Serial.println("Ignored");
  } else if (topic == "Desoto/EbbNFlow/statusUpdate") {
    Serial.println("Ignored");
  }


  //IF TOPIC NOT RECOGNIZED
  else {
    printToBroker("Update not recognized: ");
    printToBroker(topic);
  }

  //IF MESSAGE IN A TOPIC WAS NOT RECOGNIZED
  if (parsingError) {
    printToBroker("message not recognized for topic: " + String(topic));
    printToBroker(" and message: " + String(mqttMessage));
    parsingError = false;
  }

  Serial.println("Parse/response time: " + String(millis() - startResponse) + " milliseconds");
  Serial.println();
}

bool connectToWifi() {
  Serial.println("Connecting to wifi: " + String(ssid));
  WiFi.begin(ssid, password);

  long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 60000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println(" Failed:(");
  return false;
}