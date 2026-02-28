#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "utils.h"

#if ENABLE_OLED
#include <Wire.h>
#include <U8g2lib.h>
#define DISPLAY_U8G2_AVAILABLE 1
#endif

#define DISPLAY_TIMER_IDLE    0
#define DISPLAY_TIMER_RUNNING 1
#define DISPLAY_TIMER_PAUSED  2
#define DISPLAY_TIMER_DONE    3

// Function declarations only
void display_setConnectivity(bool wifiConnected, bool mqttConnected);
void display_setTimerStatus(int remainSec, int totalSec, int timerState);
void display_init();
void display_update(int mode, const char* postureType, bool isAbnormal);
void display_showSplash();
void display_showMessage(const char* line1, const char* line2 = NULL);

#endif // DISPLAY_H