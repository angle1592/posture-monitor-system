#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>
#include "config.h"
#include "utils.h"

#define ALERT_MODE_LED      0x01
#define ALERT_MODE_BUZZER   0x02
#define ALERT_MODE_VOICE    0x04

void alerts_init();
void alerts_setAlertMode(uint8_t modeMask);
uint8_t alerts_getAlertMode();
void alerts_setColor(uint8_t r, uint8_t g, uint8_t b);
void alerts_lockIndicator(bool lock, uint8_t r, uint8_t g, uint8_t b);
void alerts_triggerBuzzerPulse(unsigned long durationMs);
void alerts_beep(int durationMs);
bool alerts_voiceEnabled();
void alerts_update(bool mqttConnected, bool k230Valid, bool isAbnormal, bool noPerson);
void alerts_off();

#endif // ALERTS_H