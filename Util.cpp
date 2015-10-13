#include "Util.h"
#include "minIni.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <jsoncpp/json/json.h>
#include <curl/curl.h>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/algorithm/string/predicate.hpp>

using namespace std;

boost::mutex m;
static battery g_Battery;

bool CUtil::m_b3G = true;
bool CUtil::m_bWifi = true;
bool CUtil::m_bCamera0 = true;
bool CUtil::m_bCamera1 = true;
bool CUtil::m_bCamera0Power = false;
bool CUtil::m_bCamera1Power = false;

string CUtil::GetUserName()
{
    return ini_query_string("Global", "username", "");
}

string CUtil::GetUserPwd()
{
    return ini_query_string("Global", "password", "");
}

zone CUtil::GetMonitorZone()
{
    zone z;
    z.up = CUtil::ini_query_string("init", "up", "");
    z.down = CUtil::ini_query_string("init", "down", "");
    z.left = CUtil::ini_query_string("init", "left", "");
    z.right = CUtil::ini_query_string("init", "right", "");
    z.alarm_threshold_width = CUtil::ini_query_string("init", "alarm_threshold_width", "");
    z.alarm_threshold_height = CUtil::ini_query_string("init", "alarm_threshold_height", "");
    return z;
}

void CUtil::SetMonitorZone(zone z)
{
    CUtil::ini_save_string("init", "up", z.up.c_str());
    CUtil::ini_save_string("init", "down", z.down.c_str());
    CUtil::ini_save_string("init", "left", z.left.c_str());
    CUtil::ini_save_string("init", "right", z.right.c_str());
    CUtil::ini_save_string("init", "alarm_threshold_width", z.alarm_threshold_width.c_str());
    CUtil::ini_save_string("init", "alarm_threshold_height", z.alarm_threshold_height.c_str());
}

configure CUtil::GetConfigure()
{
    configure c;
    c.risk_count = CUtil::ini_query_string("configure", "risk_count", "1");
    c.check_interval = CUtil::ini_query_string("configure", "check_interval", "200");
    c.check_interval_night = CUtil::ini_query_string("configure", "check_interval_night", "200");
    c.morning = CUtil::ini_query_string("configure", "morning", "6");
    c.night = CUtil::ini_query_string("configure", "night", "18");
    c.communicator_server = CUtil::GetServerIP();
    c.tower_name = CUtil::ini_query_string("configure", "tower_name", "");
    return c;
}

void CUtil::SetConfigure(configure c)
{
    CUtil::ini_save_string("configure", "risk_count", c.risk_count.c_str());
    CUtil::ini_save_string("configure", "check_interval", c.check_interval.c_str());
    CUtil::ini_save_string("configure", "check_interval_night", c.check_interval_night.c_str());
    CUtil::ini_save_string("configure", "morning", c.morning.c_str());
    CUtil::ini_save_string("configure", "night", c.night.c_str());
    CUtil::ini_save_string("configure", "communicator_server", c.communicator_server.c_str());
    CUtil::ini_save_string("configure", "tower_name", c.tower_name.c_str());
}


status CUtil::GetStatus()
{
    status s;
    s.wifi = m_bWifi ? "1": "0"; //CUtil::ini_query_string("status", "wifi", "1");
    s._3G = m_b3G ? "1" : "0"; //CUtil::ini_query_string("status", "3G", "1");
    s.camera0 = m_bCamera0 ? "1" : "0"; //CUtil::ini_query_string("status", "camera0", "1");
    s.camera1 = m_bCamera1 ? "1" : "0"; //CUtil::ini_query_string("status", "camera1", "1");
    s.camera0_power = m_bCamera0Power ? "1" : "0"; //CUtil::ini_query_string("status", "camera0_power", "1");
    s.camera1_power = m_bCamera1Power ? "1" : "0";; //CUtil::ini_query_string("status", "camera1_power", "1");
    s.sendsms = CUtil::ini_query_string("status", "sms", "1");
    s.alarm = CUtil::ini_query_string("status", "alarm", "1");
    return s;
}

battery CUtil::GetBattery()
{
    return g_Battery;
}

void CUtil::SetBattery(battery b)
{
    g_Battery.solar_voltage = b.solar_voltage;
    g_Battery.battery_voltage = b.battery_voltage;
    g_Battery.charging_current = b.charging_current;
    g_Battery.discharging_current = b.discharging_current;
    g_Battery.illumination_intensity = b.illumination_intensity;
}

