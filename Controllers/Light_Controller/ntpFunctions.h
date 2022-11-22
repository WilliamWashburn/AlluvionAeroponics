#include <time.h>

//NTP
const char* NTP_SERVER = "pool.ntp.org";
const char* TZ_INFO = "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00";  // https://support.cyberdata.net/portal/en/kb/articles/010d63c0cfce3676151e1f2d5442e311
tm timeinfo;
time_t nowTime;
long unsigned lastNTPtime;
unsigned long lastEntryTime;

bool getNTPtime(int sec) {
  uint32_t start = millis();
  do {
    time(&nowTime);
    localtime_r(&nowTime, &timeinfo);
    Serial.print(".");
    delay(10);
  } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));
  if (timeinfo.tm_year <= (2016 - 1900)) return false;  // the NTP call was not successful
  Serial.print("nowTime ");
  Serial.println(nowTime);
  char time_output[30];
  strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&nowTime));
  Serial.println(time_output);
  Serial.println();
  return true;
}

void showTime(tm localTime) {
  Serial.print(localTime.tm_mday);
  Serial.print('/');
  Serial.print(localTime.tm_mon + 1);
  Serial.print('/');
  Serial.print(localTime.tm_year - 100);  //give 22 not 2022 since tm_year is years since 1900
  Serial.print('-');
  Serial.print(localTime.tm_hour);
  Serial.print(':');
  Serial.print(localTime.tm_min);
  Serial.print(':');
  Serial.print(localTime.tm_sec);
  Serial.print(" Day of Week ");
  if (localTime.tm_wday == 0) Serial.println(7);
  else Serial.println(localTime.tm_wday);
}