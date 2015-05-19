/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2015, Live Networks, Inc.  All rights reserved
// A test program that demonstrates how to stream - via unicast RTP
// - various kinds of file on demand, using a built-in RTSP server.
// main program

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/shm.h>
extern "C" {
#include <x264.h>
}
#include <zmq.h>
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#define TRUE    1
#define FALSE   0

UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName, char const* inputFileName); // fwd

#define FRAME_PER_SEC 20.0
#define WIDTH   720
#define HEIGHT  576

x264_param_t param;
x264_picture_t pic;
x264_picture_t pic_out;
x264_t *h;
int i_frame = 0;
int i_frame_size;
x264_nal_t *nal;
int i_nal;

#define FUNCTION_PRINT fprintf(stdout, "[%d] function %s .... calling\n", __LINE__, __func__);  


void convert_yuv422_to_yuv420(char *InBuff, char *OutBuff, int width,
int height)
{
    int i = 0, j = 0, k = 0;
    int UOffset = width * height;
    int VOffset = (width * height) * 5/4;
    int UVSize = (width * height) / 4;
    int line1 = 0, line2 = 0;
    int m = 0, n = 0;
    int y = 0, u = 0, v = 0;

    u = UOffset;
    v = VOffset;

    for (i = 0, j = 1; i < height; i += 2, j += 2)
    {
        /* Input Buffer Pointer Indexes */
        line1 = i * width * 2;
        line2 = j * width * 2;

        /* Output Buffer Pointer Indexes */
        m = width * y;
        y = y + 1;
        n = width * y;
        y = y + 1;

        /* Scan two lines at a time */
        for (k = 0; k < width*2; k += 4)
        {
            unsigned char Y1, Y2, U, V;
            unsigned char Y3, Y4, U2, V2;

            /* Read Input Buffer */
            Y1 = InBuff[line1++];
            U  = InBuff[line1++];
            Y2 = InBuff[line1++];
            V  = InBuff[line1++];

            Y3 = InBuff[line2++];
            U2 = InBuff[line2++];
            Y4 = InBuff[line2++];
            V2 = InBuff[line2++];

            /* Write Output Buffer */
            OutBuff[m++] = Y1;
            OutBuff[m++] = Y2;

            OutBuff[n++] = Y3;
            OutBuff[n++] = Y4;

            OutBuff[u++] = (U + U2)/2;
            OutBuff[v++] = (V + V2)/2;
        }
    }
}

class CameraFrameSource : public FramedSource
{
    int m_started;
    void *checkTask;
    void * pCtx;
    void * pSock;
    void *shm;//分配的共享内存的原始首地址
    void *shared;//指向shm
    int shmid;//共享内存标识符
  
public:  
    CameraFrameSource (UsageEnvironment &env)  
        : FramedSource(env)
    {  
        FUNCTION_PRINT
        m_started = 0;
        checkTask = NULL;
        

    pCtx = NULL;
    pSock = NULL;
    //使用tcp协议进行通信，需要连接的目标机器IP地址为192.168.1.2
    //通信使用的网络端口 为7766 
    const char * pAddr = "tcp://localhost:7766";

    //创建context 
    if((pCtx = zmq_ctx_new()) == NULL)
    {
        exit(-1);
    }
    //创建socket 
    if((pSock = zmq_socket(pCtx, ZMQ_DEALER)) == NULL)
    {
        zmq_ctx_destroy(pCtx);
        exit(-1);
    }
    int iSndTimeout = 5000;// millsecond
    //设置接收超时 
    if(zmq_setsockopt(pSock, ZMQ_RCVTIMEO, &iSndTimeout, sizeof(iSndTimeout)) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        exit(-1);
    }
    //连接目标 localhost，端口7766 
    if(zmq_connect(pSock, pAddr) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        exit(-1);
    }

    {
	//创建共享内存
	shmid = shmget((key_t)123456, 720*576*3, 0666|IPC_CREAT);
	if(shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}
	//将共享内存连接到当前进程的地址空间
	shm = shmat(shmid, 0, 0);
	if(shm == (void*)-1)
	{
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}
	printf("\nMemory attached at %X\n", (int)shm);
   
	//设置共享内存
	shared = (void*)shm;
   } 
        /* Get default params for preset/tuning */
        if(x264_param_default_preset(&param, "veryfast", "zerolatency") < 0)
        {
            printf("x264_param_default_preset failed\n");
            exit(-1);
        }

        /* Configure non-default params */
        param.i_csp = X264_CSP_I420;
        //param.i_threads  = X264_SYNC_LOOKAHEAD_AUTO;
        param.i_width  = WIDTH;
        param.i_height = HEIGHT;
        //param.b_vfr_input = 0;
        param.b_repeat_headers = 1;
        //param.b_annexb = 1;
        
        param.rc.f_rf_constant = 30;
        //param.rc.f_rf_constant_max = 45;
        param.rc.i_rc_method = X264_RC_CRF;//参数i_rc_method表示码率控制，CQP(恒定质量)，CRF(恒定码率)，ABR(平均码率)
        //param.rc.i_vbv_max_bitrate=100*1.2 ; // 平均码率模式下，最大瞬时码率，默认0(与-B设置相同)
        //param.rc.i_bitrate = 100; 

        //param.i_log_level  = X264_LOG_DEBUG;  

        //* muxing parameters
        //param.i_frame_total = 0; //* 编码总帧数.不知道用0.
        param.i_keyint_max = 10;
        param.i_fps_den  = 1; //* 帧率分母
        param.i_fps_num  = 5;//* 帧率分子
        param.i_timebase_den = param.i_fps_num; 
        param.i_timebase_num = param.i_fps_den;
        

        /* Apply profile baseline. */
        if(x264_param_apply_profile(&param, "baseline") < 0)
        {
            printf("x264_param_apply_profile failed\n");
            exit(-1);
        }

        if(x264_picture_alloc( &pic, param.i_csp, param.i_width, param.i_height ) < 0)
        {
            printf("x264_picture_alloc failed\n");
            exit(-1);
        }
        
        h = x264_encoder_open(&param);
        if(!h)
        {
            printf("x264_encoder_open failed\n");
            exit(-1);
        }
    }  
  
