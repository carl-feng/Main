#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <ctime>
#include <list>
#include <vector>
#include <algorithm>
#include <boost/format.hpp>     
#include <boost/tokenizer.hpp>     
#include <boost/algorithm/string.hpp>
#include <boost/thread/shared_mutex.hpp> 
#include <boost/thread.hpp>
#include "rs232.h"
#include "Util.h"
#include "log.h"

using namespace std;

typedef uint8_t     BYTE;
typedef uint16_t    WORD;
typedef uint32_t    DWORD;
typedef uint32_t    ULONG_PTR;
typedef ULONG_PTR   DWORD_PTR;

#define LOBYTE(w)           ((BYTE)(WORD)(w))
#define HIBYTE(w)           ((BYTE)((WORD)(w) >> 8))
#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | \
                            ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))

#define NUM                 100
int n = 0, i, cport_nr = 0, bdrate = 4800;
unsigned char buf[NUM];
char mode[] = {'8','N','1',0};

#define HEADER_1            0xEB
#define HEADER_2            0x90
#define CAM_POWER_ON_CMD    0x55
#define CAM_POWER_OFF_CMD   0x5A
#define RESTART_OS_CMD      0x5C
#define HEART_BEAT_CMD      0x5E
#define SOLAR_STATUS_CMD    0x60

#define TIMEOUT             100 //100ms
#define msleep(x)           usleep(x*1000)

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

bool SendHeartBeatCmd(uint16_t seconds)
{
    int i, n = 0;
    bool bCheck = false;
    unsigned char szBuf[NUM] = {0};
	pthread_mutex_lock(&mutex);
	if(RS232_OpenComport(cport_nr, bdrate, mode))
	{
		USER_PRINT("Can not open comport\n");
		return false;
	}

    for(i = 0; i < 5; i++)
    {
        n += RS232_PollComport(cport_nr, szBuf + n, NUM - n);
        msleep(TIMEOUT);
    }
    USER_PRINT("clear %d chars\n", n);
    n = 0;
    
    szBuf[0] = HEADER_1;
    szBuf[1] = HEADER_2;
    szBuf[2] = HEART_BEAT_CMD;
    szBuf[3] = 2;
    szBuf[4] = LOBYTE(seconds);
    szBuf[5] = HIBYTE(seconds);
    szBuf[6] = ((szBuf[2] + szBuf[3] + szBuf[4] + szBuf[5]) & 0xFF);
    RS232_SendBuf(cport_nr, szBuf, 7);

    for(int i = 0; i < 7; i++)
        printf("%x ", szBuf[i]);
    printf("\n");
    
    for(i = 0; i < 5; i++)
    {
        msleep(TIMEOUT);
		n += RS232_PollComport(cport_nr, szBuf + n, NUM - n);
    }
	RS232_CloseComport(cport_nr);
	pthread_mutex_unlock(&mutex);
    
    for(int i = 0; i < n; i++)
        printf("%x ", szBuf[i]);
    printf("\n");
    USER_PRINT("Receive bytes n = %d\n", n);
    
    if(n != 5)
        bCheck = false;
    else
        bCheck = (szBuf[4] == (szBuf[2] & 0xFF));
    USER_PRINT("SendHeartBeatCmd bCheck = %d\n", bCheck);
    return bCheck;
}

bool SendRestartOSCmd(uint16_t delay)
{
    int i, n = 0;
    bool bCheck = false;
    unsigned char szBuf[NUM] = {0};
	pthread_mutex_lock(&mutex);
	if(RS232_OpenComport(cport_nr, bdrate, mode))
	{
		USER_PRINT("Can not open comport\n");
		return false;
	}

    for(i = 0; i < 5; i++)
    {
        n += RS232_PollComport(cport_nr, szBuf + n, NUM - n);
        msleep(TIMEOUT);
    }
    USER_PRINT("clear %d chars\n", n);
    n = 0;

    delay = delay/10;
    
    szBuf[0] = HEADER_1;
    szBuf[1] = HEADER_2;
    szBuf[2] = RESTART_OS_CMD;
    szBuf[3] = 3;
    szBuf[4] = LOBYTE(delay);
    szBuf[5] = HIBYTE(delay);
    szBuf[6] = 1;
    szBuf[7] = ((szBuf[2] + szBuf[3] + szBuf[4] + szBuf[5] + szBuf[6]) & 0xFF);
	RS232_SendBuf(cport_nr, szBuf, 8);

    for(int i = 0; i < 8; i++)
        printf("%x ", szBuf[i]);
    printf("\n");

    for(i = 0; i < 5; i++)
    {
        msleep(TIMEOUT);
		n += RS232_PollComport(cport_nr, szBuf + n, NUM - n);
    }
	RS232_CloseComport(cport_nr);
	pthread_mutex_unlock(&mutex);
    
    for(int i = 0; i < n; i++)
        printf("%x ", szBuf[i]);
    printf("\n");
    USER_PRINT("Receive bytes n = %d\n", n);
    
    if(n != 5)
        bCheck = false;
    else
        bCheck = (szBuf[4] == (szBuf[2] & 0xFF));
    USER_PRINT("SendRestartOSCmd bCheck = %d\n", bCheck);
    return bCheck;
}

