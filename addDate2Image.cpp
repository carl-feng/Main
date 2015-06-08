#include <time.h>
#include <iostream>
#include "cv.h"
#include "highgui.h"

using namespace std;

int main(int argc, char* argv[])
{
    if(argc != 2)
    {   
        printf("Usage: %s [image path]\n", argv[0]);
        return -1;
    }
    IplImage* pImg = cvLoadImage(argv[1]);
    if (!pImg)
    {
        printf("load image(%s) failed\n", argv[1]);
        return -2;
    }
    time_t timep;
    time(&timep);
    struct tm *pTM = localtime(&timep);
    char buffer[50] = {0};
    snprintf(buffer, 50, "%04d-%02d-%02d %02d:%02d:%02d", pTM->tm_year+1900,
        pTM->tm_mon+1, pTM->tm_mday, pTM->tm_hour, pTM->tm_min, pTM->tm_sec);
    CvFont font;
    cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 0.7, 0.7);
    cvPutText(pImg, buffer, cvPoint(10, 50), &font, CV_RGB(250,250,250));
    cvSaveImage(argv[1], pImg);
    cvReleaseImage(&pImg);
    
    return 0;
}
