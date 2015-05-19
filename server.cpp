#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include "mongoose.h"
#include <json/json.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <map>
#include <boost/thread.hpp>
#include "Util.h"
#include "log.h"
#include "powermgt.h"

using namespace std;

extern bool g_bForceExit;
extern void SaveLineThresholdThread();
#define ERROR_CODE -1
#define SUCCESS_CODE 0

static int process_req(struct mg_connection *conn) {
    Json::Value out;

    //printf("url = %s, content = %s\n", conn->uri, conn->content);
    if (strcmp(conn->uri, "/test") == 0) {
        out["test"] = "ok";
    } else if (strcmp(conn->uri, "/connect") == 0) {
        Json::Value message;
        zone z =  CUtil::GetMonitorZone();
        message["info"]["init"]["zone"]["up"] = z.up;
        message["info"]["init"]["zone"]["down"] = z.down;
        message["info"]["init"]["zone"]["left"] = z.left;
        message["info"]["init"]["zone"]["right"] = z.right;
        message["info"]["init"]["alarm_threshold"]["width"] = z.alarm_threshold_width;
        message["info"]["init"]["alarm_threshold"]["height"] = z.alarm_threshold_height;
        message["info"]["init"]["alarm_voltage"] = CUtil::ini_query_float("init", "alarm_voltage", -1);

        configure c = CUtil::GetConfigure();
        message["info"]["configure"]["communicator_server"] = c.communicator_server;
        message["info"]["configure"]["tower_name"] = c.tower_name;
        message["info"]["configure"]["risk_count"] = c.risk_count;
        message["info"]["configure"]["check_interval"] = c.check_interval;
        message["info"]["configure"]["check_interval_night"] = c.check_interval_night;

        status s = CUtil::GetStatus();
        message["info"]["state"]["wifi"] = s.wifi;
        message["info"]["state"]["3G"] = s._3G;
        message["info"]["state"]["camera0"] = s.camera0;
        message["info"]["state"]["camera1"] = s.camera1;
        message["info"]["state"]["sms"] = s.sendsms;
        message["info"]["state"]["camera0_power"] = s.camera0_power;
        message["info"]["state"]["camera1_power"] = s.camera1_power;
        message["info"]["state"]["alarm"] = s.alarm;

        battery b = CUtil::GetBattery();
        
        message["info"]["state"]["battery"]["solar_voltage"] = b.solar_voltage;
        message["info"]["state"]["battery"]["battery_voltage"] = b.battery_voltage;
        message["info"]["state"]["battery"]["charging_current"] = b.charging_current;
        message["info"]["state"]["battery"]["discharing_current"] = b.discharging_current;
        message["info"]["state"]["battery"]["llumination_intensity"] = b.illumination_intensity;

        GPS g = CUtil::GetGPS();
        message["info"]["GPS"]["latitude"] = g.latitude;
        message["info"]["GPS"]["longitude"] = g.longitude;

        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "connect success.";
        out["message"] = message;
    }
    else if(strcmp(conn->uri, "/get_tower_name") == 0) { 
        Json::Value message;
        message["tower_name"] = CUtil::GetLocation();
        message["device_id"] = CUtil::GetBoardID();
        message["rtsp0"] = "rtsp://192.168.11.1:8554/h264ESVideoTest";
        message["rtsp1"] = "rtsp://192.168.11.1:554/proxyStream";
        out["message"] = message ;
        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "set success.";
    }
    else if(strcmp(conn->uri, "/set_tower_name") == 0) { 
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body))
        {
            if (body.isMember("tower_name"))
            {
                string tower_name = body["tower_name"].asCString();
                CUtil::SaveLocation(tower_name);
                out["error_code"] = SUCCESS_CODE;
                out["error_message"] = "set success.";
                out["message"] = body;
            }
            else {
                out["error_code"] = ERROR_CODE;
                out["error_message"] = "json format invalid.";
                out["message"] = message;
            }
        }
        else {
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = message;
        }
    }
    else if(strcmp(conn->uri, "/reportgps") == 0) { 
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body))
        {
            if (body.isMember("longitude") && body.isMember("latitude"))
            {
                GPS gps;
                gps.longitude = body["longitude"].asCString();
                gps.latitude = body["latitude"].asCString();
                CUtil::SetGPS(gps);
                out["error_code"] = SUCCESS_CODE;
                out["error_message"] = "set success.";
                out["message"] = body;
            }
            else {
                out["error_code"] = ERROR_CODE;
                out["error_message"] = "json format invalid.";
                out["message"] = message;
            }
        }
        else {
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = message;
        }
    }
    else if (strcmp(conn->uri, "/initconf") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body))
        {
            if (body.isMember("zone") &&
                body["zone"].isMember("up") && body["zone"]["up"].isString() &&
                body["zone"].isMember("down") && body["zone"]["down"].isString() &&
                body["zone"].isMember("left") && body["zone"]["left"].isString() &&
                body["zone"].isMember("right") && body["zone"]["right"].isString())
            {
                zone z;
                z.up = body["zone"]["up"].asCString();
                z.down = body["zone"]["down"].asCString();
                z.left = body["zone"]["left"].asCString();
                z.right = body["zone"]["right"].asCString();
                z.alarm_threshold_width = body["alarm_threshold"]["width"].asCString();
                z.alarm_threshold_height = body["alarm_threshold"]["height"].asCString();
                CUtil::SetMonitorZone(z);
                CUtil::CreateMaskBmp();
                out["error_code"] = SUCCESS_CODE;
                out["error_message"] = "initconf success.";
                out["message"] = body;
            }
            else {
                out["error_code"] = ERROR_CODE;
                out["error_message"] = "json format invalid.";
                out["message"] = message;
            }
        }
        else {
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = message;
        }
    }
    else if (strcmp(conn->uri, "/algorithm_threshold") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body) && body.isMember("ground_threshold") && body.isMember("line_threshold"))
        {
            double threshold1 = body["ground_threshold"].asDouble();
            double threshold2 = body["line_threshold"].asDouble();
            CUtil::ini_save_float("init", "ground_threshold", threshold1);
            CUtil::ini_save_float("init", "line_threshold_delta", threshold2);
            out["error_code"] = SUCCESS_CODE;
            out["error_message"] = "set threshold success.";
            out["message"]["ground_threshold"] = threshold1;
            out["message"]["line_threshold"] = threshold2;
        }else{
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = "";
        }
    }
    else if (strcmp(conn->uri, "/alarm_voltage") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body) && body.isMember("alarm_voltage"))
        {
            double voltage = body["alarm_voltage"].asDouble();
            CUtil::ini_save_float("init", "alarm_voltage", voltage);
            out["error_code"] = SUCCESS_CODE;
            out["error_message"] = "set alarm voltage success.";
            out["message"]["alarm_voltage"] = CUtil::ini_query_float("init", "alarm_voltage", -1);
        }else{
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = "";
        }
    }
    else if (strcmp(conn->uri, "/paramconf") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body))
        {
            if (body.isMember("type") && string("common").compare(body["type"].asCString()) == 0 &&
                body.isMember("data") &&
                body["data"].isMember("communicator_server") && body["data"]["communicator_server"].isString() &&
                body["data"].isMember("risk_count") && body["data"]["risk_count"].isString() &&
                body["data"].isMember("check_interval") && body["data"]["check_interval"].isString() &&
                body["data"].isMember("check_interval_night") && body["data"]["check_interval_night"].isString() &&
                body["data"].isMember("tower_name") && body["data"]["tower_name"].isString())
            {
                configure c;
                c.tower_name = body["data"]["tower_name"].asCString();
                c.risk_count = body["data"]["risk_count"].asCString();
                c.check_interval = body["data"]["check_interval"].asCString();
                c.check_interval_night = body["data"]["check_interval_night"].asCString();
                c.communicator_server = body["data"]["communicator_server"].asCString();
                CUtil::SetConfigure(c);
                out["error_code"] = SUCCESS_CODE;
                out["error_message"] = "paramconf success.";
                out["message"] = body;
            }
            else {
                out["error_code"] = ERROR_CODE;
                out["error_message"] = "json format invalid.";
                out["message"] = message;
            }
        }
        else {
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = message;
        }
    }
    else if (strcmp(conn->uri, "/query") == 0) {
        Json::Value message;
        status s = CUtil::GetStatus();
        message["wifi"] = s.wifi;
        message["3G"] = s._3G;
        message["camera0"] = s.camera0;
        message["camera1"] = s.camera1;
        message["camera0_power"] = s.camera0_power;
        message["camera1_power"] = s.camera1_power;
        message["sms"] = s.sendsms;
        message["alarm"] = s.alarm;

        GPS g = CUtil::GetGPS();
        message["GPS"]["latitude"] = g.latitude;
        message["GPS"]["longitude"] = g.longitude;

        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "query success";
        out["message"] = message;
    }
    else if (strcmp(conn->uri, "/restart") == 0) {
        g_bForceExit = true;
        system("./restart &");
        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "restart success";
        out["message"] = "";
    }
    else if (strcmp(conn->uri, "/wifi_ap") == 0) {
        bool enable = false;
        Json::Value message;
        char buffer[10];
        mg_get_var(conn, "enable", buffer, sizeof(buffer));
        strcmp(buffer, "true") == 0 ? enable = true : enable = false;
        if(enable) CUtil::OpenAP();
        else CUtil::CloseAP();
        message["enable"] = CUtil::IsAPEnabled() ? "1" : "0";
        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "wifi ap config success";
        out["message"] = message;
    }
    else if (strcmp(conn->uri, "/sms") == 0) {
        bool enable = false;
        Json::Value message;
        char buffer[10];
        mg_get_var(conn, "enable", buffer, sizeof(buffer));
        strcmp(buffer, "true") == 0 ? enable = true : enable = false;
        CUtil::SetSendSMSStatus(enable);
        message["enable"] = CUtil::GetSendSMSStatus() ? "1" : "0";
        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "sms config success";
        out["message"] = message;
    }
    else if (strcmp(conn->uri, "/camera_power") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body) && body.isMember("uuid") && 
            body.isMember("index") && body.isMember("poweron"))
        {
            int index = body["index"].asInt();
            string uuid = body["uuid"].asCString();
            bool poweron = body["poweron"].asBool();
            if(poweron)
                _open(uuid, index, false);
            else
                _close(uuid, index);
            out["error_code"] = SUCCESS_CODE;
            out["error_message"] = "success";
            out["message"] = message;
        }else{
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = "";
        }
    }
    else if (strcmp(conn->uri, "/camera_power_query") == 0) {
        Json::Value message;
        status s = CUtil::GetStatus();
        message["camera0_power"] = s.camera0_power;
        message["camera1_power"] = s.camera1_power;
        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "success";
        out["message"] = message;
    }
    else if (strcmp(conn->uri, "/alarm") == 0) {
        bool enable = false;
        Json::Value message;
        char buffer[10] = {0};
        mg_get_var(conn, "enable", buffer, sizeof(buffer));
        strcmp(buffer, "true") == 0 ? enable = true : enable = false;
        CUtil::SetAlarmStatus(enable);

        message["enable"] = CUtil::GetAlarmStatus() ? "1" : "0";
        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "alarm config success";
        out["message"] = message;
    }
    else if (strcmp(conn->uri, "/force_alarm") == 0) {
        extern bool g_ForceAlarm;
        Json::Value message;
        g_ForceAlarm = true;
        USER_PRINT(">> force alarm later.\n");
        message["enable"] = g_ForceAlarm ? "1" : "0";
        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "force alarm config success";
        out["message"] = message;
    }
    else if (strcmp(conn->uri, "/phonenumber") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body) && body.isMember("type"))
        {
            if(strcmp(body["type"].asCString(), "add") == 0)
            {
                Json::Value val_array = body["list"];
                for(int i = 0; i < val_array.size(); i++)
                    CUtil::SavePhoneNumber(val_array[i].asCString());
            }
            else if(strcmp(body["type"].asCString(), "remove") == 0)
            {
                Json::Value val_array = body["list"];
                for(int i = 0; i < val_array.size(); i++)
                    CUtil::RemovePhoneNumber(val_array[i].asCString());
            }
            vector<string> v = CUtil::GetPhoneNumber();
            for(int i = 0; i < v.size(); i++)
                message["list"].append(v.at(i));
            out["error_code"] = SUCCESS_CODE;
            out["error_message"] = "success";
            out["message"] = message;
        }else{
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = "";
        }
    }
    else if (strcmp(conn->uri, "/alarmcount") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body) && body.isMember("alarm_count"))
        {
            int alarm_count = body["alarm_count"].asInt();
            CUtil::SetAlarmCount(alarm_count);
            out["error_code"] = SUCCESS_CODE;
            out["error_message"] = "set alarm time success.";
            out["message"]["alarm_count"] = alarm_count;
        }else{
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = "";
        }
    }
    else if (strcmp(conn->uri, "/riskcount") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body) && body.isMember("risk_count"))
        {
            int alarm_count = body["risk_count"].asInt();
            CUtil::SetRiskCount(alarm_count);
            out["error_code"] = SUCCESS_CODE;
            out["error_message"] = "set risk time success.";
            out["message"]["risk_count"] = alarm_count;
        }else{
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = "";
        }
    }
    else if (strcmp(conn->uri, "/checkinterval") == 0) {
        Json::Value message, body;
        Json::Reader reader;
        if (reader.parse(conn->content, body) && body.isMember("check_interval") &&
            body.isMember("check_interval_night"))
        {
            int interval = body["check_interval"].asInt();
            int interval_night = body["check_interval_night"].asInt();
            CUtil::SetCheckInterval(interval);
            CUtil::SetCheckInterval_Night(interval_night);
            out["error_code"] = SUCCESS_CODE;
            out["error_message"] = "set risk check interval success.";
            out["message"]["interval"] = interval;
            out["message"]["interval_night"] = interval_night;
        }else{
            out["error_code"] = ERROR_CODE;
            out["error_message"] = "parse json failed.";
            out["message"] = "";
        }
    }
    else if (strcmp(conn->uri, "/PTZ") == 0) {
        out["error_code"] = SUCCESS_CODE;
        out["error_message"] = "success.";
        Json::Value message;
        string cmd;
        char buffer[20] = {0};
        char speed[10] = {0};
        mg_get_var(conn, "action", buffer, sizeof(buffer));
        mg_get_var(conn, "speed", speed, sizeof(speed));
        cmd = "./ptz ";
        cmd += buffer;
        cmd += " ";
        cmd += speed;
        USER_PRINT("ptz cmd = %s\n", cmd.c_str());
        message["action"] = buffer;
        message["speed"] = speed;
        out["message"] = message;
        system(cmd.c_str());
        if(strcmp("set_preset", buffer) == 0)
            boost::thread thd(&SaveLineThresholdThread);
    }
    else if (strcmp(conn->uri, "/takephoto") == 0) {
        char buffer[10] = {0};
        char* cmd = NULL;
        mg_get_var(conn, "camera", buffer, sizeof(buffer));
        if(strcmp(buffer, "0") == 0)
        {
            cmd = "./capturejpg camera0.jpg > /dev/null 2>&1";
            system(cmd);
            mg_send_file(conn, "camera0.jpg", NULL);
            remove("camera0.jpg");
            return MG_MORE;
        }
        else if(strcmp(buffer, "1") == 0)
        {
            cmd = "./rtsp2jpg rtsp://192.168.1.60:554/1/h264minor camera1.jpg > /dev/null 2>&1";
            system(cmd);
            mg_send_file(conn, "camera1.jpg", NULL);
            return MG_MORE;
        }
        else
        {
            return MG_FALSE;
        }
    }
    else if(strcmp(conn->uri, "/updateProxyUrl") == 0) {
        char buffer[1024];
	    Json::Value ret;
	    std::string strRet, strLocalAddr, strLocalAddrPort, strPublickUrl, strPublickUrlPort, strProtocol;
	    mg_get_var(conn, "local_addr", buffer, sizeof(buffer));
	    strLocalAddr = buffer;
	    strLocalAddrPort = strLocalAddr.substr(strLocalAddr.find_last_of(':')+1);

	    mg_get_var(conn, "public_url", buffer, sizeof(buffer));
	    strPublickUrl = buffer;
	    strPublickUrlPort = strPublickUrl.substr(strPublickUrl.find_last_of(':')+1);

	    mg_get_var(conn, "protocol", buffer, sizeof(buffer));
	    strProtocol = buffer;

	    if (strProtocol == "tcp" && strLocalAddrPort == "22")
            CUtil::ini_save_string("proxy", "ssh", strPublickUrl.c_str());
	    if (strProtocol == "http" && strLocalAddrPort == "8080") 
            CUtil::ini_save_string("proxy", "http", strPublickUrl.c_str());

	    if (strProtocol == "tcp" && strLocalAddrPort == "8554")
	    {
	    	std::string rtsp = strPublickUrl;
		    rtsp.replace(0, 3, "rtsp");
	    	rtsp += "/h264ESVideoTest";
    		CUtil::ini_save_string("proxy", "rtsp0", rtsp.c_str());
	    }
	    if (strProtocol == "tcp" && strLocalAddrPort == "554")
	    {
		    std::string rtsp = strPublickUrl;
		    rtsp.replace(0, 3, "rtsp");
		    rtsp += "/proxyStream";
		    CUtil::ini_save_string("proxy", "rtsp1", rtsp.c_str());
	    }

    	ret["local_addr"] = strLocalAddr;
	    ret["public_url"] = strPublickUrl;
	    ret["protocol"] = strProtocol;

	    strRet = ret.toStyledString();
    	USER_PRINT(">>>>>>>>>>>>>>>>>>>>updateProxyUrl = \n%s\n", strRet.c_str());
	    mg_send_header(conn, "Content-Type", "application/json;charset=utf-8");
	    mg_printf_data(conn, strRet.c_str(), strlen(strRet.c_str()));
        return MG_TRUE;
    }
    else {
        USER_PRINT("invalid url request, url = %s\n", conn->uri);
        out["error_code"] = ERROR_CODE;
        out["error_message"] = "invalid http request.";
    }

    mg_send_header(conn, "Content-Type", "application/json;charset=utf-8");
    mg_send_data(conn, out.toStyledString().c_str(), strlen(out.toStyledString().c_str()));
    return MG_TRUE;
}

static int ev_handler(struct mg_connection *conn, enum mg_event ev) {
    if (ev == MG_REQUEST) {
        return process_req(conn);
    }
    else if (ev == MG_AUTH) {
        return MG_TRUE;
    }
    else {
        return MG_FALSE;
    }
}

extern bool g_bForceExit;

void server_thread()
{
    struct mg_server *server = mg_create_server(NULL, ev_handler);
    mg_set_option(server, "document_root", ".");
    mg_set_option(server, "listening_port", "8080");  // Open port 8080

    USER_PRINT("Starting server on port %s\n", mg_get_option(server, "listening_port"));

    for (;;) {
        mg_poll_server(server, 1000);   // Infinite loop, Ctrl-C to stop
        if(g_bForceExit)
            break;
    }
    mg_destroy_server(&server);
    USER_PRINT("=====>>>>>server thread exit.\n");
}
