//
//  clahe.h
//  CLAHE_test
//
//  Created by Guillaume on 22.04.17.
//
//

#ifndef clahe_h
#define clahe_h

class Clahe {
    
public:
    cv::Mat greyImg, labImg, claheImg, tmpImg;
    
    template <class S, class D>
    void filter(S& src, D& dst, int clipLimit, bool isColor) {
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
        clahe->setClipLimit(clipLimit);
        if (!isColor) {
            ofxCv::copyGray(src, greyImg);
            clahe->apply(greyImg, claheImg);
            // convert back to RGB
            cv::cvtColor(claheImg, claheImg, CV_GRAY2RGB);
            // convert to ofImage
            ofxCv::toOf(claheImg, dst);
        } else {
            cv::cvtColor(ofxCv::toCv(src), labImg, CV_BGR2Lab);
            // ofxCv::convertColor(src, labImg, CV_BGR2Lab);
            
            // Extract the L channel
            vector<cv::Mat> lab_planes(3);
            cv::split(labImg, lab_planes);  // now we have the L image in lab_planes[0]
            
            // apply the CLAHE algorithm to the L channel
            clahe->apply(lab_planes[0], tmpImg);
            
            // Merge the the color planes back into an Lab img
            tmpImg.copyTo(lab_planes[0]);
            cv::merge(lab_planes, labImg);
            
            // convert back to RGB
            cv::cvtColor(labImg, claheImg, CV_Lab2BGR);
            
            // convert to ofImage
            ofxCv::toOf(claheImg, dst);
        }
    }
};

#endif /* clahe_h */
