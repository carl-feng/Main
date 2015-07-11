#include "opencv2/core/core.hpp"
#include "opencv2/video/background_segm.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <unistd.h>

using namespace std;
using namespace cv;
const double PI=3.141592654;
double  maxArea=0;
double  area=0;
int largestComp=0;

//int i=-1;
bool update_bg_model = true;
 
int num1;
int num2;

int Pixel_Threshold = 100;
int PicNum=0;

BackgroundSubtractorMOG2 bg_model;

void refineSegments(const Mat& img, Mat& mask, Mat& dst)
{
    int niters = 3;

    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;

    Mat temp;

    dilate(mask, temp, Mat(), Point(-1,-1), niters);
    erode(temp, temp, Mat(), Point(-1,-1), niters*2);
    dilate(temp, temp, Mat(), Point(-1,-1), niters);

    findContours( temp, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE );

    dst = Mat::zeros(img.size(), CV_8UC3);

    if( contours.size() == 0 )
        return;

    int idx = 0, largestComp = 0;
    double maxArea = 0;

	 for( ; idx <=contours.size()-1; idx =idx+1 )
    {
		float radius; 
        Point2f center; 
        minEnclosingCircle(Mat(contours[idx]), center, radius);
	    area=PI*radius*radius;

        if( area > maxArea )
        {
            maxArea = area;
            largestComp = idx;
        }
    }
    Scalar color( 0, 255, 0 );
    drawContours( dst, contours, largestComp, color, CV_FILLED, 8, hierarchy );
}

void drawDetectLines(Mat& image, const vector<Vec4i>& lines, Scalar& color) 
{ 
    vector<Vec4i>::const_iterator it=lines.begin(); 
    while(it!=lines.end()) 
    { 
        Point pt1((*it)[0],(*it)[1]); 
        Point pt2((*it)[2],(*it)[3]); 
        line(image,pt1,pt2,color,2);
        ++it; 
    } 
} 

bool TargetDetection(Mat img,int Pixel_Threshold, bool update_bg_model)
{
	bool Danger_Flag=false;
    Mat fgmask, fgimg;

    bg_model(img, fgmask, update_bg_model ? 0.005 : 0);
    refineSegments(img, fgmask, fgimg);

	//imshow("image", img);
	//imshow("foreground image", fgimg);

	cvtColor(fgimg,fgimg,CV_BGR2GRAY);

    num1=countNonZero(fgimg);
    printf("num1 = %d\n", num1);
    
    Mat edege; 
    Canny(fgimg,edege,125,350); 
    threshold(edege,edege,128,255,THRESH_BINARY);
    //imshow("Canny Image",edege);

    vector<Vec4i> lines;
    Scalar scalar(255,0,0);
    HoughLinesP(edege, lines, 1, CV_PI/180, 80, 30, 10);
    //drawDetectLines(fgimg, lines, scalar); 
    //imshow("line",fgimg);

	if((num1>Pixel_Threshold) || lines.size() > 0)
	{
		Danger_Flag=true;
	}
	else
	{
		Danger_Flag=false;
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
