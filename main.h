#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>  
#include <dirent.h>  
#include <sys/types.h> 
#include <algorithm>
#include <unistd.h>
#include <ctime>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <curl/curl.h>
#include <json/json.h>
#include <pthread.h>
#include "log.h"
#include "Util.h"
#include "powermgt.h"

using namespace std;

bool CheckAP();
bool Post(string Url, string data);

const char* RecvSMSTemplate[] =
{
    /* 增加接收报警信息手机 */
    "*#增加手机号码*#",

    /*删除接收报警信息手机 */
    "*#删除手机号码*#",

    /* 装置安装地点 */
    "*#地点名称*#",

    /* 如果不发送该指令，1分钟报警1次， */
    /* 总共报警3次，再停止报警；若发送该指令，则不再报警。 */
    "*#收到*#",

    /* 该命令用于判断该系统是否工作正常。若正常系统发送短信告知用户， */
    /* 若不正常，系统重启不发送短信。 */
    "*#工况*#",

    /* 配置模式下，系统检测模块停止工作，打开相机电源，1s读一次图像 */
    "*#系统模式*#配置",

    /* 进入演示模式，系统正常工作，同时开启WIFI，供客户端软件读取图像。 */
    "*#系统模式*#演示",

    /* 进入系统工作模式 */
    "*#系统模式*#工作",

    /* 开启WIFI模块 */
    "*#开启无线*#",

    /* 关闭WIFI模块 */
    "*#关闭无线*#",

    /* 强制报警 */
    "*#强制报警*#"
};

enum RestartReason
{
    WIFI_ERROR,
    _3G_ERROR,
    CAMERA0_ERROR,
    CAMERA1_ERROR,
    ReadSMS_ERROR,
    SMS_QUERY_STATUS_ERROR,
    CAMERA0_CAPTURE_ERROR,
    CAMERA1_CAPTURE_ERROR,
    CAMERA0_CHECK_ERROR,
    CAMERA1_CHECK_ERROR,
    DAILY_RESTART,
    SEND_PROXY_ERROR,
};

bool g_bForceExit = false;
bool g_bWorkThreadRunning = false;
pthread_mutex_t mutex;

void RestartSystem(enum RestartReason reason)
{
    USER_PRINT( "going to restart, reason [%d]...\n", reason );
    char buffer[50] = {0};
    sprintf(buffer, "echo \"[`date`] reason = %d\" >> /root/reason\n", reason);
    system(buffer);
    g_bForceExit = true;
    system("sync");
    //system("./restart &");
    SendRestartOSCmd(10000);
    exit(-1);
}

void ClearExpiredProxyInfo()
{
    CUtil::ini_save_string("proxy", "rtsp0", "");
    CUtil::ini_save_string("proxy", "rtsp1", "");
    CUtil::ini_save_string("proxy", "http", "");
    CUtil::ini_save_string("proxy", "ssh", "");
}

void SendProxyInfo()
{
    char buffer[100];
    int retry = 3;
    Json::Value root;
    root["device_id"] = CUtil::GetBoardID();
    root["tower_name"] = CUtil::GetLocation();
    root["http"] = CUtil::ini_query_string("proxy", "http", "");
    if(strcmp(root["http"].asCString(), "") == 0) return;

    root["rtsp0"] = CUtil::ini_query_string("proxy", "rtsp0", "");
    root["rtsp1"] = CUtil::ini_query_string("proxy", "rtsp1", "");
    root["ssh"] = CUtil::ini_query_string("proxy", "ssh", "");
    root["version"] = CUtil::CheckVersion();
    string url;
    url += "http://";
    url += CUtil::ini_query_string("configure", "communicator_server", "czyhdli.com");
    url += ":9093/updateinfo";
again:
    time_t start = time(NULL);
    bool bRet = Post(url, root.toStyledString());
    time_t end = time(NULL);
    sprintf(buffer, "echo \"[`date`] time(%ld), bRet(%d)\" >> /root/sendproxyinfo\n", end - start, bRet);
    system(buffer);
    if(!bRet)
    {
        if(retry == 0)
        {
            RestartSystem(SEND_PROXY_ERROR);
        }
        else
        {
            USER_PRINT("retry to sent proxy info to server ...\n");
            goto again;
        }
    }
    USER_PRINT("Sent proxy info to server ...\n");
}

