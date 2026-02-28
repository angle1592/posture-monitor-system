#ifndef VOICE_H
#define VOICE_H

#include <Arduino.h>
#include "config.h"
#include "utils.h"

// Function declarations only (no inline, no bodies)
void voice_init();
void voice_speak(const char* text);
void voice_speakWithParam(const char* text, uint8_t frameParam);
void voice_speakGbk(const uint8_t* gbkBytes, size_t byteLen, uint8_t frameParam);
void voice_sendDemoPacket();
void voice_sendPacket(const uint8_t* packet, size_t len);
void voice_speakRaw(const char* text, bool appendCrlf);
void voice_setFeedbackLogEnabled(bool enabled);
void voice_update();
bool voice_isBusy();
void voice_stop();

#endif // VOICE_H