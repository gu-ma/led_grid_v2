#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    ofSetBackgroundColor(0);
    #ifdef _USE_LIVE_VIDEO
        grabber.setup(1280, 720);
    #else
        video.load("vids/motinas_multi_face_fast.mp4");
        video.play();
    #endif
    initVar();
    initTracker();
    initVidRecorder();
    initTimers();

}

//--------------------------------------------------------------
void ofApp::update(){
    
    // update
    newFrame = false;
    #ifdef _USE_LIVE_VIDEO
        grabber.update(); newFrame = grabber.isFrameNew();
    #else
        video.update(); newFrame = video.isFrameNew();
    #endif
    
    if(newFrame){
        
        // capture
        #ifdef _USE_LIVE_VIDEO
            srcImg.setFromPixels(grabber.getPixels());
        #else
            srcImg.setFromPixels(video.getPixels());
        #endif
        
        // Resize, mirror, crop, filter
        srcImg.resize(srcImg.getWidth()*srcImgScale, srcImg.getHeight()*srcImgScale);
        srcImg.mirror(false, true);
        if (srcImgIsCropped) srcImg.crop((srcImg.getWidth()-srcImg.getHeight())/2, 0, srcImg.getHeight(), srcImg.getHeight());
        if (srcImgIsFiltered) clahe.filter(srcImg, srcImg, filterClaheClipLimit, srcImgIsColored);
        srcImg.update();
        
        // Update Tracker
        tracker.update(srcImg.getPixels());
        
        // Sleep wake logic
        if ( tracker.size() == 0 ) {
            // Faces were found before
            if (facesFound) {
                timerSleep.reset(), timerSleep.startTimer(); // reset and start sleep timer
                timerWake.stopTimer(), timerShowGrid.stopTimer(); // stop the timers
                facesFound = false; // reset bools
            }
        } else if (!facesFound) {
            // It comes from idle mode
            if (isIdle) timerWake.reset(), timerWake.startTimer(); // reset and start the wake timer
            timerSleep.stopTimer(); // stop the sleep timer
            facesFound = true;
        }
        
        // If it's not in Idle mode
        if (!isIdle) {
            //
            agedImages.clear();
            lockedFaceFound = false;
            for(auto instance : tracker.getInstances()) {
                
                if ( instance.getAge() > ageToLock && !faceLocked ) {
                    //
                    faceLockedLabel = instance.getLabel();
                    faceLocked = true;
                    // start Video recording
                    vidRecorder.setup(faceVideoPath+"/"+ofGetTimestampString("%Y%m%d-%H%M%S")+"-"+ofToString(faceLockedLabel)+".mov", faceImgSize, faceImgSize, (int)ofGetFrameRate() );
                    vidRecorder.start();
                    // faceUtils.resetLandmarksAverage();
                }
                
                // Get aligned face
                ofImage img = faceUtils.getAlignedFace(srcImg, instance, faceImgSize, desiredLeftEyeX, desiredLeftEyeY, faceScaleRatio, faceRotate, faceConstrain);
                // save to agedImages vector
                if ( img.isAllocated() ) agedImages.push_back(IndexedImages(instance.getLabel(),img));
                
                // if the instance is the face locked
                if ( img.isAllocated() && instance.getLabel()==faceLockedLabel) {
                    // add image to vid recorder
                    if (vidRecorder.isInitialized()) vidRecorder.addFrame(img.getPixels());
                    //
                    lockedFaceFound = true;
                }
            }
            
            // If the locked face is not visible
            if (!lockedFaceFound) {
                timerShowGrid.reset(), timerShowGrid.stopTimer();
                showGrid = false;
            } else if (!showGrid) {
                // Start gridTimer
                timerShowGrid.startTimer();
            }
            
            // If the face locked does not exist anymore
            if ( !tracker.faceRectanglesTracker.existsCurrent(faceLockedLabel) ) {
                faceLocked = false;
                vidRecorder.close();
            }
            
            
            // sort aged Images vector
            std::sort(agedImages.begin(), agedImages.end(), byFirst());
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    
    // Draw source image + tracker
    ofPushMatrix();
        ofScale(1/srcImgScale, 1/srcImgScale);
        srcImg.draw(0, 0);
        // Draw tracker landmarks
        tracker.drawDebug();
    ofPopMatrix();
    
    if (!isIdle) {
        
        // Draw faces
        int x = 0;
        for(auto agedImage : agedImages) {
            ofImage img = agedImage.second;
            if (img.isAllocated()) img.draw(x, 400);
            if (agedImage.first == faceLockedLabel) {
                ofPushStyle();
                    ofNoFill();
                    ofSetColor(255,0,0);
                    ofDrawRectangle(x, 400, faceImgSize, faceImgSize);
                ofPopStyle();
            }
            x += faceImgSize;
        }
        
        //
        for(auto instance : tracker.getInstances()) {
            if ( instance.getLabel() == faceLockedLabel ) {
                faceUtils.updateLandmarksAverage(instance);
                ofImage img0 = faceUtils.getLandmarkImg(srcImg, instance, 0, faceImgSize, 80);
                ofImage img1 = faceUtils.getLandmarkImg(srcImg, instance, 1, faceImgSize, 30);
                ofImage img2 = faceUtils.getLandmarkImg(srcImg, instance, 2, faceImgSize, 30);
                ofImage img3 = faceUtils.getLandmarkImg(srcImg, instance, 3, faceImgSize, 40);
                ofImage img4 = faceUtils.getLandmarkImg(srcImg, instance, 4, faceImgSize, 40);
                if (img0.isAllocated()) img0.draw(0, 400 + faceImgSize);
                if (img1.isAllocated()) img1.draw(faceImgSize, 400 + faceImgSize);
                if (img2.isAllocated()) img2.draw(faceImgSize*2, 400 + faceImgSize);
                if (img3.isAllocated()) img3.draw(faceImgSize*3, 400 + faceImgSize);
                if (img4.isAllocated()) img4.draw(faceImgSize*4, 400 + faceImgSize);
            }
        }

    }
    
    // Draw text UI
    ofDrawBitmapStringHighlight("Framerate : " + ofToString(ofGetFrameRate()), 10, 20);
    ofDrawBitmapStringHighlight("Tracker thread framerate : " + ofToString(tracker.getThreadFps()), 10, 40);
    
    //
    if (!isIdle) {
        if (!facesFound) ofDrawBitmapStringHighlight("Idle in: " + ofToString(timerSleep.getTimeLeftInSeconds()), 10, 80, ofColor(200,0,0), ofColor(255));
        //
        if (faceLocked) ofDrawBitmapStringHighlight("LOCKED - Face label: " + ofToString(faceLockedLabel), 10, 100, ofColor(150,0,0), ofColor(255));
        //
        if (lockedFaceFound) {
            string grid = (showGrid) ? "GRID" : "Time to Grid: " + ofToString(timerShowGrid.getTimeLeftInSeconds());
            ofDrawBitmapStringHighlight(grid, 10, 120, ofColor(100,0,0), ofColor(255));
        }
    } else {
        string idle = (!facesFound) ? "IDLE" : "IDLE - Wake in: " + ofToString(timerWake.getTimeLeftInSeconds());
        ofDrawBitmapStringHighlight(idle, 10, 80, ofColor(200,0,0), ofColor(255));
    }

    // Draw GUI
    drawGui();
}

//--------------------------------------------------------------
void ofApp::exit(){
    //
    ofRemoveListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::vidRecordingComplete);
    vidRecorder.close();
//    blackCam.close();
//    if (live.isLoaded()) live.stop();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == ' ') faceLocked = false;
}

// VAR
//--------------------------------------------------------------
void ofApp::initVar(){
    // general
    isIdle = false, facesFound = false, lockedFaceFound = false,  faceLocked = false, showGrid = false, showText = false;
    
    // capture
    srcImgScale = 1;
    
    // timers
    timeToSleep = 5000; // time before entering idle mode
    timeToWake = 2000; // time before exiting idle mode
    timeToLock = 2000, // Time before locking up a face
    timeToShowGrid = 1000; // time before grid
    timeToShowText = 20000; // time before showing the text
    timeToShowNextText = 5000; // time before showing the next bunch of text

    // tracker
    trackerFaceDetectorImageSize = 2000000, trackerLandmarkDetectorImageSize = 2000000;
    secondToAgeCoef = 80, ageToLock = timeToLock / secondToAgeCoef;
    trackerIsThreaded = false;
    //
    faceImgSize = 128, desiredLeftEyeX = 0.39, desiredLeftEyeY = 0.43, faceScaleRatio = 2.6;
    faceRotate = true, faceConstrain = true;


    // filter
    filterClaheClipLimit = 2;
    srcImgIsCropped = true, srcImgIsFiltered = true, srcImgIsColored = false;
    
    // video recording
    faceVideoPath = "output";
    
}

// GUI
//--------------------------------------------------------------
void ofApp::drawGui(){
    // GUI
    gui.begin();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Scale Source", &srcImgScale, .1, 3);
    if (ImGui::CollapsingHeader("filter", false)) {
        ImGui::SliderInt("filterClaheClipLimit", &filterClaheClipLimit, 0, 6);
        ImGui::Checkbox("Cropped", &srcImgIsCropped);
        ImGui::Checkbox("Filter", &srcImgIsFiltered);
        ImGui::Checkbox("Colored", &srcImgIsColored);
    }
    if (ImGui::CollapsingHeader("tracker", false)) {
        int maxSize = int(srcImg.getWidth()*srcImg.getHeight())*20;
        if (ImGui::SliderInt("Face Detector Size", &trackerFaceDetectorImageSize, 100000, maxSize)) tracker.setFaceDetectorImageSize(trackerFaceDetectorImageSize);
        if (ImGui::SliderInt("Landmark Detector Size", &trackerLandmarkDetectorImageSize, 100000, maxSize*2)) tracker.setLandmarkDetectorImageSize(trackerLandmarkDetectorImageSize);
        if (ImGui::Checkbox("Threaded", &trackerIsThreaded)) tracker.setThreaded(trackerIsThreaded);
        ImGui::Text("Face");
        ImGui::SliderInt("Img Size", &faceImgSize, 0, 500);
        ImGui::SliderFloat("desiredLeftEyeX", &desiredLeftEyeX, 0, .5);
        ImGui::SliderFloat("desiredLeftEyeY", &desiredLeftEyeY, 0, .5);
        ImGui::SliderFloat("scale amount", &faceScaleRatio, 1, 5);
        ImGui::Checkbox("Rotate", &faceRotate);
        ImGui::Checkbox("Constrain only", &faceConstrain);
    }
    if (ImGui::CollapsingHeader("timers", false)) {
        if (ImGui::SliderInt("Time to sleep", &timeToSleep, 1000, 20000)) timerSleep.setTimer(timeToSleep);
        if (ImGui::SliderInt("Time to wake", &timeToWake, 1000, 20000)) timerWake.setTimer(timeToWake);
        ImGui::Text("------");
        if (ImGui::SliderInt("Time to lock", &timeToLock, 1000, 20000)) ageToLock = timeToLock / secondToAgeCoef;
        ImGui::SliderInt("Sec to age coef", &secondToAgeCoef, 50, 150);
        ImGui::Text("------");
        if (ImGui::SliderInt("Time to grid", &timeToShowGrid, 1000, 20000)) timerShowGrid.setTimer(timeToShowGrid);
        if (ImGui::SliderInt("Time to text", &timeToShowText, 1000, 20000)) timerShowText.setTimer(timeToShowText);
    }
    gui.end();
}


// VIDEO RECORDER
//--------------------------------------------------------------
void ofApp::initVidRecorder() {
    vidRecorder.setVideoCodec("mjpeg");
    vidRecorder.setVideoBitrate("1000k");
    //    vidRecorder.setAudioCodec("mp3");
    //    vidRecorder.setAudioBitrate("320k");
    ofAddListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::vidRecordingComplete);
}