bool SendCameraPowerCmd(uint8_t cam_index, uint16_t delay, bool enable)
{
    int i, n = 0;
    bool bCheck = false;
    unsigned char szBuf[NUM] = {0};
	pthread_mutex_lock(&mutex);
	if(RS232_OpenComport(cport_nr, bdrate, mode))
	{
		USER_PRINT("Can not open comport\n");
		return false;
	}

    for(i = 0; i < 5; i++)
    {
        n += RS232_PollComport(cport_nr, szBuf + n, NUM - n);
        msleep(TIMEOUT);
    }
    USER_PRINT("clear %d chars\n", n);
    n = 0;
    
    delay /= 10;

    szBuf[0] = HEADER_1;
    szBuf[1] = HEADER_2;
    szBuf[2] = enable ? CAM_POWER_ON_CMD : CAM_POWER_OFF_CMD;
    szBuf[3] = 4;
    szBuf[4] = cam_index + 1;
    szBuf[5] = LOBYTE(delay);
    szBuf[6] = HIBYTE(delay);
    szBuf[7] = 1;
    szBuf[8] = ((szBuf[2] + szBuf[3] + szBuf[4] + szBuf[5] + szBuf[6] + szBuf[7]) & 0xFF);
    
    for(int i = 0; i < 9; i++)
        printf("%x ", szBuf[i]);
    printf("\n");
	
    RS232_SendBuf(cport_nr, szBuf, 9);

    for(i = 0; i < 5; i++)
    {
        msleep(TIMEOUT);
		n += RS232_PollComport(cport_nr, szBuf + n, NUM - n);
    }
	RS232_CloseComport(cport_nr);
	pthread_mutex_unlock(&mutex);
    
    for(int i = 0; i < n; i++)
        printf("%x ", szBuf[i]);
    printf("\n");
    USER_PRINT("Receive bytes n = %d\n", n);
    
    if(n != 5)
        bCheck = false;
    else
        bCheck = (szBuf[4] == (szBuf[2] & 0xFF));
    USER_PRINT("SendCameraPowerCmd bCheck = %d\n", bCheck);
    return bCheck;
}

bool SendSolarStatusCmd()
{
    int i, n = 0;
    bool bCheck = false;
    unsigned char szBuf[NUM] = {0};
	pthread_mutex_lock(&mutex);
	if(RS232_OpenComport(cport_nr, bdrate, mode))
	{
		USER_PRINT("Can not open comport\n");
		return false;
	}

    for(i = 0; i < 5; i++)
    {
        n += RS232_PollComport(cport_nr, szBuf + n, NUM - n);
        msleep(TIMEOUT);
    }
    USER_PRINT("clear %d chars\n", n);
    n = 0;
    
    szBuf[0] = HEADER_1;
    szBuf[1] = HEADER_2;
    szBuf[2] = SOLAR_STATUS_CMD;
    szBuf[3] = 0;
    szBuf[4] = ((szBuf[2] + szBuf[3]) & 0xFF);
	RS232_SendBuf(cport_nr, szBuf, 5);

    for(int i = 0; i < 5; i++)
        printf("%x ", szBuf[i]);
    printf("\n");	
    
    for(i = 0; i < 5; i++)
    {
        msleep(TIMEOUT);
		n += RS232_PollComport(cport_nr, szBuf + n, NUM - n);
    }
	RS232_CloseComport(cport_nr);
	pthread_mutex_unlock(&mutex);
    
    for(int i = 0; i < n; i++)
        printf("%x ", szBuf[i]);
    printf("\n");	
    USER_PRINT("Receive bytes n = %d\n", n);
    
    if(n != 15)
        bCheck = false;
    else
    {
        bCheck = (szBuf[14] == ((szBuf[2] + szBuf[3] + 
            szBuf[4] + szBuf[5] + szBuf[6] + szBuf[7] + szBuf[8] +
            szBuf[9] + szBuf[10] + szBuf[11] + szBuf[12] + szBuf[13]) & 0xFF));
        if(bCheck)
        {
            battery b;
            b.solar_voltage = MAKEWORD(szBuf[4], szBuf[5])/100.0;
            b.battery_voltage = MAKEWORD(szBuf[6], szBuf[7])/100.0;
            b.charging_current = MAKEWORD(szBuf[8], szBuf[9])/1000.0;
            b.discharging_current = MAKEWORD(szBuf[10], szBuf[11])/1000.0;
            b.illumination_intensity = MAKEWORD(szBuf[12], szBuf[13]);
            CUtil::SetBattery(b);
            bCheck = true;
        }
    }    
    USER_PRINT("SendSolarStatusCmd bCheck = %d\n", bCheck);
    return bCheck;
}

class Item
{
public:
    time_t start;
    string uuid;

    bool operator==(const Item &rhs) const
    {
        return uuid == rhs.uuid;
    }
};

list<Item> cam0_list;
list<Item> cam1_list;

boost::mutex _mutex;