    ~CameraFrameSource ()  
    {  
        FUNCTION_PRINT
        if (m_started) {
            envir().taskScheduler().unscheduleDelayedTask(checkTask);  
        }
        x264_encoder_close( h );
        x264_picture_clean( &pic );
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
	if(shmdt(shm) == -1)
	{
		fprintf(stderr, "shmdt failed\n");
	}
    }  
  
protected:  
    virtual void doGetNextFrame ()  
    {
//        FUNCTION_PRINT
        if (m_started) return;  
        m_started = 1;  
  
        double delay = 1000.0 / FRAME_PER_SEC;  
        int to_delay = delay * 1000;    // us  
  
        checkTask = envir().taskScheduler().scheduleDelayedTask(to_delay,  
                getNextFrame, this);
    }
    virtual unsigned maxFrameSize() const
    {
//        FUNCTION_PRINT
        return 100*1024;
    }

private:  
    static void getNextFrame (void *ptr)
    {
//	    FUNCTION_PRINT
        ((CameraFrameSource*)ptr)->getNextFrame1();  
    }  
  
    void getNextFrame1 ()
    {
//        FUNCTION_PRINT
        // capture:
        int luma_size = WIDTH * HEIGHT;
        int chroma_size = luma_size / 4;

        /* Read input frame */
	char szMsg[10] = {0};
        if(zmq_send(pSock, "YUYV", 4, 0) >= 0)
	{
		if(zmq_recv(pSock, szMsg, sizeof(szMsg), 0) >= 0)
		{
			if(strcmp(szMsg, "OK"))
			{
				printf("capture one frame\n");
			}
		}
	}
    
    char yuv420[luma_size*3];
    convert_yuv422_to_yuv420((char*)shared, yuv420, WIDTH, HEIGHT); 
    /* Read input frame */
    memcpy(pic.img.plane[0], yuv420, luma_size);
    memcpy(pic.img.plane[1], yuv420 + luma_size, chroma_size);
    memcpy(pic.img.plane[2], yuv420 + luma_size + chroma_size, chroma_size);
	
        pic.i_pts = i_frame++;
        i_frame_size = x264_encoder_encode( h, &nal, &i_nal, &pic, &pic_out );        /* Encode frames */
        printf("i_frame_size 1 = %d\n", i_frame_size);
         if(i_frame_size < 0)
         {
            printf("x264_encoder_encode failed\n");
            exit(-1);
         }
        
        if(i_frame_size)
        {
            //GOTO_IF_ERROR( !fwrite( nal->p_payload, i_frame_size, 1, fout ),
            //    fail_encode, "fwrite failed\n");
            // save outbuf
            gettimeofday(&fPresentationTime, 0); 
            fFrameSize = i_frame_size;
            if (fFrameSize > fMaxSize) {
                fNumTruncatedBytes = fFrameSize - fMaxSize;
                fFrameSize = fMaxSize;
            }
            else {
                fNumTruncatedBytes = 0;
            }
            memmove(fTo, nal->p_payload, fFrameSize);  
            
            afterGetting(this);  
            m_started = 0; 
        }
    }
};
  
class CameraOndemandMediaSubsession : public OnDemandServerMediaSubsession  
{  
public:
    static CameraOndemandMediaSubsession *createNew (UsageEnvironment &env)  
    {
        FUNCTION_PRINT
        return new CameraOndemandMediaSubsession(env);  
    }  
  
protected:  
    CameraOndemandMediaSubsession (UsageEnvironment &env)  
        : OnDemandServerMediaSubsession(env, reuseFirstSource) // reuse the first source  
    {  
        FUNCTION_PRINT
        mp_sdp_line = NULL;  
    }  
  
