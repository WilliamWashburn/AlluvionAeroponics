#include <TimeLib.h>
#include <TimeAlarms.h>

int solenoidPins[] = {32,15,33};
int nbrOfPins = 3;

void setup() {

  Serial.begin(115200);
  while (!Serial)
    ;  // wait for Arduino Serial Monitor

  Serial.println();
  Serial.println();

  for (int i = 0; i < nbrOfPins; i++) {
    pinMode(solenoidPins[i], OUTPUT);
    digitalWrite(solenoidPins[i], LOW);
  }

  //SET ALARMS
  Alarm.timerRepeat(60, turnOnSolenoids);
  turnOnSolenoids();
}

void loop() {
  Alarm.delay(0);  //needed to service alarms
}

void turnOnSolenoids() {
  for (int i = 0; i < nbrOfPins; i++) {
    digitalWrite(solenoidPins[i], HIGH);
    Alarm.delay(7000);
    digitalWrite(solenoidPins[i], LOW);
    Alarm.delay(500);
  }
}