void wait_thread(int index)
{
    int sec;
    if(index == 0) sec = CUtil::ini_query_int("Global", "wait_time_cam0", 10);
    if(index == 1) sec = CUtil::ini_query_int("Global", "wait_time_cam1", 120);
    USER_PRINT("wait thread: sleep %ds\n", sec);
    sleep(sec);
    if(index == 0)
    {
        CUtil::SetCameraPowerStatus(index, true);
        USER_PRINT("wait thread: cam0_poweron = %d\n", CUtil::GetCameraPowerStatus(index));
    }
    else if (index == 1)
    {
        CUtil::SetCameraPowerStatus(index, true);
        USER_PRINT("wait thread: cam1_poweron = %d\n", CUtil::GetCameraPowerStatus(index));
    }
}

void _open(string uuid, int index, bool sync)
{
    Item item;
    item.start = time(NULL);
    item.uuid = uuid;

    boost::unique_lock<boost::mutex> lock(_mutex);

    list<Item>::iterator it;
    list<Item>* tmp_list = NULL;
    if(index == 0)
    {
        tmp_list = &cam0_list;
    }
    else if(index == 1)
    {
        tmp_list = &cam1_list;
    }
    if(!tmp_list)
        return;

    it = find(tmp_list->begin(), tmp_list->end(), item);
    if (it != tmp_list->end())
    {
    	USER_PRINT("update time in cam%d_list for item = %s\n", index,
            it->uuid.c_str());
    	it->start = time(NULL);
    }
    else
    {
    	USER_PRINT("add item in cam%d_list, uuid = %s\n", index,
            item.uuid.c_str());
        tmp_list->push_back(item);
    }
    lock.unlock();

    if(!CUtil::GetCameraPowerStatus(index))
    {
        // do open
        SendCameraPowerCmd(index, 0, true);
        if(sync)
        {
            int sec;
            if(index == 0) sec = CUtil::ini_query_int("Global", "wait_time_cam0", 10);
            if(index == 1) sec = CUtil::ini_query_int("Global", "wait_time_cam1", 120);
            USER_PRINT("open: sync to wait for cam%d ready, sleep %ds\n", index, sec);
            sleep(sec);
            CUtil::SetCameraPowerStatus(index, true);
            USER_PRINT("open: cam%d_poweron = %d\n", index, CUtil::GetCameraPowerStatus(index));
        }
        else
        {
            boost::thread thrd(&wait_thread, index);
        }
    }
}

void _close(string uuid, int index)
{
    Item item;
    item.uuid = uuid;

    boost::unique_lock<boost::mutex> lock(_mutex);
    
    list<Item>* tmp_list = NULL;
    if(index == 0)
    {
        tmp_list = &cam0_list;
    }
    else if(index == 1)
    {
        tmp_list = &cam1_list;
    }
    if(!tmp_list)
        return;
    list<Item>::iterator it = find(tmp_list->begin(), tmp_list->end(), item);
    if (it != tmp_list->end())
    {
        USER_PRINT("force to del item in cam%d_list, uuid = %s\n", index, 
            it->uuid.c_str());
        tmp_list->erase(it);
    }
    USER_PRINT("close: cam%d_list length = %d, cam%d_poweron = %d\n", 
        index, tmp_list->size(), index, CUtil::GetCameraPowerStatus(index));
}

extern bool g_bForceExit;

void expire_check_thread()
{
    while(true)
    {
        boost::unique_lock<boost::mutex> lock(_mutex);
        
        list<Item>::iterator it;
        time_t now = time(NULL);
        time_t remain = CUtil::ini_query_int("Global", "poweron_time_cam", 10*60); //default 10mins
        for(it = cam0_list.begin(); it != cam0_list.end();)
        {
            if((now - it->start) > remain)
            {
                USER_PRINT("item expired in cam0_list, uuid = %s\n", it->uuid.c_str());
                it = cam0_list.erase(it);
            }
            else
            {
                it++;
            }
        }

        now = time(NULL);
        for(it = cam1_list.begin(); it != cam1_list.end();)
        {
            if((now - it->start) > remain)
            {
                USER_PRINT("item expired in cam1_list, uuid = %s\n", it->uuid.c_str());
                it = cam1_list.erase(it);
            }
            else
            {
                it++;
            }
        }

        if(cam0_list.size() == 0)
        {
            if(CUtil::GetCameraPowerStatus(0))
            {
                // do close
                SendCameraPowerCmd(0, 0, false);
                CUtil::SetCameraPowerStatus(0, false);
                USER_PRINT("check thread: cam0 closed.\n");
            }
        }

        if(cam1_list.size() == 0)
        {
            if(CUtil::GetCameraPowerStatus(1))
            {
                // do close
                SendCameraPowerCmd(1, 0, false);
                CUtil::SetCameraPowerStatus(1, false);
                USER_PRINT("check thread: cam1 closed.\n");
            }
        }

        lock.unlock();
        if(g_bForceExit)
            break;
        sleep(2);
    }
    USER_PRINT("=====>>>>>expire check thread exit.\n");
}