void ofApp::vidRecordingComplete(ofxVideoRecorderOutputFileCompleteEventArgs& args){
    cout << "The recorded video file is now complete." << endl;
}


// TRACKERS
//--------------------------------------------------------------
void ofApp::initTracker() {
    tracker.setup("../../../../models/shape_predictor_68_face_landmarks.dat");
    tracker.setFaceDetectorImageSize(trackerFaceDetectorImageSize);
    tracker.setLandmarkDetectorImageSize(trackerLandmarkDetectorImageSize);
    tracker.setThreaded(trackerIsThreaded);
}


// TIMERS
//--------------------------------------------------------------
void ofApp::initTimers() {
    timerSleep.setup(timeToSleep, false), timerWake.setup(timeToWake, false);
    timerShowGrid.setup(timeToShowGrid, false), timerShowText.setup(timeToShowText, false);
    ofAddListener(timerSleep.TIMER_REACHED, this, &ofApp::timerSleepFinished), ofAddListener(timerWake.TIMER_REACHED, this, &ofApp::timerWakeFinished);
    ofAddListener(timerShowGrid.TIMER_REACHED, this, &ofApp::timerShowGridFinished), ofAddListener(timerShowText.TIMER_REACHED, this, &ofApp::timerShowTextFinished);
}

void ofApp::timerSleepFinished(ofEventArgs &e) {
    //
//    timerSleep.stopTimer();
    // Add the wake time to the age to lock
    ageToLock = timeToLock/secondToAgeCoef + timeToWake/secondToAgeCoef;
    // Start Videos
    // Adjust volumes
    isIdle = true;
}

void ofApp::timerWakeFinished(ofEventArgs &e) {
    //
//    timerWake.stopTimer();
    // set age to Lock to default
    ageToLock = timeToLock/secondToAgeCoef;
    // Stop Videos
    // Adjust volumes
    isIdle = false;
}

void ofApp::timerShowGridFinished(ofEventArgs &e) {
    //
//    timerShowGrid.stopTimer();
    showGrid = true;
    timerShowText.reset(), timerShowText.startTimer();
}

void ofApp::timerShowTextFinished(ofEventArgs &e) {
    //
    timerShowText.stopTimer();
}
