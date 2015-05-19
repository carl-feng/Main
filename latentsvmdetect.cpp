#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"

#ifdef HAVE_CVCONFIG_H
#include <cvconfig.h>
#endif
#ifdef HAVE_TBB
#include "tbb/task_scheduler_init.h"
#endif

using namespace cv;

static bool detect(const char* image_filename, const char* model_filename, int tbbNumThreads = 4)
{
    bool bRisk = false;
    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* detections = 0;
    int i = 0;
    int64 start = 0, finish = 0;

    IplImage* image = cvLoadImage(image_filename);
    if (!image)
        return false;
    CvLatentSvmDetector* detector = cvLoadLatentSvmDetector(model_filename);
    if (!detector)
    {
        cvReleaseImage(&image);
        return false;
    }
#ifdef HAVE_TBB
    tbb::task_scheduler_init init(tbb::task_scheduler_init::deferred);
    if (numThreads > 0)
    {
        init.initialize(numThreads);
        printf("Number of threads %i\n", numThreads);
    }
    else
    {
        printf("Number of threads is not correct for TBB version");
        return;
    }
#endif

    start = cvGetTickCount();
    detections = cvLatentSvmDetectObjects(image, detector, storage, 0.5f, numThreads);
    finish = cvGetTickCount();
    printf("detection time = %.3f\n", (float)(finish - start) / (float)(cvGetTickFrequency() * 1000000.0));

#ifdef HAVE_TBB
    init.terminate();
#endif
    for( i = 0; i < detections->total; i++ )
    {
        CvObjectDetection detection = *(CvObjectDetection*)cvGetSeqElem(detections, i);
	if(detection.score > 0)
        {
            bRisk = true;
            break;
        }
    }
    cvReleaseMemStorage( &storage );
    cvReleaseLatentSvmDetector(&detector);
    cvReleaseImage(&image);
    return bRisk;
}