GPS CUtil::GetGPS()
{
    GPS g;
    g.latitude = CUtil::ini_query_string("GPS", "latitude", "0");
    g.longitude = CUtil::ini_query_string("GPS", "longitude", "0");
    return g;
}

void CUtil::SetGPS(GPS g)
{
    CUtil::ini_save_string("GPS", "latitude", g.latitude.c_str());
    CUtil::ini_save_string("GPS", "longitude", g.longitude.c_str());
}

bool CUtil::SavePhoneNumber(string phoneNumber)
{
    string numbers = ini_query_string("Global", "phoneNumbers", "");
    if(numbers.find(phoneNumber) != string::npos) return true;
    if(!numbers.empty())
        numbers.append(":");
    numbers.append(phoneNumber);
    return ini_save_string("Global", "phoneNumbers", numbers.c_str());
}

bool CUtil::RemovePhoneNumber(string phoneNumber)
{
    string numbers = ini_query_string("Global", "phoneNumbers", "");
    if(numbers.find(phoneNumber) == string::npos) return true;
    vector<string> vecSegTag;
    boost::split(vecSegTag, numbers, boost::is_any_of(":"));

    vector<string>::iterator it = find(vecSegTag.begin(), vecSegTag.end(), phoneNumber);
    if (it != vecSegTag.end())
        vecSegTag.erase(it);
    numbers.clear();
    for (it = vecSegTag.begin(); it != vecSegTag.end(); it++)
    {
        numbers += *it;
        numbers += ":";
    }
    numbers = numbers.substr(0, numbers.size() - 1);
    return ini_save_string("Global", "phoneNumbers", numbers.c_str());
}

vector<string> CUtil::GetPhoneNumber()
{
    string numbers = ini_query_string("Global", "phoneNumbers", "");
    vector<string> vecSegTag;
    boost::split(vecSegTag, numbers, boost::is_any_of(":"));
    return vecSegTag;
}

int CUtil::ini_query_int(const char* section, const char* key, const int notfound)
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini(CONFIG_FILE);
    return ini.geti(section, key, notfound);
}

bool CUtil::ini_save_int(const char* section, const char* key, const int value)
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini(CONFIG_FILE);
    return ini.put(section, key, value);
}

bool CUtil::ini_query_bool(const char* section, const char* key, const bool notfound)
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini(CONFIG_FILE);
    return ini.getbool(section, key, notfound);
}

bool CUtil::ini_save_bool(const char* section, const char* key, const bool value)
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini(CONFIG_FILE);
    return ini.put(section, key, value);
}

string CUtil::ini_query_string(const char* section, const char* key, char* notfound)
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini(CONFIG_FILE);
    return ini.gets(section, key, notfound);
}

bool CUtil::ini_save_string(const char* section, const char* key, const char* value)
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini(CONFIG_FILE);
    return ini.put(section, key, value);
}

float CUtil::ini_query_float(const char* section, const char* key, const float notfound)
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini(CONFIG_FILE);
    return ini.getf(section, key, notfound);
}

bool CUtil::ini_save_float(const char* section, const char* key, const float value)
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini(CONFIG_FILE);
    return ini.put(section, key, value);
}

void CUtil::BackupOrRestoreIni()
{
    string strServer = CUtil::ini_query_string("configure", 
        "communicator_server", "");
    boost::unique_lock<boost::mutex> lock(m);
    
    if(strServer == "")
        system("./initool restore"); 
    else
        system("./initool backup"); 
}

bool CUtil::GenerateDeviceID(string& deviceID)
{
    int sock_mac;
    struct ifreq ifr_mac;
    char mac_addr[30];

    deviceID.clear();

    sock_mac = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_mac == -1) return false;

    memset(&ifr_mac,0,sizeof(ifr_mac));
    strncpy(ifr_mac.ifr_name, "eth0", sizeof(ifr_mac.ifr_name)-1);

    if((ioctl(sock_mac, SIOCGIFHWADDR, &ifr_mac)) < 0) return false;

    sprintf(mac_addr,"%02x%02x%02x%02x%02x%02x",
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[0],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[2],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[3],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[5]);

    deviceID = mac_addr;

    close(sock_mac);
    return true;
}

string CUtil::GetIP()
{
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to "ppp0" */
    strncpy(ifr.ifr_name, "ppp0", IFNAMSIZ-1);

    int err = ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    if(err) return "";

    return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}

