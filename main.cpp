#include "main.h"
#include <sys/file.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <jsoncpp/json/json.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/thread.hpp>
#include "opencv2/opencv.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/contrib/contrib.hpp"
#include "powermgt.h"

using namespace cv;

bool g_ForceAlarm = false;
bool g_bInitModel = false;
int g_Count = 0;

string curPath;

extern bool backup_pic(string path);
bool detect_2(const char* image_filename, bool bFirst);
extern void ReadandTrain(Mat img);
extern bool TargetDetection(Mat img, int Pixel_Threshold, bool update_bg_model);

void CameraPower(int index, bool enable)
{
    static boost::uuids::uuid uuid_1 = boost::uuids::random_generator()();
    if(enable)
        _open(boost::lexical_cast<std::string>(uuid_1), index, true);
    else
        _close(boost::lexical_cast<std::string>(uuid_1), index);
}

bool CapturePic(int index, string& jpgPath)
{
    time_t timep;
    time(&timep);
    struct tm *pTM = localtime(&timep);
    char buffer[50];
    snprintf(buffer, 50, "%04d%02d%02d-%02d%02d%02d.jpg", pTM->tm_year+1900, 
        pTM->tm_mon+1, pTM->tm_mday, pTM->tm_hour, pTM->tm_min, pTM->tm_sec);
    char cmd[256] = {0};
    jpgPath = curPath;
    jpgPath += "/";
    jpgPath += buffer;
    if(index == 0)
        snprintf(cmd, 256, "./capturejpg %s > /dev/null 2>&1", jpgPath.c_str());
    else
    {
        snprintf(cmd, 256, "./rtsp2jpg rtsp://192.168.1.60:554/1/h264minor %s \
            > /dev/null 2>&1", "/root/Main/camera1.jpg");
        jpgPath = "/root/Main/camera1.jpg";
    }
    int ret = system(cmd);
    
    USER_PRINT("capture jpg from cam%d, path = %s\n", index, jpgPath.c_str());
    return ret == 0;
}

void SaveLineThresholdThread()
{
    string path;
    CapturePic(1, path);
    detect_2(path.c_str(), true);
}