void HeartBeatThread()
{
    int sec = CUtil::ini_query_int("Global", "HeartBeat", 50);
    int i = 0;
    while(!g_bForceExit)
    {
        USER_PRINT("Send heart beat ...\n");
        SendHeartBeatCmd(2*60);
        
        bool  status = CheckAP();
        USER_PRINT("AP status (%d)\n", status);
        
        SendProxyInfo();
        int temp = i++ < 40 ? 2 : sec;
        while(temp--)
        {
            sleep(1);
            if(g_bForceExit) break;
        }
    }
    USER_PRINT("=====>>>>>HeartBeat thread exit\n");
}

bool startWiths( const string & input, const string & match )
{
    return(input.size() >= match.size()
       && equal(match.begin(), match.end(), input.begin()));
}

bool isPhoneNumberValid(string phoneNumber)
{
    boost::regex pattern("^((\\+86)|(86))?(1)\\d{10}$");
    boost::cmatch what;
    bool isValid = boost::regex_search(phoneNumber.c_str(), what, pattern);
    return isValid;
}

bool SendSMS(string phoneNum, string text)
{
    //return true;
    
    char sms[1024];
    USER_PRINT("will send sms \"%s\" to [%s]\n", text.c_str(), phoneNum.c_str());
    snprintf(sms, 1024, "echo \"%s\" | gnokii --sendsms %s > /dev/null 2>&1", 
        text.c_str(), phoneNum.c_str());
	pthread_mutex_lock(&mutex);
    int ret = system(sms);
	pthread_mutex_unlock(&mutex);
    return ret == 0;
}

bool SendAllSMS(string text)
{                
    vector<string> vPhoneNumbers = CUtil::GetPhoneNumber();
    USER_PRINT("start to send sms to [%d] people ...\n", vPhoneNumbers.size());
    for(unsigned int i = 0; i < vPhoneNumbers.size(); i++)
    {
        bool ret = SendSMS(vPhoneNumbers.at(i),  CUtil::GetLocation() + " " +text);
        if(ret)
        {
            USER_PRINT("send sms success.\n");
        }
        else 
        {
            USER_PRINT("send sms failed.\n");
        }
    }
}

bool ReadSMS(vector<string> & vSMS)
{
    USER_PRINT("enter ReadSMS\n");
    vSMS.clear();
    string line, strNumber, strText;
    int lineCount = 0, retry = 3;
again: 
	pthread_mutex_lock(&mutex);
    int ret = system("gnokii --getsms SM 0 end -F sms -d > /dev/null 2>&1");
	pthread_mutex_unlock(&mutex);
    if(!(ret == 1024 || ret == 4096))
    {
        if(retry--)
        {
            USER_PRINT("retry to getsms ...\n");
            sleep(2);
            goto again;
        }
        USER_PRINT("gnokii getsms failed, ret = %d\n", ret);
        return false;
    }

    ifstream infile("sms");
    while(getline(infile, line))
    {
        if(++lineCount == 12)
        {
            lineCount = 0;
            vSMS.push_back(strNumber+"@"+strText);
        }

        if(startWiths(line, "From:"))
        {
            strNumber = line.substr(6);
            reverse(strNumber.begin(), strNumber.end());
            strNumber = strNumber.substr(strNumber.find_first_of('@')+1);
            reverse(strNumber.begin(), strNumber.end());
        }

        if(lineCount == 10)
            strText = line;
    }
    infile.close();
    remove("sms");

    return true;
}

