#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define PI            3.14159265358979323846
#define RAD           (PI / 180.0)
#define DAY_MS        86400000.0
#define J1970         2440588.0
#define J2000         2451545.0
#define AXIAL_TILT    (RAD * 23.4397)

struct SolarPosition { double azimuth; double elevation; };
struct PanelTarget   { double tilt;    double azimuth;   };
struct SolarTimes {
  int sunriseHour, sunriseMin;
  int sunsetHour,  sunsetMin;
  int noonHour,    noonMin;
  bool valid;
};

static double toJulianDate(int year, int month, int day, int hour, int minute, int second) {
  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  double jdn = day + (153 * m + 2) / 5 + 365.0 * y + y / 4 - y / 100 + y / 400 - 32045.0;
  return jdn + (hour - 12.0) / 24.0 + minute / 1440.0 + second / 86400.0;
}

static double toDays(int year, int month, int day, int hour, int minute, int second) {
  return toJulianDate(year, month, day, hour, minute, second) - J2000;
}

static double solarMeanAnomaly(double d) { return RAD * (357.5291 + 0.98560028 * d); }

static double eclipticLongitude(double M) {
  double C = RAD * (1.9148 * sin(M) + 0.02 * sin(2 * M) + 0.0003 * sin(3 * M));
  return M + C + RAD * 102.9372 + PI;
}

static double declination(double L)    { return asin(sin(L) * sin(AXIAL_TILT)); }
static double rightAscension(double L) { return atan2(sin(L) * cos(AXIAL_TILT), cos(L)); }
static double siderealTime(double d, double lw) { return RAD * (280.16 + 360.9856235 * d) - lw; }
static double azCalc(double H, double phi, double dec) { return atan2(sin(H), cos(H) * sin(phi) - tan(dec) * cos(phi)); }
static double altCalc(double H, double phi, double dec) { return asin(sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(H)); }

SolarPosition getPosition(int year, int month, int day, int hour, int minute, int second, double lat, double lng) {
  double lw = RAD * -lng, phi = RAD * lat;
  double d  = toDays(year, month, day, hour, minute, second);
  double M  = solarMeanAnomaly(d), L = eclipticLongitude(M);
  double H  = siderealTime(d, lw) - rightAscension(L);
  SolarPosition pos;
  pos.azimuth   = (azCalc(H, phi, declination(L)) / RAD) + 180.0;
  pos.elevation = altCalc(H, phi, declination(L)) / RAD;
  return pos;
}

SolarTimes getTimes(int year, int month, int day, double lat, double lng) {
  double lw = RAD * -lng, phi = RAD * lat;
  double d  = toDays(year, month, day, 12, 0, 0);
  double M  = solarMeanAnomaly(d), L = eclipticLongitude(M);
  double dec = declination(L);
  double n   = round(d - 0.0009 - lw / (2 * PI));
  double ds  = 0.0009 + lw / (2 * PI) + n;
  double Jnoon = J2000 + ds + 0.0053 * sin(M) - 0.0069 * sin(2 * L);
  SolarTimes times; times.valid = false;
  double cosW = (sin(-0.833 * RAD) - sin(phi) * sin(dec)) / (cos(phi) * cos(dec));
  if (cosW < -1.0 || cosW > 1.0) {
    int sec = (int)((Jnoon + 0.5 - J1970) * DAY_MS / 1000.0) % 86400;
    times.noonHour = sec / 3600; times.noonMin = (sec % 3600) / 60;
    return times;
  }
  double w    = acos(cosW);
  double Jset  = J2000 + (0.0009 + (w + lw) / (2 * PI) + n) + 0.0053 * sin(M) - 0.0069 * sin(2 * L);
  double Jrise = Jnoon - (Jset - Jnoon);
  auto toHHMM = [](double J, int &h, int &m) {
    int sec = (int)((J + 0.5 - J1970) * 86400000.0 / 1000.0) % 86400;
    h = sec / 3600; m = (sec % 3600) / 60;
  };
  toHHMM(Jrise, times.sunriseHour, times.sunriseMin);
  toHHMM(Jset,  times.sunsetHour,  times.sunsetMin);
  toHHMM(Jnoon, times.noonHour,    times.noonMin);
  times.valid = true;
  return times;
}

