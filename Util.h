#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <string>
#include <vector>
#include "log.h"

using namespace std;

#define CONFIG_FILE "config.ini"

class zone
{
public:
    string up;
    string down;
    string left;
    string right;
    string alarm_threshold_width;
    string alarm_threshold_height;
};

class configure
{
public:
    string risk_count;
    string check_interval;
    string check_interval_night;
    string morning;
    string night;
    string communicator_server;
    string tower_name;
};

class status
{
public:
    string wifi;
    string _3G;
    string camera0_power;
    string camera1_power;
    string camera0;
    string camera1;
    string sendsms;
    string alarm;
};

class battery
{
public:
    double solar_voltage;
    double battery_voltage;
    double charging_current;
    double discharging_current;
    double illumination_intensity;
};

class GPS
{
public:
    string longitude;
    string latitude;
};

class CUtil
{
private:
    static bool m_b3G;
    static bool m_bWifi;
    static bool m_bCamera0;
    static bool m_bCamera1;
    static bool m_bCamera0Power;
    static bool m_bCamera1Power;

public:
    CUtil(){};

    ~CUtil();
    static string GetUserName();
    static string GetUserPwd();

    static zone GetMonitorZone();
    static void SetMonitorZone(zone z);

    static configure GetConfigure();
    static void SetConfigure(configure z);

    static status GetStatus();
    //static void SetStatus(status p);

    static battery GetBattery();
    static void SetBattery(battery p);

    static GPS GetGPS();
    static void SetGPS(GPS g);

    static bool GenerateDeviceID(string& deviceID);
    static void ChangeAPSSIDIfNeeded();
    static void OpenAP();
    static void CloseAP();

    static int ini_query_int(const char* section, const char* key, const int notfound);
    static bool ini_save_int(const char* section, const char* key, const int value);
    static bool ini_query_bool(const char* section, const char* key, const bool notfound);
    static bool ini_save_bool(const char* section, const char* key, const bool value);
    static string ini_query_string(const char* section, const char* key, char* notfound);
    static bool ini_save_string(const char* section, const char* key, const char* value);
    static float ini_query_float(const char* section, const char* key, const float notfound);
    static bool ini_save_float(const char* section, const char* key, const float value);

    static string GetBoardID()
    {
        return ini_query_string("Global", "board_id", "");
    }

    static bool SetBoardID(char* board_id)
    {
        return ini_save_string("Global", "board_id", board_id);
    }

    static bool IsAPEnabled()
    {
        return m_bWifi; //ini_query_bool("status", "wifi", true);
    }

    static bool SaveAPStatus(bool bEnabled)
    {
        m_bWifi = bEnabled;
        return true; //ini_save_bool("status", "wifi", bEnabled);
    }

    static bool Is3GEnabled()
    {
        return m_b3G; //ini_query_bool("status", "3g", false);
    }

    static bool Save3GStatus(bool bEnabled)
    {
        m_b3G = bEnabled;
        return true; //ini_save_bool("status", "3g", bEnabled);
    }

    static bool IsCameraEnabled(int index)
    {
        if(index == 0)
            return m_bCamera0; //ini_query_bool("status", "camera0", true);
        else
            return m_bCamera1; //ini_query_bool("status", "camera1", true);
    }

    static bool SaveCameraStatus(int index, bool bEnabled)
    {
        if(index == 0)
            m_bCamera0 = bEnabled; //return ini_save_bool("status", "camera0", bEnabled);
        else
            m_bCamera1 = bEnabled; //return ini_save_bool("status", "camera1", bEnabled);
    }

    static bool SetCameraPowerStatus(int index, bool enable)
    {
        if(index == 0)
            m_bCamera0Power = enable; //return ini_save_bool("status", "camera0_power", enable);
        else
            m_bCamera1Power = enable; //return ini_save_bool("status", "camera1_power", enable);
    }

    static bool GetCameraPowerStatus(int index)
    {
        if(index == 0)
            return m_bCamera0Power; //ini_query_bool("status", "camera0_power", false);
        else
            return m_bCamera1Power; //ini_query_bool("status", "camera1_power", false);
    }

    static bool GetAlarmStatus()
    {
        return CUtil::ini_query_bool("status", "alarm", true);
    }

    static bool SetAlarmStatus(bool status)
    {
        return CUtil::ini_save_bool("status", "alarm", status);
    }

    static bool SetSendSMSStatus(bool enable)
    {
        return CUtil::ini_save_bool("status", "sms", enable);
    }

    static bool GetSendSMSStatus()
    {
        return CUtil::ini_query_bool("status", "sms", false);
    }

    static bool SaveLocation(string location)
    {
        return ini_save_string("configure", "tower_name", location.c_str());
    }

    static string GetLocation()
    {
        return ini_query_string("configure", "tower_name", (char*)GetBoardID().c_str());
    }

    static bool SetCheckInterval(int interval)
    {
        return CUtil::ini_save_int("configure", "check_interval", interval);
    }

    static int GetCheckInterval()
    {
        return CUtil::ini_query_int("configure", "check_interval", 120);
    }
    
    static bool SetCheckInterval_Night(int interval)
    {
        return CUtil::ini_save_int("configure", "check_interval_night", interval);
    }

    static int GetCheckInterval_Night()
    {
        return CUtil::ini_query_int("configure", "check_interval_night", 120);
    }


    static bool SetRiskCount(int risk_count)
    {
        return CUtil::ini_save_int("configure", "risk_count", risk_count);
    }

    static int GetRiskCount()
    {
        return CUtil::ini_query_int("configure", "risk_count", 3);
    }
    
    static bool SetAlarmCount(int alarmtime)
    {
        return CUtil::ini_save_int("configure", "alarm_count", alarmtime);
    }

    static int GetAlarmCount()
    {
        return CUtil::ini_query_int("configure", "alarm_count", 3);
    }

    static bool SavePhoneNumber(string phoneNumber);
    static bool RemovePhoneNumber(string phoneNumber);
    static vector<string> GetPhoneNumber();

    static float CalculateCarThreshold();
    static int FindPidByName(char* pName);
    static int Exec(char* path, char* arg, bool sync);
    static bool isFileExist(const char* file);
    static bool CheckInternetConnection();
    static bool CheckProxyServer();
    static string CheckVersion();
    static void BackupOrRestoreIni();
    static int GetMorningInt();
    static int GetNightInt();
    static bool SetMorningInt(int morning);
    static bool SetNightInt(int night);
    static void SyncDateTime();
};

#endif // _UTIL_H_

