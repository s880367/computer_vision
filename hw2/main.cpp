#include <opencv2/opencv.hpp>
 
#include <stdio.h>
#include <stdlib.h>
#include <opencv2/legacy/legacy.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/nonfree/nonfree.hpp>
#include <opencv2/nonfree/features2d.hpp>

//#include <opencv2/imgcodecs.hpp> // imread
#include <opencv2/core/core.hpp> // imread
#include <opencv2/highgui/highgui.hpp> // imshow, waitKey

#include <math.h>
#include <cmath>
#include <limits.h>

#include <sstream>
#include <fstream>

#include <pthread.h>
#include <omp.h>

#include <string.h>
#include <time.h>


#define src_x1 278
#define src_y1 138
#define src_x2 760
#define src_y2 145
#define src_x3 762
#define src_y3 509
#define src_x4 273
#define src_y4 509


#define K_KNN 4  // k for KNN
#define RANSAC_DISTANCE 2

#define HNUMBER 50

#define H_START_NUM 0
#define H_END_NUM 4

#define OBJECT_IMG "object_11.bmp"
#define TARGET_IMG "object_12.bmp"

#define dst_x1 652
#define dst_y1 110
#define dst_x2 652
#define dst_y2 471
#define dst_x3 376
#define dst_y3 471
#define dst_x4 376
#define dst_y4 110
 
long thread_count;
long long n = 256;

int key1,key2;
std::vector<cv::KeyPoint> keypoints1, keypoints2;
std::vector<cv::KeyPoint> keypoints3, keypoints4;



using namespace std;
using namespace cv;

//void* RANSAC_Thread(void* rank);

Mat ComputeH(int n, Point2f *p1, Point2f *p2)
{ 
    int i; 
    CvMat *A = cvCreateMat(2*n, 9, CV_64FC1); 
    CvMat *U = cvCreateMat(2*n, 2*n, CV_64FC1); 
    CvMat *D = cvCreateMat(2*n, 9, CV_64FC1); 
    CvMat *V = cvCreateMat(9, 9, CV_64FC1); 
    Mat H_local;
    cvZero(A); 
    
    for(i = 0; i < 1; i++)
    { 
        // 2*i row 
        cvmSet(A,2*i,3,-p1[i].x); 
        cvmSet(A,2*i,4,-p1[i].y); 
        cvmSet(A,2*i,5,-1); 
        cvmSet(A,2*i,6,p2[i].y*p1[i].x); 
        cvmSet(A,2*i,7,p2[i].y*p1[i].y); 
        cvmSet(A,2*i,8,p2[i].y); 
        // 2*i+1 row 
        cvmSet(A,2*i+1,0,p1[i].x); 
        cvmSet(A,2*i+1,1,p1[i].y); 
        cvmSet(A,2*i+1,2,1); 
        cvmSet(A,2*i+1,6,-p2[i].x*p1[i].x); 
        cvmSet(A,2*i+1,7,-p2[i].x*p1[i].y); 
        cvmSet(A,2*i+1,8,-p2[i].x); 
    } 
    // SVD 
    // The flags cause U and V to be returned transpose
    // Therefore, in OpenCV, A = U^T D V 
    cvSVD(A, D, U, V, CV_SVD_U_T|CV_SVD_V_T);  

    // take the last column of V^T, i.e., last row of V
    /*for(i=0; i<9; i++) 
        cvmSet(H_local, i/3, i%3, cvmGet(V, 8, i)); */
      
    cvReleaseMat(&A); 
    cvReleaseMat(&U); 
    cvReleaseMat(&D); 
    cvReleaseMat(&V); 

    return H_local;
} 
 
/*****************************************************************************

1. This function check the type of Mat
2. input  : Mat.type()
3. output : char of ty.c_str()
4. usage  : 
            string ty =  type2str( M.type() );
            printf("Matrix H: %s %dx%d \n", ty.c_str(), M.cols, M.rows );

******************************************************************************/
string type2str(int type) {
  string r;

  uchar depth = type & CV_MAT_DEPTH_MASK;
  uchar chans = 1 + (type >> CV_CN_SHIFT);

  switch ( depth ) {
    case CV_8U:  r = "8U"; break;
    case CV_8S:  r = "8S"; break;
    case CV_16U: r = "16U"; break;
    case CV_16S: r = "16S"; break;
    case CV_32S: r = "32S"; break;
    case CV_32F: r = "32F"; break;
    case CV_64F: r = "64F"; break;
    default:     r = "User"; break;
  }

  r += "C";
  r += (chans+'0');

  return r;
}

