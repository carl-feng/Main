#include <iostream>
#include "Util.h"
#include "log.h"

using namespace std;

#define PPPD_NAME "pppd"
#define PPPD_PATH "pppd"
#define PPPD_ARG "call wcdma"

#define MAIN_NAME "Main"
#define MAIN_PATH "/root/Main/Main"
#define MAIN_ARG ""

#define RTSP0_SRV_NAME "testOnDemandRTSPServer"
#define RTSP0_SRV_PATH "/root/Main/testOnDemandRTSPServer"
#define RTSP0_SRV_ARG ""

#define RTSP1_SRV_NAME "live555ProxyServer"
#define RTSP1_SRV_PATH "live555ProxyServer"
#define RTSP1_SRV_ARG "rtsp://192.168.1.60:554/1/h264minor"

#define CAPTURE_SERVICE_NAME "captureService"
#define CAPTURE_SERVICE_PATH "/root/Main/captureService"
#define CAPTURE_SERVICE_ARG ""

extern bool delete_expired_pic();

int main()
{
    static int sleepTime = 5;
    while(true)
    {
        // 2. check the captureService
        int pid = CUtil::FindPidByName(CAPTURE_SERVICE_NAME);
        if(pid < 0) {
            USER_PRINT("start %s\n", CAPTURE_SERVICE_NAME);
            CUtil::Exec(CAPTURE_SERVICE_PATH, CAPTURE_SERVICE_ARG, false);
            sleep(2);
        }
        
        // 3. check the pppd
        pid = CUtil::FindPidByName(PPPD_NAME);
        if(pid < 0) {
            USER_PRINT("start %s\n", PPPD_NAME);
            CUtil::Exec(PPPD_PATH, PPPD_ARG, false);
        }

        // 4. check the Main
        pid = CUtil::FindPidByName(MAIN_NAME);
        if(pid < 0) {
            USER_PRINT("start %s\n", MAIN_NAME);
            CUtil::Exec(MAIN_PATH, MAIN_ARG, false);
            sleep(10); // wait 10s, so ngrok can report it's url to Main
        }
        
        // 5. check the testOnDemandRTSPServer
        pid = CUtil::FindPidByName(RTSP0_SRV_NAME);
        if(pid < 0) {
            USER_PRINT("start %s\n", RTSP0_SRV_NAME);
            CUtil::Exec(RTSP0_SRV_PATH, RTSP0_SRV_ARG, false);
            sleep(1);
        }
        
        // 6. check the live555ProxyServer
        pid = CUtil::FindPidByName(RTSP1_SRV_NAME);
        if(pid < 0) {
            USER_PRINT("start %s\n", RTSP1_SRV_NAME);
            CUtil::Exec(RTSP1_SRV_PATH, RTSP1_SRV_ARG, false);
        }

        // 9. check the config file "config.ini"
        CUtil::BackupOrRestoreIni();

        // 10. delete expired pic in tf card
        delete_expired_pic();

        // 11. sleep 60s
        USER_PRINT("sleep %ds in watchdog process\n", sleepTime);
        sleep(sleepTime);
        if(sleepTime < 600)
            sleepTime += 30;
    }
}