bool CheckAP()
{
    // check the wifi
    int ret = system("ifconfig | grep wlan0 > /dev/null 2>&1");
    if(ret) goto fail;
    ret = system("ps aux | grep dhcpd | grep -v grep > /dev/null 2>&1");
    if(ret) goto fail;
    ret = system("ps aux | grep hostapd | grep -v grep > /dev/null 2>&1");
    if(ret) goto fail;
    if(!CUtil::IsAPEnabled())
        CUtil::SaveAPStatus(true);
    return true;

fail:
    if(CUtil::IsAPEnabled())
        CUtil::SaveAPStatus(false);
    return false;
}

bool Check3G()
{
    // check the 3g
    int nRetry = 3;
    static time_t start = 0, now;
    int ret = system("ifconfig | grep ppp0 > /dev/null 2>&1");

    while(true)
    {
        ret = system("ping -c 1 baidu.com -I ppp0 > /dev/null 2>&1");
        if(ret == 0) break;
        if(nRetry-- == 0)
            goto fail;
        USER_PRINT("Retry to check the 3G connection\n");
    }
    start = 0;

    if(!CUtil::Is3GEnabled())
        CUtil::Save3GStatus(true);
    return true;
fail:
    if(start == 0)
        start = time(NULL);
    now = time(NULL);
    if(now - start > 600 && !ret)
        RestartSystem(_3G_ERROR);

    USER_PRINT("3g connection lost.\n");
    CUtil::Save3GStatus(false);
    return false;
}

bool CheckCamera0(string path)
{
    struct stat st;
    USER_PRINT("check camera 0, jpg path = %s\n", path.c_str());
    if (stat(path.c_str(), &st) == 0)
    {
        if(st.st_size > 4*1024)
        {
            if(!CUtil::IsCameraEnabled(0))
                CUtil::SaveCameraStatus(0, true);
            return true;
        }
        else
        {
            USER_PRINT("check camera 0, jpg file size = %ldKB\n", st.st_size/1024);
        }
    }
    else
    {
        USER_PRINT("check camera 0, jpg file stat error = %s\n", strerror(errno));
    }
    CUtil::SaveCameraStatus(0, false);
    USER_PRINT("CheckCamera0 failed\n");

    return false;
}

bool CheckCamera1()
{
    USER_PRINT("check camera 1\n");
    if (system("ping -c 1 192.168.1.60 -I eth0 > /dev/null 2>&1") == 0)
    {
        if(!CUtil::IsCameraEnabled(1))
            CUtil::SaveCameraStatus(1, true);
        return true;
    }
    CUtil::SaveCameraStatus(1, false);
    USER_PRINT("CheckCamera1 failed\n");

    return false;
}

bool UploadPicture(string path)
{
    string filename = path.substr(path.find_last_of('/')+1);
    string date = filename.substr(0, filename.find('-'));
    string ftp_server = CUtil::ini_query_string("configure", "communicator_server", "czyhdli.com");
    string ftp_port = CUtil::ini_query_string("configure", "ftp_port", "22");
    string ftp_user = CUtil::ini_query_string("configure", "ftp_user", "admin");
    string ftp_password = CUtil::ini_query_string("configure", "ftp_password", "yog123$");
    string account = ftp_user+":"+ftp_password;
    string url = "ftp://";
    url += ftp_server;
    url += ":";
    url += ftp_port;
    url += "/";
    url += date.substr(0, date.size() - 2);
    url += "/";
    url += filename;

    USER_PRINT("start to upload picture ...\n");
    USER_PRINT("ftp url = %s\n", url.c_str())


    FILE* pSendFile = ::fopen(path.c_str(), "rb");
    if(pSendFile == NULL)
    {
        USER_PRINT("fopen file %s failed.\n", path.c_str());
        return false;
    }
    fseek(pSendFile, 0L, SEEK_END);
    size_t iFileSize = ::ftell(pSendFile);
    fseek(pSendFile, 0L, SEEK_SET);

    CURL* curl = ::curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_USERPWD, account.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_READDATA, pSendFile);
    curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, iFileSize);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);

    CURLcode result = ::curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(pSendFile);

    if ( CURLE_OK == result ) {
        return true; //Succeeded
    }
    return false;  //Failed
}

