int solenoidPins[] = { 15, 33, 27 };

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