PanelTarget getPanelTarget(double elevation, double azimuth) {
  PanelTarget t;
  if (elevation <= 0) { t.tilt = 90.0; t.azimuth = 180.0; }
  else { t.tilt = 90.0 - elevation; t.azimuth = azimuth; }
  return t;
}

static int daysInMonth(int month, int year) {
  int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
  return days[month - 1];
}

static const char* monthName(int m) {
  const char* names[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  return names[m - 1];
}

// print every 5 min for one day
static void printDayFull(int year, int month, int day, double lat, double lng) {
  SolarTimes times = getTimes(year, month, day, lat, lng);

  printf("\n  --- %04d/%02d/%02d", year, month, day);
  if (times.valid)
    printf("  |  Sunrise: %02d:%02d UTC  Noon: %02d:%02d UTC  Sunset: %02d:%02d UTC",
           times.sunriseHour, times.sunriseMin,
           times.noonHour,    times.noonMin,
           times.sunsetHour,  times.sunsetMin);
  printf(" ---\n");

  printf("  %-8s %-12s %-12s %-12s %-12s %-8s\n",
         "Time", "Elev(deg)", "Az(deg)", "Tilt(deg)", "PanelAz", "Visible");
  printf("  %-8s %-12s %-12s %-12s %-12s %-8s\n",
         "--------","----------","----------","----------","----------","-------");

  for (int h = 0; h < 24; h++) {
    for (int m = 0; m < 60; m += 5) {
      SolarPosition pos = getPosition(year, month, day, h, m, 0, lat, lng);
      PanelTarget t = getPanelTarget(pos.elevation, pos.azimuth);
      printf("  %02d:%02d    %-12.2f %-12.2f %-12.2f %-12.2f %-8s\n",
             h, m,
             pos.elevation, pos.azimuth,
             t.tilt, t.azimuth,
             pos.elevation > 0 ? "YES" : "no");
    }
  }
}

static int parseDateInput(const char* input, int &year, int &month, int &day) {
  int slashes = 0;
  for (int i = 0; input[i]; i++) if (input[i] == '/') slashes++;
  if (slashes == 2) { if (sscanf(input, "%d/%d/%d", &month, &day, &year) == 3) return 1; }
  else if (slashes == 1) { if (sscanf(input, "%d/%d", &month, &year) == 2) return 2; }
  return 0;
}

int main() {
  printf("==========================================\n");
  printf("   Solar Tracker - Desktop Test CLI\n");
  printf("         ECE Capstone Team 20\n");
  printf("==========================================\n\n");

  double lat, lng;
  printf("Enter location (press Enter to use Columbus OH defaults)\n");
  printf("Lat (deg N) [40.0]: ");
  char buf[64];
  fgets(buf, sizeof(buf), stdin);
  lat = (buf[0] == '\n') ? 40.0 : atof(buf);

  printf("Lng (deg W) [83.0]: ");
  fgets(buf, sizeof(buf), stdin);
  lng = (buf[0] == '\n') ? 83.0 : atof(buf);
  lng = -fabs(lng);

  printf("\nLocation set: %.4f, %.4f\n", lat, lng);
  printf("------------------------------------------\n");
  printf("Date formats:\n");
  printf("  4/9/2026  ->  single day  (5-min intervals)\n");
  printf("  4/2026    ->  full month  (5-min intervals per day)\n");
  printf("  q         ->  quit\n");
  printf("------------------------------------------\n\n");

  while (true) {
    printf("Enter date: ");
    fgets(buf, sizeof(buf), stdin);
    buf[strcspn(buf, "\n")] = 0;

    if (buf[0] == 'q' || buf[0] == 'Q') { printf("Exiting.\n"); break; }

    int year = 0, month = 1, day = 1;
    int mode = parseDateInput(buf, year, month, day);

    if (mode == 0) {
      printf("  Could not parse. Try: 4/9/2026 or 4/2026\n\n");
      continue;
    }

    if (mode == 1) {
      // single day
      printDayFull(year, month, day, lat, lng);

    } else if (mode == 2) {
      // full month — each day gets 5-min breakdown
      int days = daysInMonth(month, year);
      printf("\n  === %s %04d (%d days) ===", monthName(month), year, days);
      for (int d = 1; d <= days; d++)
        printDayFull(year, month, d, lat, lng);
    }

    printf("\n");
  }

  return 0;
}