static size_t OnWriteData(void* buffer, size_t size, size_t nmemb, void* lpVoid)  
{  
    std::string* str = dynamic_cast<std::string*>((std::string *)lpVoid);  
    if( NULL == str || NULL == buffer )  return -1;
  
    char* pData = (char*)buffer;  
    str->append(pData, size * nmemb);  
    return nmemb;  
}

bool Post(string Url, string data)
{
    string strResponse;
    string url = Url;
    string postData = data;

    CURL* curl = ::curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);  
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteData);  
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&strResponse);

    CURLcode result = ::curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if ( CURLE_OK == result )
        return true;
    else
        return false;
}
string GetShortUrl(string& longUrl)
{
    string strResponse;
    string url = "http://dwz.cn/create.php";
    string shortUrl = longUrl;
    string postData = "url=";
    postData += longUrl;

    CURL* curl = ::curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);  
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteData);  
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&strResponse);

    CURLcode result = ::curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if ( CURLE_OK == result ) {
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(strResponse, root))
        {
            if (root.isMember("status") && root["status"].asInt() == 0)
            {
                shortUrl = root["tinyurl"].asCString();
            }
        }
     }
     return shortUrl;
}

bool SendAlarmInfo(string picPath, string& imgUrl)
{
    string strResponse;
    string http_server = CUtil::ini_query_string("configure", "communicator_server", "czyhdli.com");
    string http_port = CUtil::ini_query_string("configure", "http_port", "8085");
    string filename = picPath.substr(picPath.find_last_of('/')+1);
    string date = filename.substr(0, filename.find('-') - 2);

    string url = "http://";
    url += http_server;
    url += ":";
    url += http_port;
    url += "/abs-platform/outer/receiveMonitorNotice?deviceNo=";
    url += CUtil::GetBoardID();
    url += "&pictureName=";
    url += picPath;
    url += "&pictureUrl=";
    url += date+"/"+filename;
    url += "&latitude=";
    url += CUtil::ini_query_string("gps", "latitude", "0");
    url += "&longitude=";
    url += CUtil::ini_query_string("gps", "longitude", "0");

    USER_PRINT("start to send alarm ...\n");
    USER_PRINT("http url = %s\n", url.c_str());

    CURL* curl = ::curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);  
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteData);  
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&strResponse);

    CURLcode result = ::curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if ( CURLE_OK == result ) {
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(strResponse, root))
        {
            USER_PRINT("alarm response: \n%s\n", root.toStyledString().c_str());
            if (root.isMember("result_code") && strcmp(root["result_code"].asCString(), "1") == 0)
            {
                if (root.isMember("result_content") && root["result_content"].isMember("imgUrl"))
                {
                    imgUrl = root["result_content"]["imgUrl"].asCString();
                    imgUrl = GetShortUrl(imgUrl);
                    USER_PRINT("remote img url = %s\n", imgUrl.c_str());
                    return true;
                }
            }
        }
        return false;
    }
    return false;  //Failed
}

int remove_pic_in_dir(const char *dir_name)
{
    struct stat st;
    struct dirent *dir;
    char fullpath[1024];
    DIR *dirp = opendir(dir_name);
    if(!dirp) {
        perror("opendir");
        return -1;
    }
    while((dir = readdir(dirp)) != NULL) {
        if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
            continue;
        }
        sprintf(fullpath, "%s/%s", dir_name, dir->d_name); //获取全局路径
        if(lstat(fullpath, &st) < 0) {
            perror("lstat");
            continue;
        }
        if(S_ISDIR(st.st_mode)) continue;
        if(boost::algorithm::ends_with(fullpath, ".jpg") && !boost::algorithm::starts_with(fullpath, "camera"))
            remove(fullpath);
    }
    closedir(dirp);
    return 0;
}

#endif // MAIN_H_INCLUDED
