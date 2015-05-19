#include <iostream>
#include "Util.h"
#include "log.h"

using namespace std;
#define NGROK_NAME "ngrok"
#define NGROK_PATH "/root/Main/ngrok"
#define NGROK_ARG "-log=stdout start ssh rtsp0 rtsp1 > /dev/null 2>&1"

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

void CreateNgrokConfigFile()
{
    char szCmd[1024] = {0};
    string data;
    data = "server_addr: ";
    data += CUtil::ini_query_string("configure", "communicator_server",
        "czyhdli.com");
    data += ":4443\n";
    data += "trust_host_root_cert: false\n";
    
    data += "tunnels:\n\n";
    data += "  ssh:\n";
    data += "    proto:\n";
    data += "      tcp: 22\n\n";
    
    data += "  rtsp0:\n";
    data += "    proto:\n";
    data += "      tcp: 8554\n\n";
    
    data += "  rtsp1:\n";
    data += "    proto:\n";
    data += "      tcp: 554\n\n";

    snprintf(szCmd, sizeof(szCmd), "echo \'%s\' > /root/.ngrok", data.c_str());
    CUtil::Exec(szCmd, "", true);
}

int main()
{
    // 1. create .ngrok
    CreateNgrokConfigFile();

    static int sleepTime = 30;
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

        // 7. test proxy url
        if(CUtil::Is3GEnabled())
        {
            USER_PRINT("going to check the proxy server.\n");
            bool bConnected = false;
            int nRetry = 3;
            while(nRetry--) 
            {
                if(CUtil::CheckProxyServer())
                {
                    bConnected = true;
                    break;
                }
            }
            if(!bConnected)
            {
                USER_PRINT("proxy server disconnected, stop %s\n", NGROK_NAME);
                CUtil::Exec("killall ngrok", "", true);
                sleep(1);
            }
        }
        else
            sleepTime = 30;
            

        // 8. check the ngrok
        pid = CUtil::FindPidByName(NGROK_NAME);
        if(pid < 0) {
            USER_PRINT("start %s\n", NGROK_NAME);
            CUtil::Exec(NGROK_PATH, NGROK_ARG, false);
            char szBuf[100] = {0};
            snprintf(szBuf, sizeof(szBuf), "%s -log=stdout -proto http 8080 \
                > /dev/null 2>&1", NGROK_PATH);
            CUtil::Exec(szBuf, "", false);
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