void WorkThread()
{
    int nRiskCount = 0;
    int nSendCount = 0;

    while(!g_bForceExit)
    {
        g_bWorkThreadRunning = true;
        // 1. check if needed to alarm risk
        bool Enable = CUtil::GetAlarmStatus();
        if(!Enable)
        {
            USER_PRINT("sleep until allowing to check risk ...\n");
            sleep(1);
            continue;
        }
        USER_PRINT(">>>>>>>>>start to check the risk on camera0 ...\n");

        // 2. power on camera0
        CameraPower(0, true);
again:

        // 3. take pictures
        USER_PRINT("start to capture picture ...\n");
        string jpgPath;
        int nRetry = 3;
        while(true)
        {
            bool ret = CapturePic(0, jpgPath);
            if(ret) break;
            if(nRetry-- == 0)
                RestartSystem(CAMERA0_CAPTURE_ERROR);
            USER_PRINT("Retry to capture picture...\n");
        }

        USER_PRINT("capture picture success\n");
        
        // 4. check the picture
        bool bRet = CheckCamera0(jpgPath);
        if(!bRet) RestartSystem(CAMERA0_CHECK_ERROR);
        USER_PRINT("camera works well\n");
        
        int left = CUtil::ini_query_int("init", "left", 0);
        int up = CUtil::ini_query_int("init", "up", 0);
        int right = CUtil::ini_query_int("init", "right", 0);
        int down = CUtil::ini_query_int("init", "down", 0);
        Mat testImage = imread(jpgPath.c_str());
        if(!g_bInitModel)
        {
            g_Count++;
            ReadandTrain(testImage(Rect(left, up, right - left, down - up)));
            if(g_Count > CUtil::ini_query_int("global", "train_frame", 200))
            {
                USER_PRINT("finished model training.**********************\n");
                g_bInitModel = true;
                g_Count = 0;
            }
            
            goto again;
        }
 
        // 5. power off camera0
        CameraPower(0, false);

        
        // 6. analyze the picuture
        int threshold = (int)CUtil::CalculateCarThreshold();
        bool bRisk = false;
        if(!g_ForceAlarm)
        {
            USER_PRINT("going to check the image...\n");
            bRisk = TargetDetection(testImage(Rect(left, up, right - left, down - up)), threshold, false);
            USER_PRINT(">>>>>> bRisk = %d, threshold = %d\n", bRisk, threshold); 
        }
        // 7. upload picture if needed
        USER_PRINT("g_ForceAlarm = %d\n", g_ForceAlarm); 
        if(bRisk || g_ForceAlarm)
        {
            USER_PRINT("detected risk.\n");
            backup_pic(jpgPath);
            nRiskCount++;
        }
        else
        {
            USER_PRINT("detected no risk.\n");
            nRiskCount = 0;
            nSendCount = 0;
        }

        int risk_count = CUtil::GetRiskCount();
        int alarm_count = CUtil::GetAlarmCount();
        USER_PRINT("nRiskCount = %d/%d, nSendCount = %d/%d\n", nRiskCount, 
            risk_count, nSendCount, alarm_count); 
        if(nRiskCount != 0 && (nRiskCount%risk_count == 0) && (nSendCount != alarm_count))
        {
            nRiskCount = 0;
            USER_PRINT("detected risk %d tims persistently, start to alarm\n", 
                risk_count);
            char buffer[100] = {0};
            snprintf(buffer, 100, "./addDate2Image %s", jpgPath.c_str());
            system(buffer);
            string imgUrl;
            bool ret = UploadPicture(jpgPath);
            if(ret)
            {
                USER_PRINT("upload picture success\n");
                g_ForceAlarm = false;
                if(SendAlarmInfo(jpgPath, imgUrl))
                {
                    USER_PRINT("sent alarm success\n");
                }
                else
                {
                    USER_PRINT("sent alarm failed\n");
                }
            }
            else
            {
                USER_PRINT("upload picture failed\n");
            }
            if(CUtil::GetSendSMSStatus())
            {
                vector<string> vPhoneNumbers = CUtil::GetPhoneNumber();
                USER_PRINT("start to send sms to [%d] people ...\n", 
                    vPhoneNumbers.size());
                for(unsigned int i = 0; i < vPhoneNumbers.size(); i++)
                {
                    ret = SendSMS(vPhoneNumbers.at(i),  CUtil::GetLocation()
                        + " 有危险车辆驻留!!! " + imgUrl);
                    if(ret)
                        {USER_PRINT("send sms success.\n");} 
                    else
                        {USER_PRINT("send sms failed.\n");}
                }
                if(++nSendCount == alarm_count)
                {
                    USER_PRINT("sent sms %d times persistently, don't send when still risk next cycle.\n", alarm_count);
                }
            }
        }

        // 8. remove the pitcure
        remove_pic_in_dir(".");

        // 9. sleep if needed
        if(CUtil::GetAlarmStatus())
        {
            int sleepSec;
            time_t now;
            time(&now);
            struct tm *pTM = localtime(&now);
            int morning = CUtil::GetMorningInt();
            int night = CUtil::GetNightInt();
            USER_PRINT("morning = %d, night = %d.\n", morning, night);
            if(pTM->tm_hour > morning && pTM->tm_hour < night)
                sleepSec = CUtil::GetCheckInterval();
            else
                sleepSec = CUtil::GetCheckInterval_Night();
            if(sleepSec > 0)
            {
                USER_PRINT("Sleep %d seconds in WorkThread.\n", sleepSec);
                while(!g_bForceExit && !g_ForceAlarm && g_bInitModel &&
			            sleepSec-- && CUtil::GetAlarmStatus())
                    sleep(1);
            }
        }
    }
    g_bWorkThreadRunning = false;
    USER_PRINT("=====>>>>>work thread1 exit.\n");
}