void CUtil::ChangeAPSSIDIfNeeded()
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini("/etc/hostapd/hostapd.conf");
    string ssid = ini.gets("", "ssid", "CHINANDY");
    string deviceID, suffix;
    GenerateDeviceID(deviceID);
    if(!boost::algorithm::starts_with(ssid, "CHINANDY") || ssid.length() != 13)
        ssid = "CHINANDY";
    suffix = boost::algorithm::to_upper_copy(deviceID.substr(8)); 
    if(!boost::algorithm::ends_with(ssid, suffix))
    {
        ssid += "_";
        ssid += suffix;
        ini.put("", "ssid", ssid);
        USER_PRINT("changed ssid to %s.\n", ssid.c_str());
        system("service hostapd restart");
    }
}

float CUtil::CalculateCarThreshold()
{
    float car_width = CUtil::ini_query_float("init", "alarm_threshold_width", 0);
    float car_height = CUtil::ini_query_float("init", "alarm_threshold_height", 0);
    float result = car_width*car_height;
    return result;
}

void CUtil::OpenAP()
{
    CUtil::SaveAPStatus(true);
//    return;

    USER_PRINT("Open AP ...\n");
    system("ifdown wlan0 > /dev/null 2>&1");
    system("ifup wlan0 > /dev/null 2>&1");
    system("ifconfig wlan0 192.168.11.1 > /dev/null 2>&1");
    system("service isc-dhcp-service restart > /dev/null 2>&1");
    system("service hostapd restart > /dev/null 2>&1");
    USER_PRINT("Opened AP\n");
}

void CUtil::CloseAP()
{
    CUtil::SaveAPStatus(false);
//    return;

    USER_PRINT("Close AP\n");
    system("ifdown wlan0 > /dev/null 2>&1");
    system("service hostapd stop > /dev/null 2>&1");
    USER_PRINT("Closed AP\n");
}

int CUtil::FindPidByName(char* pName)
{
    int szPid=-1;
    char szQuery[256];  
    sprintf(szQuery,"ps -ef|grep '%s'|grep -v 'grep'|awk '{print $2$8}'", pName);
    FILE *fp=popen(szQuery,"r");
    char szBuff[100]; 
  
    while(fgets(szBuff, 100 , fp)!=NULL) // 逐行读取执行结果  
    {
        string line = szBuff, pname = pName;
        if(boost::algorithm::ends_with(line.substr(0, line.size() -1), pname)){
            szPid=atoi(szBuff);
            break;
        }
    }  

    pclose(fp); // 关闭管道指针,不是fclose()很容易混淆  
    return szPid;  
}

int CUtil::Exec(char* path, char* arg, bool sync)
{
    char szCommand[1024] = {0};
    snprintf(szCommand, sizeof(szCommand), "%s %s %s", 
        path, arg, (sync ? "" : "&"));
    return system(szCommand);
}

bool CUtil::isFileExist(const char* filePath)
{
    if(filePath == NULL)
        return false;
    if(access(filePath, F_OK) == 0)
        return true;
    return false;
}

bool CUtil::CheckInternetConnection()
{
    int ret = Exec("ping", "-c 1 114.114.114.114 > /dev/null 2>&1", true);
    if(!ret) return false;

    return true;
}

static size_t OnWriteData(void* buffer, size_t size, size_t nmemb, void* lpVoid)
{
    std::string* str = dynamic_cast<std::string*>((std::string *)lpVoid);
    if( NULL == str || NULL == buffer )  return -1;

    char* pData = (char*)buffer;
    str->append(pData, size * nmemb);
    return nmemb;
}

bool CUtil::CheckProxyServer()
{
    string strResponse;
    string url = ini_query_string("proxy", "http", "");
    url += "/test";

    USER_PRINT("proxy url:%s\n", url.c_str());
    
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&strResponse);
    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    USER_PRINT("proxy test return:\n%s\n", strResponse.c_str());
    if (CURLE_OK == result)
    {
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(strResponse, root))
        {
            if (root.isMember("test"))
            {
                 return strcmp(root["test"].asCString(), "ok") == 0;
            }
        }
    }
    return false;
}

string CUtil::CheckVersion()
{
    boost::unique_lock<boost::mutex> lock(m);
    minIni ini("/root/Main/version.txt");
    string ssid = ini.gets("", "version", "1");
    return ssid;
}

int CUtil::GetMorningInt()
{
    return ini_query_int("configure", "morning", 6);
}

int CUtil::GetNightInt()
{
    return ini_query_int("configure", "night", 18);
}

bool CUtil::SetMorningInt(int morning)
{
    return ini_save_int("configure", "morning", morning);
}

bool CUtil::SetNightInt(int night)
{
    return ini_save_int("configure", "night", night);
}

void CUtil::SyncDateTime()
{
    system("ntpdate pool.ntp.org");
}
