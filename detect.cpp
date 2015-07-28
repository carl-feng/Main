#include "opencv2/core/core.hpp"
#include "opencv2/video/background_segm.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <unistd.h>

using namespace std;
using namespace cv;
const double PI = 3.141592654;
double area = 0;
int largestComp = 0;

bool update_bg_model = true; 
int PicNum = 0;

BackgroundSubtractorMOG2 bg_model;

void drawDetectLines(Mat& image, const vector<Vec4i>& lines, Scalar& color) 
{ 
    vector<Vec4i>::const_iterator it=lines.begin(); 
    while(it!=lines.end()) 
    { 
        Point pt1((*it)[0],(*it)[1]); 
        Point pt2((*it)[2],(*it)[3]);
        line(image,pt1,pt2,Scalar(0,255,0),2);
        ++it; 
    } 
}

bool TargetDetection(Mat img, int Pixel_Threshold, bool update_bg_model)
{
    int niters = 3;
    bool Danger_Flag=false;
    Mat temp, fgmask, fgimg;
    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;

    bg_model(img, fgmask, update_bg_model ? 0.005 : 0);

    if(update_bg_model) return false;

    dilate(fgmask, temp, Mat(), Point(-1,-1), niters);
    erode(temp, temp, Mat(), Point(-1,-1), niters*2);
    dilate(temp, temp, Mat(), Point(-1,-1), niters);

    findContours(temp, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);

    fgimg = Mat::zeros(img.size(), CV_8UC3);

    if(contours.size() == 0)
        return 0;

    int idx = 0, largestComp = 0;
    for(; idx <= contours.size() -1 ; idx =idx+1)
    {
        double  nowArea=0;
        vector<Point> hull; 
        convexHull(Mat(contours[idx]), hull);
        area = contourArea(hull);
        printf("&&&&&&&&&&&&&&&&&&&& threshold = %d, area = %f\n", Pixel_Threshold, area);
        if (area > Pixel_Threshold)
        {
            //largestComp = idx;
            //nowArea=area;
            //Scalar color(0, 255, 0);
            //drawContours(fgimg, contours, largestComp, color, CV_FILLED, 8, hierarchy);
            Danger_Flag=true;
         }
    }

    return Danger_Flag;
}

void ReadandTrain(Mat img)
{
    TargetDetection(img, 0, true);
}

/*int main()
{
    VideoCapture capture(1);
    //capture.open("C:\\Users\\Administrator\\Desktop\\1\\2.avi");

    if (!capture.isOpened())
    {
        std::cout<<"read video failure"<<std::endl;
        return -1;
    }
    sleep(1000);

    Mat img;
    while (capture.read(img))
    {
        resize(img, img, Size(640,480),0,0,CV_INTER_LINEAR);
        bool bRet = ReadandDetect(img);
        printf("bRet = %d\n", bRet);
    }
    return 0;
}*/
