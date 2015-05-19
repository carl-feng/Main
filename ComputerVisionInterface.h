//ComputerVisionInterface.h

#ifndef COMPUTERVISIONINTERFACE_H
#define COMPUTERVISIONINTERFACE_H


#ifndef COMPUTERVISIONPARAM
#define COMPUTERVISIONPARAM

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>	// SIGINT, signal()
#include "opencv2/opencv.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/contrib/contrib.hpp"

#ifdef WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

#ifdef HAVE_CVCONFIG_H
#include <cvconfig.h>
#endif

#ifdef HAVE_TBB
#include "tbb/task_scheduler_init.h"
#endif

using namespace std;
using namespace cv;

#ifndef PROCESS_WIDTH
    #define PROCESS_WIDTH 720
#endif
#ifndef PROCESS_HEIGHT
    #define PROCESS_HEIGHT 576
#endif
//Define the data stucture that will be used in the Vision class
struct COMPUTER_VISION_PARAM
{
    int nInputImageWidth;		//image's width
    int nInputImageHeight;		//image's height
    float fPercentageThreshold;	//threshold for position and area
    float fScoreThreshold;		//threshold for object's score
    float fThreshold;		//threshold for object's score
    char sMaskImageFileName[100];	//mask image's file name
};

#endif

//bool mixAreaPixelNum(Mat mMaskMain , Mat mMaskTwo , float fPercent);
//bool detectAndDrawObjects( Mat& image, Mat mMaskImage,LatentSvmDetector& detector, const vector<Scalar>& colors, float overlapThreshold, int numThreads ,float fScoreThreshold = 0.0f ,float fPercentageThreshold = 1.0f);
//static void readDirectory( const string& directoryName, vector<string>& filenames, bool addDirectoryName=true );
//the export method of our vision class
class ComputerVisionInterface
{
//varibles
private:
	COMPUTER_VISION_PARAM m_sComputerVisionParam;
	Mat m_mColorImage;
	Mat m_mMaskImage;
	LatentSvmDetector* m_pDetector;
	vector<Scalar> m_vColors;
//methods	
private:
	bool mixAreaPixelNum(Mat mMaskMain , Mat mMaskTwo , float fPercent);
	bool detectAndDrawObjects();
	void readDirectory( const string& directoryName, vector<string>& filenames, bool addDirectoryName=true );
	
public:
	//constructor
    ComputerVisionInterface();
    //initialize method,only called when the class's instance is declared.
    int InitInterface(COMPUTER_VISION_PARAM sComputerVisionParam);
    //set parameter,you can call it at any time.
    int SetInterfaceParam(COMPUTER_VISION_PARAM sComputerVisionParam);
    //the method for image analysis , call it in every cycle.
    bool DoNext(unsigned char *pRGBImageBuffer);
};

#endif // COMPUTERVISIONINTERFACE_H
