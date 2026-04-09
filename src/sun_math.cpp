#include "sun_math.h"
#include <math.h>

// --- constants ---
#define PI       3.14159265358979323846
#define RAD      (PI / 180.0)
#define DAY_MS   86400000.0
#define J1970    2440588.0
#define J2000    2451545.0
#define AXIAL_TILT (RAD * 23.4397) // earth's tilt in radians

// --- date conversion ---
// convert calendar date + time to Julian Date
static double toJulianDate(int year, int month, int day,
                            int hour, int minute, int second) {
  // using standard Julian Date formula
  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  double jdn = day + (153 * m + 2) / 5 + 365.0 * y
               + y / 4 - y / 100 + y / 400 - 32045.0;
  double dayFraction = (hour - 12.0) / 24.0
                     + minute / 1440.0
                     + second / 86400.0;
  return jdn + dayFraction;
}

// days since J2000 epoch
static double toDays(int year, int month, int day,
                     int hour, int minute, int second) {
  return toJulianDate(year, month, day, hour, minute, second) - J2000;
}

// --- sun coordinate math (same as React app) ---
static double solarMeanAnomaly(double d) {
  return RAD * (357.5291 + 0.98560028 * d);
}

static double eclipticLongitude(double M) {
  double C = RAD * (1.9148 * sin(M) + 0.02 * sin(2 * M) + 0.0003 * sin(3 * M));
  return M + C + RAD * 102.9372 + PI;
}

static double declination(double L) {
  return asin(sin(L) * sin(AXIAL_TILT));
}

static double rightAscension(double L) {
  return atan2(sin(L) * cos(AXIAL_TILT), cos(L));
}

static double siderealTime(double d, double lw) {
  return RAD * (280.16 + 360.9856235 * d) - lw;
}

static double azCalc(double H, double phi, double dec) {
  return atan2(sin(H),
               cos(H) * sin(phi) - tan(dec) * cos(phi));
}

static double altCalc(double H, double phi, double dec) {
  return asin(sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(H));
}

// --- public functions ---
SolarPosition getPosition(int year, int month, int day,
                          int hour, int minute, int second,
                          double lat, double lng) {
  double lw  = RAD * -lng;
  double phi = RAD * lat;
  double d   = toDays(year, month, day, hour, minute, second);

  double M = solarMeanAnomaly(d);
  double L = eclipticLongitude(M);
  double dec = declination(L);
  double ra  = rightAscension(L);
  double H   = siderealTime(d, lw) - ra;

  SolarPosition pos;
  pos.azimuth  = (azCalc(H, phi, dec) / RAD) + 180.0;
  pos.elevation = altCalc(H, phi, dec) / RAD;
  return pos;
}

SolarTimes getTimes(int year, int month, int day,
                    double lat, double lng) {
  double lw  = RAD * -lng;
  double phi = RAD * lat;
  double d   = toDays(year, month, day, 12, 0, 0); // use noon as base

  double M   = solarMeanAnomaly(d);
  double L   = eclipticLongitude(M);
  double dec = declination(L);

  double n   = round(d - 0.0009 - lw / (2 * PI));
  double ds  = 0.0009 + lw / (2 * PI) + n;
  double Jnoon = J2000 + ds + 0.0053 * sin(M) - 0.0069 * sin(2 * L);

  SolarTimes times;
  times.valid = false;

  // check for valid sunrise/sunset (acos domain must be [-1, 1])
  double cosW = (sin(-0.833 * RAD) - sin(phi) * sin(dec))
              / (cos(phi) * cos(dec));

  if (cosW < -1.0 || cosW > 1.0) {
    // polar day or polar night - no sunrise/sunset
    double noonUnix = (Jnoon + 0.5 - J1970) * DAY_MS / 1000.0;
    int noonSec = (int)noonUnix % 86400;
    times.noonHour = noonSec / 3600;
    times.noonMin  = (noonSec % 3600) / 60;
    return times;
  }

  double w    = acos(cosW);
  double Jset = J2000 + (0.0009 + (w + lw) / (2 * PI) + n)
              + 0.0053 * sin(M) - 0.0069 * sin(2 * L);
  double Jrise = Jnoon - (Jset - Jnoon);

  // convert Julian dates back to HH:MM UTC
  auto julianToHHMM = [](double J, int &h, int &m) {
    double unixSec = (J + 0.5 - J1970) * DAY_MS / 1000.0;
    int sec = (int)unixSec % 86400;
    h = sec / 3600;
    m = (sec % 3600) / 60;
  };

  julianToHHMM(Jrise, times.sunriseHour, times.sunriseMin);
  julianToHHMM(Jset,  times.sunsetHour,  times.sunsetMin);
  julianToHHMM(Jnoon, times.noonHour,    times.noonMin);
  times.valid = true;
  return times;
}

PanelTarget getPanelTarget(double elevation, double azimuth) {
  PanelTarget t;
  if (elevation <= 0) {
    t.tilt    = 90.0;  // stow vertical
    t.azimuth = 180.0; // face south
  } else {
    t.tilt    = 90.0 - elevation;
    t.azimuth = azimuth;
  }
  return t;
}