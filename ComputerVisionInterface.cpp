#include "ComputerVisionInterface.h"

ComputerVisionInterface::ComputerVisionInterface()
{

}

int ComputerVisionInterface::InitInterface(COMPUTER_VISION_PARAM sComputerVisionParam)
{
    string models_folder = "./Mdl";
    //float overlapThreshold = 0.2f;//0.2f

    vector<string> models_filenames;

    readDirectory( models_folder, models_filenames );

    this->m_pDetector = new LatentSvmDetector( models_filenames );
    if( this->m_pDetector->empty() )
    {
        cout << "Models cann't be loaded" << endl;
        return 0;
    }

    memcpy(&(this->m_sComputerVisionParam) ,&sComputerVisionParam,sizeof(sComputerVisionParam));

    const vector<string>& classNames = this->m_pDetector->getClassNames();
    cout << "Loaded " << classNames.size() << " models:" << endl;
    for( size_t i = 0; i < classNames.size(); i++ )
    {
        cout << i << ") " << classNames[i] << "; ";
    }
    cout << endl;

    m_vColors.clear();

    generateColors( m_vColors, this->m_pDetector->getClassNames().size() );


    if(m_sComputerVisionParam.nInputImageHeight*m_sComputerVisionParam.nInputImageWidth <= 0 )
    {
        printf("Input Param Error!\n");
        return 0;
    }
    //load image
    this->m_mColorImage = Mat::zeros(m_sComputerVisionParam.nInputImageHeight,m_sComputerVisionParam.nInputImageWidth,CV_8UC3);

    this->m_mMaskImage = Mat::zeros(m_sComputerVisionParam.nInputImageHeight,m_sComputerVisionParam.nInputImageWidth,CV_8UC1);

    this->m_mMaskImage = imread(m_sComputerVisionParam.sMaskImageFileName , 0);

    return 1;

}

int ComputerVisionInterface::SetInterfaceParam(COMPUTER_VISION_PARAM sComputerVisionParam)
{
    memcpy(&(this->m_sComputerVisionParam) ,&sComputerVisionParam,sizeof(sComputerVisionParam));

    return 1;
}

bool ComputerVisionInterface::DoNext(unsigned char *pRGBImageBuffer)
{
    memcpy(this->m_mColorImage.data , pRGBImageBuffer , (this->m_sComputerVisionParam.nInputImageHeight*this->m_sComputerVisionParam.nInputImageWidth)*3);
    return detectAndDrawObjects();
}
bool ComputerVisionInterface::detectAndDrawObjects()
{
    //temp Mask
    Mat mTempImage = Mat::zeros(this->m_sComputerVisionParam.nInputImageHeight,this->m_sComputerVisionParam.nInputImageWidth,CV_8UC1);

    vector<LatentSvmDetector::ObjectDetection> detections;

    TickMeter tm;
    tm.start();
    this->m_pDetector->detect( this->m_mColorImage, detections, this->m_sComputerVisionParam.fThreshold, 4);
    tm.stop();

    cout << "Detection time = " << tm.getTimeSec() << " sec" << endl;


    const vector<string> classNames = this->m_pDetector->getClassNames();
    CV_Assert( this->m_vColors.size() == classNames.size() );
    float maxScore = -2.0f;
    int maxIndex = -1;
    for( size_t i = 0; i < detections.size(); i++ )
    {
        const LatentSvmDetector::ObjectDetection& od = detections[i];

        memset(mTempImage.data , 0, PROCESS_HEIGHT*PROCESS_WIDTH);

        rectangle( mTempImage, od.rect, Scalar(255,255,255), -1 );

        if(mixAreaPixelNum(this->m_mMaskImage , mTempImage , this->m_sComputerVisionParam.fPercentageThreshold) == false)
        {
            cout << "Not In the Area Or Too Small "<< endl;
            continue;
        }

        cout << "Detection score = " << od.score << endl;

        if( od.score > maxScore)
        {
            maxScore = od.score;
            maxIndex = i;
        }
    }

    if(maxScore > this->m_sComputerVisionParam.fScoreThreshold)
    {
        const LatentSvmDetector::ObjectDetection& od = detections[maxIndex];
        //if( od.score > -1.0f)       //    continue;

        cout << "Detection score = " << od.score << endl;
        //if( od.score > -1.0f)
        //    continue;
        rectangle( this->m_mColorImage, od.rect, this->m_vColors[od.classID], 3 );
        char buffer[512];
        memset(buffer , 0, 512);
        sprintf(buffer ,"%s %f",classNames[od.classID].c_str()  ,od.score);
        putText(  this->m_mColorImage, buffer , Point(od.rect.x+4,od.rect.y+13), FONT_HERSHEY_SIMPLEX, 0.55, this->m_vColors[od.classID], 1 );

        return true;
    }
    return false;
}

bool ComputerVisionInterface::mixAreaPixelNum(Mat mMaskMain, Mat mMaskTwo, float fPercent)
{
    long nMaskMainNum = mMaskMain.cols*mMaskMain.rows;
    long nMaskTwoNum = mMaskTwo.cols*mMaskTwo.rows;

    printf("nMaskMainNum = %ld\t nMaskTwoNum= %ld \n",nMaskMainNum,nMaskTwoNum);
    if(long(nMaskMainNum*fPercent) < nMaskTwoNum)
    {
        return true;
    }
    return false;
}

void ComputerVisionInterface::readDirectory(const string &directoryName, vector<string> &filenames, bool addDirectoryName)
{
    filenames.clear();

#ifdef WIN32
    struct _finddata_t s_file;
    string str = directoryName + "\\*.*";

    intptr_t h_file = _findfirst( str.c_str(), &s_file );
    if( h_file != static_cast<intptr_t>(-1.0) )
    {
        do
        {
            if( addDirectoryName )
                filenames.push_back(directoryName + "\\" + s_file.name);
            else
                filenames.push_back((string)s_file.name);
        }
        while( _findnext( h_file, &s_file ) == 0 );
    }
    _findclose( h_file );
#else
    DIR* dir = opendir( directoryName.c_str() );
    if( dir != NULL )
    {
        struct dirent* dent;
        while( (dent = readdir(dir)) != NULL )
        {
            if( addDirectoryName )
                filenames.push_back( directoryName + "/" + string(dent->d_name) );
            else
                filenames.push_back( string(dent->d_name) );
        }
    }
#endif

    sort( filenames.begin(), filenames.end() );
}
