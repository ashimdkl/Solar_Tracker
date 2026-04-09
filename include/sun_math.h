#pragma once
#include <Arduino.h>

struct SolarPosition {
  double azimuth;
  double elevation;
};

struct SolarTimes {
  int sunriseHour, sunriseMin;
  int sunsetHour,  sunsetMin;
  int noonHour,    noonMin;
  bool valid; // false if no sunrise/sunset (polar edge case)
};

struct PanelTarget {
  double tilt;
  double azimuth;
};

// main functions that we will call from main.cpp
SolarPosition getPosition(int year, int month, int day,
                          int hour, int minute, int second,
                          double lat, double lng);

SolarTimes    getTimes(int year, int month, int day,
                       double lat, double lng);

PanelTarget   getPanelTarget(double elevation, double azimuth);