    ~CameraOndemandMediaSubsession ()  
    {
        FUNCTION_PRINT
        if (mp_sdp_line) free(mp_sdp_line);  
    }  
  
private:  
    static void afterPlayingDummy (void *ptr) 
    {
//        FUNCTION_PRINT
        // ok  
        CameraOndemandMediaSubsession *This = (CameraOndemandMediaSubsession*)ptr;  
        This->m_done = 0xff;  
    }  
  
    static void chkForAuxSDPLine (void *ptr)  
    {
//        FUNCTION_PRINT
        CameraOndemandMediaSubsession *This = (CameraOndemandMediaSubsession *)ptr;  
        This->chkForAuxSDPLine1();  
    }  
  
    void chkForAuxSDPLine1 ()  
    {
//        FUNCTION_PRINT
        if (mp_dummy_rtpsink->auxSDPLine())  
            m_done = 0xff;  
        else {  
            int delay = 100*1000;   // 100ms  
            nextTask() = envir().taskScheduler().scheduleDelayedTask(delay,  
                    chkForAuxSDPLine, this);  
        }  
    }  
  
protected:  
    virtual const char *getAuxSDPLine (RTPSink *sink, FramedSource *source)  
    {
        FUNCTION_PRINT
        if (mp_sdp_line) return mp_sdp_line;  
  
        mp_dummy_rtpsink = sink;  
        mp_dummy_rtpsink->startPlaying(*source, 0, 0);  
        //mp_dummy_rtpsink->startPlaying(*source, afterPlayingDummy, this);  
        chkForAuxSDPLine(this);
        m_done = 0;  
        envir().taskScheduler().doEventLoop(&m_done);  
        mp_sdp_line = strdup(mp_dummy_rtpsink->auxSDPLine());  
        mp_dummy_rtpsink->stopPlaying();  
  
        return mp_sdp_line;  
    }  
  
    virtual RTPSink *createNewRTPSink(Groupsock *rtpsock, unsigned char type, FramedSource *source)  
    {
        FUNCTION_PRINT
        return H264VideoRTPSink::createNew(envir(), rtpsock, type);  
    }  
  
    virtual FramedSource *createNewStreamSource (unsigned sid, unsigned &bitrate)  
    {
        FUNCTION_PRINT
        bitrate = 500;  
        return H264VideoStreamFramer::createNew(envir(), new CameraFrameSource(envir()));  
    }  
  
private:  
    char *mp_sdp_line;  
    RTPSink *mp_dummy_rtpsink;  
    char m_done;  
};

int get_lock(void)
{
    int fdlock;
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if((fdlock = open("/tmp/rtspserver.lock", O_WRONLY|O_CREAT, 0666)) == -1)
        return 0;
    if(fcntl(fdlock, F_SETLK, &fl) == -1)
        return 0;
    return 1;
}


int main(int argc, char** argv) {

    if(!get_lock())
    {
        printf("another instance(testRTSPServer) is running, exit.\n");
        exit(0);
    }
    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler); 

    UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
    // To implement client access control to the RTSP server, do the following:
    authDB = new UserAuthenticationDatabase;
    authDB->addUserRecord("username1", "password1"); // replace these with real strings
    // Repeat the above with each <username>, <password> that you wish to allow
    // access to the server.
#endif

    // Create the RTSP server:
    RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554, authDB);
    if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
    }

    char const* descriptionString
    = "Session streamed from /dev/video0";

    // A H.264 video elementary stream:
    {
    char const* streamName = "h264ESVideoTest";
    char const* inputFileName = "camera";
    OutPacketBuffer::maxSize = 100000;
    ServerMediaSession* sms
      = ServerMediaSession::createNew(*env, streamName, NULL, descriptionString);
    sms->addSubsession(CameraOndemandMediaSubsession::createNew(*env));
    rtspServer->addServerMediaSession(sms);

    announceStream(rtspServer, sms, streamName, inputFileName);
    }

    // Also, attempt to create a HTTP server for RTSP-over-HTTP tunneling.
    // Try first with the default HTTP port (80), and then with the alternative HTTP
    // port numbers (8000 and 8080).

    if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080)) {
    *env << "\n(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling.)\n";
    } else {
    *env << "\n(RTSP-over-HTTP tunneling is not available.)\n";
    }

    env->taskScheduler().doEventLoop(); // does not return

    return 0; // only to prevent compiler warning
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName, char const* inputFileName) {
    char* url = rtspServer->rtspURL(sms);
    UsageEnvironment& env = rtspServer->envir();
    env << "\n\"" << streamName << "\" stream, from the file \""
      << inputFileName << "\"\n";
    env << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;
}