bool detect_2(const char* image_filename, bool bFirst)
{
    IplImage *frame,*gray,*sobel; 
    double temp,sum_temp;
    frame=cvLoadImage(image_filename);
    gray=cvCreateImage(cvGetSize(frame),frame->depth,1);
    sobel=cvCreateImage(cvGetSize(frame),IPL_DEPTH_16S,1);  
    
    IplImage* currentFrame;
    IplImage* previousFrame;
 
    cvCvtColor(frame,gray,CV_BGR2GRAY);
    cvSobel(gray,sobel,1,0,3);  
    cvSmooth(sobel,sobel,CV_GAUSSIAN,3,3);
    IplImage *sobel8u=cvCreateImage(cvGetSize(sobel),IPL_DEPTH_8U,1);  
    cvConvertScaleAbs(sobel,sobel8u,1,0);
    for(int i=0;i<sobel8u->height;i++)
       for(int j=0;j<sobel8u->width;j++)
       {
           temp=cvGet2D(sobel8u,i,j).val[0];
           if(temp>150)
               sum_temp++;
       }
    if(bFirst)
    {
        CUtil::ini_save_float("init", "line_threshold", sum_temp);
        return false;
    }
    else
    {
        double line_threshold = CUtil::ini_query_float("init", "line_threshold", 0);
        double line_threshold_delta = CUtil::ini_query_float("init", "line_threshold_delta", 0);
        if(sum_temp > line_threshold + line_threshold_delta)
            return true;
    }
    return false;
}

void WorkThread_2()
{
    while(!g_bForceExit)
    {
        // 1. check if time at 6:00 or 18:00
        time_t now = time(NULL);
        struct tm *pTM = localtime(&now);
        if(!((pTM->tm_hour == 6 || pTM->tm_hour == 18) && (pTM->tm_min == 0)))
        {
            USER_PRINT("sleep until 6:00 or 18:00 ...\n");
            int timeout = 30;
            while(timeout--)
            {
                sleep(1);
                if(g_bForceExit) break;
            }
            continue;
        }
        
        // 2. check if needed to alarm risk
        bool Enable = CUtil::GetAlarmStatus();
        if(!Enable)
        {
            USER_PRINT("sleep until allowing to check risk ...\n");
            sleep(1);
            continue;
        }
        USER_PRINT(">>>>>>>>>start to check the risk on camera1 ...\n");

        // 3. power on camera1
        CameraPower(1, true);

        // 4. take pictures
        USER_PRINT("start to capture picture ...\n");
        string jpgPath;
        int nRetry = 3;
        while(true)
        {
            bool ret = CapturePic(1, jpgPath);
            if(ret) break;
            if(nRetry-- == 0)
                RestartSystem(CAMERA1_CAPTURE_ERROR);
            USER_PRINT("Retry to capture picture...\n");
        }

        USER_PRINT("capture picture success\n");
 
        // 5. power off camera1
        CameraPower(1, false);

        // 6. check the picture
        bool bRet = CheckCamera1();
        if(!bRet) RestartSystem(CAMERA1_CHECK_ERROR);
        USER_PRINT("camera1 works well\n");
        
        // 7. analyze the picuture
        bool bRisk = false;
        bRisk = detect_2(jpgPath.c_str(), false);
        // 8. upload picture if needed
        if(!bRisk)
        {
            USER_PRINT("detected no risk on camera1.\n");
        }
        else
        {
            USER_PRINT("detected risk on camera1.\n");
            bool ret = UploadPicture(jpgPath);
            if(ret)
            {
                string imgUrl;
                USER_PRINT("upload picture success\n");
                if(SendAlarmInfo(jpgPath, imgUrl))
                {
                    USER_PRINT("sent alarm success\n");
                    if(CUtil::GetSendSMSStatus())
                    {
                        vector<string> vPhoneNumbers = CUtil::GetPhoneNumber();
                        USER_PRINT("start to send sms to [%d] people ...\n",
                            vPhoneNumbers.size());
                        for(unsigned int i = 0; i < vPhoneNumbers.size(); i++)
                        {
                            ret = SendSMS(vPhoneNumbers.at(i),  CUtil::GetLocation() + 
                                " 线上有异物!!! " + imgUrl);
                            if(ret) {USER_PRINT("send sms success.\n");} 
                            else {USER_PRINT("send sms failed.\n");}
                        }
                    }
                }
                else
                {
                    USER_PRINT("sent alarm failed\n");
                }
            }
            else
            {
                USER_PRINT("upload picture failed\n");
            }
        }

        // 9. remove the pitcure
        remove(jpgPath.c_str());
    }
    USER_PRINT("=====>>>>>work thread2 exit.\n");
}

