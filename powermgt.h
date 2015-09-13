#ifndef __POWER_MGT__
#define __POWER_MGT__

#include "Util.h"
#include <unistd.h>
#include <stdint.h>

bool SendHeartBeatCmd(uint16_t seconds);
bool SendRestartOSCmd(uint16_t delay);
bool SendCameraPowerCmd(uint8_t cam_index, uint16_t delay, bool enable);
bool SendSolarStatusCmd();
bool SendSetSystemTime(int year, int month, int day, int hour, int minute, int second);
bool SendSetAlarmVoltage(double voltage);
bool SendSetNightInterval(int month, int start_hour, int start_minute, int end_hour, int end_minute);

void _open(string uuid, int index, bool sync);
void _close(string uuid, int index);

#endif
