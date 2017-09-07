//
//  FaceUtils.h
//  Facetracker_face_alignment
//
//  Created by Guillaume on 21.08.17.
//
//  http://www.pyimagesearch.com/2017/05/22/face-alignment-with-opencv-and-python/
//  https://github.com/MasteringOpenCV/code/blob/master/Chapter8_FaceRecognition/preprocessFace.cpp
//
// Improvment:
// . Get avg box width ratio
// . Create a proper class including the srcImg and FacetrackerInstance
// . Add an alias system for landmarks (faceOutline, leftEye, rightEye, mouth, nose)

#ifndef FaceUtils_h
#define FaceUtils_h

class FaceUtils {
    
public:
    
    FaceUtils(){
        landmarksPosHistory.assign(5,deque<ofVec2f>());
        avgLandmarksPos.assign(5,ofVec2f(0,0));
    };
    ~FaceUtils(){};
    
    //--------------------------------------------------------------
    void updateLandmarksAverage(ofxFaceTracker2Instance &instance){
        // get face bounding box
        ofRectangle rect = instance.getBoundingBox();
        
        // add pos
        for (unsigned i = 0; i < landmarksPosHistory.size(); i++) {
            // add
            ofVec2f pos;
            switch(i) {
                case 0: pos = getPct(instance.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::FACE_OUTLINE), rect); break;
                case 1: pos = getPct(instance.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::LEFT_EYE), rect); break;
                case 2: pos = getPct(instance.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::RIGHT_EYE), rect); break;
                case 3: pos = getPct(instance.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::OUTER_MOUTH), rect); break;
                case 4: pos = getPct(instance.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::NOSE_BASE), rect); break;
            }
            landmarksPosHistory.at(i).push_back(pos);
            // remove first element if necessary
            if ( landmarksPosHistory.at(i).size() > landmarksPosHistoryLength ) landmarksPosHistory.at(i).pop_front();
        }
        
        // calculate avg position
        for (int i = 0; i < avgLandmarksPos.size(); i++) {
            ofVec2f t = ofVec2f(0,0);
            for (unsigned j=0; j<landmarksPosHistory.at(i).size(); j++) t = t + landmarksPosHistory[i][j];
            avgLandmarksPos.at(i) = t / landmarksPosHistory.at(i).size();
        }

    }

    
    //--------------------------------------------------------------
    ofImage getAlignedFace(ofImage &srcImg, ofxFaceTracker2Instance &instance,
                   int desiredFaceWidth, float desiredLeftEyeX = 0.4, float desiredLeftEyeY = 0.4, float desiredScale = 2,
                   bool rotate = true, bool constrain = false) {
        
        // square box
        int desiredFaceHeight = desiredFaceWidth;
        bool outOfBound = false;
        
        // get face bounding box
        ofRectangle rect = instance.getBoundingBox();
        rect.setHeight(rect.getWidth());

        // clamp scale
        float scaleX = ofClamp(desiredScale, 0, srcImg.getWidth()/rect.getWidth());
        float scaleY = ofClamp(desiredScale, 0, srcImg.getHeight()/rect.getHeight());
        rect.scaleFromCenter(min(scaleX, scaleY));
        
        int x = rect.x;
        int y = rect.y;
        // if scaled rect outside of img boundaries
        if ( x < 0 || x > srcImg.getWidth()-rect.width || y < 0 || y > srcImg.getHeight()-rect.height ) {
            // clamp x, y to img size
            x = ofClamp(x, 0, srcImg.getWidth()-rect.width);
            y = ofClamp(y, 0, srcImg.getHeight()-rect.height);
            // mark as oob
            outOfBound = true;
        }
        
        // Crop image
        cv::Mat mat = ofxCv::toCv(srcImg);
        cv::Mat croppedMat = mat(cv::Rect(x, y, rect.width, rect.height));
        
        // Get eyes center coordinate within the face bounding box
        ofVec3f leftEyeCenter = instance.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::LEFT_EYE).getCentroid2D() - rect.getTopLeft();
        ofVec3f rightEyeCenter = instance.getLandmarks().getImageFeature(ofxFaceTracker2Landmarks::RIGHT_EYE).getCentroid2D() - rect.getTopLeft();
        
        // Get the angle between the 2 eyes.
        float dY = rightEyeCenter.y - leftEyeCenter.y;
        float dX = rightEyeCenter.x - leftEyeCenter.x;
        double len = sqrt(dX*dX + dY*dY);
        
        // rotate
        float angle = (rotate) ? ofRadToDeg(atan2(dY, dX)) : 0;
        
        // Get the center between the 2 eyes.
        cv::Point2f eyesCenter = cv::Point2f( (leftEyeCenter.x + rightEyeCenter.x) * 0.5f, (leftEyeCenter.y + rightEyeCenter.y) * 0.5f );
        // Hand measurements shown that the left eye center should ideally be at roughly (0.19, 0.14) of a scaled face image.
        const double DESIRED_RIGHT_EYE_X = (1.0f - desiredLeftEyeX);
        
        // Get the amount we need to scale the image to be the desired fixed size we want.
        double desiredLen = (DESIRED_RIGHT_EYE_X - desiredLeftEyeX) * desiredFaceWidth;
        double scale = desiredLen / len;
        // Get the transformation matrix for rotating and scaling the face to the desired angle & size.
        cv::Mat rot_mat = cv::getRotationMatrix2D(eyesCenter, angle, scale);
        // Shift the center of the eyes to be the desired center between the eyes.
        rot_mat.at<double>(0, 2) += desiredFaceWidth * 0.5f - eyesCenter.x;
        rot_mat.at<double>(1, 2) += desiredFaceHeight * desiredLeftEyeY - eyesCenter.y;
        
        // Rotate and scale and translate the image to the desired angle & size & position!
        // Note that we use 'w' for the height instead of 'h', because the input face has 1:1 aspect ratio.
        cv::Mat warped = cv::Mat(desiredFaceHeight, desiredFaceWidth, CV_8U, cvScalar(128)); // Clear the output image to a default grey.
        warpAffine(croppedMat, warped, rot_mat, warped.size());
        croppedMat = warped;
    
        ofImage img;
        ofPixels px;
        ofxCv::copy(croppedMat, px);
        img.setFromPixels(px);
        img.resize(desiredFaceWidth, desiredFaceHeight);
        if ( !constrain ) return img;
        else if ( !outOfBound ) return img;
    }
    
    
    //--------------------------------------------------------------
    ofPixels getLandmarkPixels(ofImage &srcImg, const ofxFaceTracker2Instance &instance,
                           int index, int desiredWidth, int sizePct = 30, int offset = 0, bool useAvg = true){
        
        // get face bounding box
        ofRectangle rect = instance.getBoundingBox();
        
        // retrieve landmark avg position
        ofVec2f landmarkAvg = avgLandmarksPos.at(index);
        
        // calculate landmark position relative to srcImg
        int x = rect.x + rect.width * landmarkAvg.x / 100 + offset;
        int y = rect.y + rect.height * landmarkAvg.y / 100 + offset;
        
        // calculate w / h
        int w = rect.width*sizePct / 100;
        
        // clamp x, y to img size
        x = ofClamp(x-w/2, 0, srcImg.getWidth()-w);
        y = ofClamp(y-w/2, 0, srcImg.getHeight()-w);

        // crop
        ofImage img;
        img.cropFrom(srcImg, x, y, w, w);
        img.resize(desiredWidth, desiredWidth);
        return img.getPixels();
    
    }

private:
    
    // Total landmarks position in pct of the img (pctX, pctY).
    // Order: faceOutline, leftEye, rightEye, mouth, nose
    vector<deque<ofVec2f>> landmarksPosHistory;
    int landmarksPosHistoryLength = 10;
    // Average landmarks position in pct of the img (pctX, pctY).
    // Order: faceOutline, leftEye, rightEye, mouth, nose
    vector<ofVec2f> avgLandmarksPos;    

    //--------------------------------------------------------------
    // return a point position (in percent) within a rectangle
    ofVec2f getPct(ofPolyline polyline, const ofRectangle &boundingRect) {
        // Check if the polyline is not empty
        if (polyline.size()!=0) {
            // Get polyline center
            ofVec3f polylineCenter = polyline.getCentroid2D();
            polylineCenter = polylineCenter - boundingRect.getTopLeft();
            //
            float pctX = 100*polylineCenter.x / boundingRect.getWidth();
            float pctY = 100*polylineCenter.y / boundingRect.getHeight();
            //
            return ofVec2f(pctX, pctY);
        }
    }
    
};

#endif /* FaceUtils_h */