float EucliDistance(Point& p, Point& q) 
{
    Point diff = p - q;
    return cv::sqrt(diff.x*diff.x + diff.y*diff.y);
}

double ComputeDistance(double x1, double y1, double x2, double y2)
{
    double distance;
    double x_diff = x1-x2;
    double y_diff = y1-y2;
    
    distance = sqrt(x_diff*x_diff + y_diff*y_diff);
    return distance;
}


Mat SecondProcess( Mat ObjectImage, Mat TargetImage,int cluster)
{
    int diff_vector = 0;

    printf("object rows = %d, cols = %d\n",ObjectImage.rows, ObjectImage.cols );
    printf("Target rows = %d, cols = %d\n",TargetImage.rows, TargetImage.cols );

    //Mat ObjectImage = cv::imread( OBJECT_IMG, 1 );  //type : 8UC3
    //Mat TargetImage = cv::imread( TARGET_IMG, 1 );
 
    /* threshold      = 0.04;
       edge_threshold = 10.0;
       magnification  = 3.0;    */
 
    // SIFT feature detector and feature extractor
    cv::SiftFeatureDetector detector( 0.05, 5.0 );
    cv::SiftDescriptorExtractor extractor( 3.0 );
 
    // Feature detection
    std::vector<cv::KeyPoint> keypoints1, keypoints2;
    detector.detect( ObjectImage, keypoints1 );
    detector.detect( TargetImage, keypoints2 );
 
    // Feature display
    Mat feat1,feat2;
    drawKeypoints(ObjectImage,keypoints1,feat1,Scalar(255, 255, 255),DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    drawKeypoints(TargetImage,keypoints2,feat2,Scalar(255, 255, 255),DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    imwrite( "feat1.bmp", feat1 );
    imwrite( "feat2.bmp", feat2 );
    //imshow("feat2",feat2);

    int key1 = keypoints1.size();   //object
    int key2 = keypoints2.size();   //target
    printf("Keypoint1 = %d \n",key1);
    printf("Keypoint2 = %d \n",key2);
 
    // Feature descriptor computation
    Mat descriptor1,descriptor2;
    extractor.compute( ObjectImage, keypoints1, descriptor1 );
    extractor.compute( TargetImage, keypoints2, descriptor2 );

    printf("Descriptor1=(%d,%d)\n", descriptor1.size().height,descriptor1.size().width);
    printf("Descriptor2=(%d,%d)\n", descriptor2.size().height,descriptor2.size().width);

    
    int **KeyPoint_Neighborhood;    //int KeyPoint_Neighborhood[key1][key2];
    int **K_NearestNeighbor;   //int K_NearestNeighbor[key1][K_KNN];

    KeyPoint_Neighborhood = (int **)malloc(key1*sizeof(int*));
    for (int i = 0; i < key1; ++i)
        KeyPoint_Neighborhood[i] = (int *)malloc(key2*sizeof(int));

    K_NearestNeighbor = (int **)malloc(key1*sizeof(int*));
    for (int i = 0; i < key1; ++i)
        K_NearestNeighbor[i] = (int *)malloc(K_KNN*sizeof(int));


    cout << "computing KNN" << endl;

    for (size_t i = 0; i < key1; ++i)   //need threading
    {
        for (size_t j = 0; j < key2; ++j)
        {
            for (size_t k = 0; k < 128; ++k)
            {
                diff_vector +=  (descriptor1.at<float>(i,k) - descriptor2.at<float>(j,k))*(descriptor1.at<float>(i,k) - descriptor2.at<float>(j,k));    
            }
            KeyPoint_Neighborhood[i][j] = sqrt(diff_vector);
            diff_vector = 0;
        }
    }

    int Min_Distance;
    int Min_Index = 0;

    for (size_t i = 0; i < key1; ++i)   //need threading7
    {   
        for (size_t j = 0; j < K_KNN; ++j)
        {
            Min_Distance = KeyPoint_Neighborhood[i][0]; 
            for (size_t k = 0; k < key2; ++k)
            {
                if(Min_Distance > KeyPoint_Neighborhood[i][k])
                {
                    Min_Distance = KeyPoint_Neighborhood[i][k];
                    Min_Index = k;
                } 
            }
            K_NearestNeighbor[i][j] = Min_Index;
            KeyPoint_Neighborhood[i][Min_Index] = INT_MAX;
        }
    }



    std::vector<Point2f> obj[HNUMBER];
    std::vector<Point2f> scene[HNUMBER];
    int Extract_Index[HNUMBER][4];
    int m = 0;
    int IndexForKNN;
    int IndexForScene; 
    int H_InlierNumber[HNUMBER];
    //memset(H_InlierNumber,0,sizeof(int));
    Mat H[HNUMBER];
    int count = 0;
    int store[HNUMBER][4];
    int Candidate_store[ITERATIVE][4];
    srand(time(NULL));


    cout << "computing Homography" << endl;
    for (int i = 0; i < HNUMBER; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            int CorespondIndex = (rand()% key1);
            store[i][j] = CorespondIndex;

            IndexForScene = K_NearestNeighbor[CorespondIndex][rand()% 2];
            scene[i].push_back( keypoints2[IndexForScene].pt );
            obj[i].push_back( keypoints1[CorespondIndex].pt );
        }
        H[i] = findHomography(obj[i], scene[i]);
        obj[i].clear();
        scene[i].clear();
    }
    
    double Candidate[3];
    Mat TargetKeypoint[key1];
    double Object_X, Object_Y;
    double Target_X, Target_Y;
    double distance;
    int Max_InlierNumber;
    int Max_InlierIndex;

    Max_InlierNumber = 0;
    Max_InlierIndex = 0;

    cout << "Computing the best Homography using RANSAC" << endl;
    for (int i = 0; i < HNUMBER; ++i)
    {
        H_InlierNumber[i] = 0;
        for (int j = 0; j < key1; j++)
        {
            Candidate[0] = keypoints1[j].pt.x;
            Candidate[1] = keypoints1[j].pt.y;
            Candidate[2] = 1;

            Mat Before(3,1,CV_64FC1,Candidate);
            TargetKeypoint[j] = H[i]*Before;               
            Target_X = TargetKeypoint[j].at<double>(0,0);
            Target_Y = TargetKeypoint[j].at<double>(0,1);
            
            for (int k = 0; k < key2; k++)
            {
                Object_X = keypoints2[k].pt.x;
                Object_Y = keypoints2[k].pt.y;
                distance = ComputeDistance(Target_X,Target_Y,Object_X,Object_Y);

                if (distance < RANSAC_DISTANCE)
                {
                    H_InlierNumber[i]++;
                    break;
                }
            }
        }
        if(H_InlierNumber[i] > 60)
            printf("H[%d] InlierNumber : %d \n",i,H_InlierNumber[i]);
        if (H_InlierNumber[i] > Max_InlierNumber)
        {
            Max_InlierNumber = H_InlierNumber[i];
            Max_InlierIndex = i;
        }
    }


    Mat Candidate_H;
    int Candidate_InlierNumber;

    printf("the best Candidate Homography[%d] inliernumber = %d\n",Max_InlierIndex , Max_InlierNumber );

    Candidate_H = H[Max_InlierIndex].clone();
    Candidate_InlierNumber = Max_InlierNumber;

    cout << Candidate_H << endl;

    vector<Point2f> src;
    vector<Point2f> dst;
    int inliercount = 0;
    Mat amature_H;
    int icp = 0;
    Mat drawobject = ObjectImage.clone();
    Mat drawtarget = TargetImage.clone();

    cout << "Homograpy final refinment" << endl;
    while(icp < 1)
    {
        amature_H = Candidate_H.clone();
        src.clear();
        dst.clear();
        inliercount = 0;

        for (int j = 0; j < key1; j++)
        {
            Candidate[0] = keypoints1[j].pt.x;
            Candidate[1] = keypoints1[j].pt.y;
            Candidate[2] = 1;

            Mat Beforee(3,1,CV_64FC1,Candidate);
            TargetKeypoint[j] = amature_H * Beforee;            
            Target_X = TargetKeypoint[j].at<double>(0,0);
            Target_Y = TargetKeypoint[j].at<double>(0,1);
            
            for (int k = 0; k < key2; k++)
            {
                Object_X = keypoints2[k].pt.x;
                Object_Y = keypoints2[k].pt.y;
                distance = ComputeDistance(Target_X,Target_Y,Object_X,Object_Y);

                if (distance < 1)
                {
                    src.push_back(keypoints1[j].pt);
                    dst.push_back(keypoints2[k].pt);
                    inliercount++;
                    break;
                }
            }
        }
        for (int i = 0; i < src.size(); i++)
        {
            circle(drawobject,cvPoint(src[i].x,src[i].y),10,CV_RGB(0,255,0),2,8,0);
            circle(drawtarget,cvPoint(dst[i].x,dst[i].y),10,CV_RGB(0,255,0),2,8,0);
        }
        
        Candidate_H = findHomography(src,dst);
        cout << "inliercount = "<< inliercount << endl;
        icp++;
    }

    Mat H_inverse;
    Mat Reconvered_H;


    if (cluster == 2)
    {
        src.clear();
        src.push_back(Point2f(src_x1,src_y1));
        src.push_back(Point2f(src_x2,src_y2));
        src.push_back(Point2f(src_x3,src_y3));
        src.push_back(Point2f(src_x4,src_y4));

        dst.clear();
        dst.push_back(Point2f(dst_x1,dst_y1));
        dst.push_back(Point2f(dst_x2,dst_y2));
        dst.push_back(Point2f(dst_x3,dst_y3));
        dst.push_back(Point2f(dst_x4,dst_y4));

        Reconvered_H = findHomography(src,dst);
        cout << Reconvered_H << endl;
        H_inverse = Reconvered_H.inv();
    }
    else
    {
        Reconvered_H = Candidate_H.clone();
        cout << Reconvered_H << endl;
        H_inverse = Reconvered_H.inv();
    }

/*
    int inliercount = 0;
    double Candidate[3];
    Mat TargetKeypoint[key1];
    double Object_X, Object_Y;
    double Target_X, Target_Y;
    double distance;
    int Max_InlierNumber;
    int Max_InlierIndex;

    Max_InlierNumber = 0;
    Max_InlierIndex = 0;
    for (int j = 0; j < key1; j++)
    {
        Candidate[0] = keypoints1[j].pt.x;
        Candidate[1] = keypoints1[j].pt.y;
        Candidate[2] = 1;

        Mat Beforee(3,1,CV_64FC1,Candidate);
        TargetKeypoint[j] = Reconvered_H * Beforee;            
        Target_X = TargetKeypoint[j].at<double>(0,0);
        Target_Y = TargetKeypoint[j].at<double>(0,1);
        
        for (int k = 0; k < key2; k++)
        {
            Object_X = keypoints2[k].pt.x;
            Object_Y = keypoints2[k].pt.y;
            distance = ComputeDistance(Target_X,Target_Y,Object_X,Object_Y);

            if (distance < RANSAC_DISTANCE)
            {
                inliercount++;
                break;
            }
        }
    }
    cout << "inliercount = "<< inliercount << endl;
    cout << "final reconvered = " << Reconvered_H << endl;
*/



    Mat temp = Mat::zeros(TargetImage.rows, TargetImage.cols,CV_8UC3);
    Mat WarpingImage = TargetImage.clone();
    Mat WarpingImage_inv = ObjectImage.clone();


    //printf("object : row = %d, col = %d\n",ObjectImage.rows, ObjectImage.cols );
    //printf("warping : row = %d, col = %d\n",WarpingImage.rows, WarpingImage.cols );
    printf("Warping\n");
    Mat WarpingPoint;
    Mat TempImage;
    warpPerspective(ObjectImage, TempImage, Reconvered_H, TargetImage.size());

    Mat TempImage_inv;
    warpPerspective(TargetImage,TempImage_inv,H_inverse,ObjectImage.size());

    for (int i = 0; i <= TempImage.rows-1 ; i++)      //rows
    {
        for (int j = 0; j <= TempImage.cols-1; j++)   //cols
        {
            if(TempImage.at<Vec3b>(i,j)[0] != 255 && TempImage.at<Vec3b>(i,j)[1] != 255 && TempImage.at<Vec3b>(i,j)[2] != 255)
            {
                if(TempImage.at<Vec3b>(i,j)[0] != 0 && TempImage.at<Vec3b>(i,j)[1] != 0 && TempImage.at<Vec3b>(i,j)[2] != 0)
                    WarpingImage.at<Vec3b>(i,j) = TempImage.at<Vec3b>(i,j);     
            }    
        }
    }

    for (int i = 0; i <= TempImage_inv.rows-1 ; i++)      //rows
    {
        for (int j = 0; j <= TempImage_inv.cols-1; j++)   //cols
        {
            if(TempImage_inv.at<Vec3b>(i,j)[0] != 255 && TempImage_inv.at<Vec3b>(i,j)[1] != 255 && TempImage_inv.at<Vec3b>(i,j)[2] != 255)
            {
                if(TempImage_inv.at<Vec3b>(i,j)[0] != 0 && TempImage_inv.at<Vec3b>(i,j)[1] != 0 && TempImage_inv.at<Vec3b>(i,j)[2] != 0)
                    if(WarpingImage_inv.at<Vec3b>(i,j)[0] != 255 && WarpingImage_inv.at<Vec3b>(i,j)[1] != 255 && WarpingImage_inv.at<Vec3b>(i,j)[2] != 255)
                        WarpingImage_inv.at<Vec3b>(i,j) = TempImage_inv.at<Vec3b>(i,j);     
            }    
        }
    }

    imwrite("backwarping.bmp",WarpingImage_inv);

    /*for (int i = 0; i <= ObjectImage.rows-1 ; i++)      //rows
    {
        for (int j = 0; j <= ObjectImage.cols-1; j++)   //cols
        {
            if(ObjectImage.at<Vec3b>(i,j)[0] != 255 || ObjectImage.at<Vec3b>(i,j)[1] != 255 || ObjectImage.at<Vec3b>(i,j)[2] != 255)
            {
                Candidate[0] = i;
                Candidate[1] = j;
                Candidate[2] = 1;
                Mat Before(3,1,CV_64FC1,Candidate);
                WarpingPoint = Reconvered_H*Before;
                int u = WarpingPoint.at<double>(0,2);
                int x = WarpingPoint.at<double>(0,0);
                int y = WarpingPoint.at<double>(0,1);
                if ( x >= 0 && y >= 0)
                    temp.at<Vec3b>(x,y) = ObjectImage.at<Vec3b>(i,j);     
            }    
        }
    }*/

    imshow("drawobject",drawobject);
    imshow("drawtarget",drawtarget);

    free(KeyPoint_Neighborhood);
    free(K_NearestNeighbor);


    waitKey(0);
    return WarpingImage;
}


/*void* RANSAC_Thread(void* rank)
{
    long my_rank = (long) rank;
    long long i;
    long long part = n / thread_count;
    long long my_first_i = part * my_rank;
    long long my_last_i = my_first_i + part;

    double Candidate[3];
    Mat TargetKeypoint[key1];
    double Object_X, Object_Y;
    double Target_X, Target_Y;
    double distance;

    int Max_InlierNumber = 0;
    int Max_InlierIndex = 0;

    //printf("thread %ld is working : %lld - %lld\n",my_rank,my_first_i,my_last_i );

    for (int i = my_first_i; i < my_last_i; ++i)
    {
        for (int j = 0; j < key1; j++)
        {
            Candidate[0] = keypoints1[j].pt.x;
            Candidate[1] = keypoints1[j].pt.y;
            Candidate[2] = 1;

            Mat Before(3,1,CV_64FC1,Candidate);
            TargetKeypoint[j] = H[i]*Before;                     
            Target_X = TargetKeypoint[j].at<double>(0,0);
            Target_Y = TargetKeypoint[j].at<double>(0,1);
            
            for (int k = 0; k < key2; k++)
            {
                Object_X = keypoints2[k].pt.x;
                Object_Y = keypoints2[k].pt.y;
                distance = ComputeDistance(Target_X,Target_Y,Object_X,Object_Y);

                if (distance < RANSAC_DISTANCE)
                {
                    H_InlierNumber[i]++;
                    break;
                }
            }

        }
    }
}*/


int main(int argc, char const *argv[])
{
    Mat ObjectImage = cv::imread( argv[1], 1 );  //type : 8UC3
    Mat TargetImage = cv::imread( argv[2], 1 );
    Mat ResultImage;
    Mat TempImage;
    char cmp[11] = "object.bmp";
    double start, end;
    clock_t clock();
    start = clock();

    if (strcmp(argv[1],cmp) == 0)
        ResultImage = SecondProcess(ObjectImage,TargetImage,2);
    else
        ResultImage = SecondProcess(ObjectImage,TargetImage,1);



    imshow("result",ResultImage);
    imwrite( "result_now.bmp", ResultImage );

    end = clock();
    printf("the time elasped = %f s\n",(end-start)/CLOCKS_PER_SEC);
    waitKey(0);

    return 0;
}