int get_lock(void)
{
    int fdlock;
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if((fdlock = open("/tmp/carlfeng.lock", O_WRONLY|O_CREAT, 0666)) == -1)
        return 0;
    if(fcntl(fdlock, F_SETLK, &fl) == -1)
        return 0;
    return 1;
}

void my_func(int sign_no)
{
    if(sign_no == SIGINT)
        USER_PRINT("Capture SIGINT.\n");
    if(sign_no == SIGQUIT)
        USER_PRINT("Capture SIGQUIT.\n");

    USER_PRINT("=====>>>>>Start to exit...\n");
    g_bForceExit = true;
}

extern void server_thread();
extern void expire_check_thread();

int main( int argc, char* argv[] )
{
    signal(SIGINT,my_func);
    signal(SIGQUIT,my_func);

    pthread_mutex_init(&mutex, NULL);
    
    if(!get_lock())
    {
        USER_PRINT("another instance(Main) is running, exit.\n");
        exit(0);
    }
    
    ClearExpiredProxyInfo();

    char buf[1024] = { 0 };
    int n = readlink("/proc/self/exe" , buf , sizeof(buf));
    if( n > 0 && n < sizeof(buf))
    {
        USER_PRINT("current binary path = %s\n" , buf);
        curPath = buf;
        curPath = curPath.substr(0, curPath.find_last_of('/'));
        USER_PRINT("chdir path = %s\n" , curPath.c_str());
        chdir(curPath.c_str());
    }
    
    string deviceID;
    CUtil::GenerateDeviceID(deviceID);
    string board_id = CUtil::GetBoardID();
    USER_PRINT("board_id = %s\n", deviceID.c_str());
    if(board_id != deviceID)
    {
        CUtil::SetBoardID((char*)deviceID.c_str());
    }
    
    // start config http server firstly
    boost::thread chkthrd(&expire_check_thread);
    boost::thread srvthrd(&server_thread);
    boost::thread heartthrd(&HeartBeatThread);

    CUtil::ChangeAPSSIDIfNeeded(); 
    
    // wait for 3G connection ready
    while(!Check3G())
    {
        USER_PRINT("waiting for 3G connection ready ...\n");
        sleep(5);
    };
    USER_PRINT("3G connection is ready\n");
   
    CUtil::SyncDateTime();
 
    boost::thread workthrd(&WorkThread);
    boost::thread workthrd2(&WorkThread_2);
    
    while(!g_bForceExit)
    {
        if(!Check3G()) USER_PRINT("3G connection lost ...\n");
        
        SendSolarStatusCmd();
        double alarm_voltage = CUtil::GetBattery().battery_voltage;
        USER_PRINT("solar battery voltage = %f.\n", alarm_voltage);
        
        time_t now = time(NULL);
        struct tm *pTM = localtime(&now);
        static int hour = 0;
        
        if(pTM->tm_hour != hour)
        {
            if(alarm_voltage < CUtil::ini_query_float("init", "alarm_voltage", -1))
            {
                // report to watchdog board, and shutdown main board to charge battery.
                
            }
        }
        hour = pTM->tm_hour;

        /* sleep 120s */
        int sec = 120;
        while(sec--)
        {
            sleep(1);
            if(g_bForceExit) break;

           // restart board at about 6:00am
           time_t timep;
           time(&timep);
           struct tm *pTM = localtime(&timep);
           if(pTM->tm_hour == 6 && pTM->tm_min < 2)
           {
               sleep(2*60);
               RestartSystem(DAILY_RESTART);
           }
        }
    }

    USER_PRINT("wait for other threads exiting.\n");
    
    chkthrd.join();
    srvthrd.join();
    heartthrd.join();
    workthrd.join();
    workthrd2.join();
    
    USER_PRINT("=====>>>>>main thread exit.\n");

    return(0);
}
