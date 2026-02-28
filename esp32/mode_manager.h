#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "utils.h"

enum ModeClickEvent {
    MODE_CLICK_NONE = 0,
    MODE_CLICK_SHORT,
    MODE_CLICK_LONG
};

// Function declarations only
void mode_init();
void mode_update();
SystemMode mode_getCurrent();
bool mode_setCurrent(SystemMode mode);
bool mode_hasChanged();
const char* mode_getName();
void mode_setRotationLocked(bool locked);
int mode_takeRotationDelta();
ModeClickEvent mode_takeClickEvent();

#endif // MODE_MANAGER_H