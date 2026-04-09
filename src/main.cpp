#include <Arduino.h>
#include "sun_math.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  // test: Columbus OH, today at solar noon
  SolarPosition pos = getPosition(2026, 4, 9, 17, 0, 0, 40.0, -83.0);
  // (17:00 UTC = ~1pm EDT, close to solar noon in Columbus)

  Serial.printf("Elevation: %.2f deg\n", pos.elevation);
  Serial.printf("Azimuth:   %.2f deg\n", pos.azimuth);

  PanelTarget t = getPanelTarget(pos.elevation, pos.azimuth);
  Serial.printf("Panel Tilt: %.2f deg\n", t.tilt);
  Serial.printf("Panel Az:   %.2f deg\n", t.azimuth);

  SolarTimes times = getTimes(2026, 4, 9, 40.0, -83.0);
  if (times.valid) {
    Serial.printf("Sunrise: %02d:%02d UTC\n", times.sunriseHour, times.sunriseMin);
    Serial.printf("Noon:    %02d:%02d UTC\n", times.noonHour,    times.noonMin);
    Serial.printf("Sunset:  %02d:%02d UTC\n", times.sunsetHour,  times.sunsetMin);
  }
}

void loop() {}