#include "ofApp.h"

// CORE
//--------------------------------------------------------------
void ofApp::setup(){
    ofSetLogLevel(OF_LOG_VERBOSE);
    ofSetFullscreen(true);
    ofSetBackgroundColor(0);
    #ifdef _USE_LIVE_VIDEO
//        grabber.setup(1920, 1080);
//        grabber.setup(1024, 768);
        blackCam.setup(1920, 1080, 25, 0, ofxBlackMagic::LOW_LATENCY);
    #else
        video.load("vids/motinas_multi_face_fast.mp4"); video.play();
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
//        grabber.update(); newFrame = grabber.isFrameNew();
        newFrame = blackCam.update();
    #else
        video.update(); newFrame = video.isFrameNew();
    #endif
    
    if(newFrame){
        
        // capture
        #ifdef _USE_LIVE_VIDEO
//            srcImg.setFromPixels(grabber.getPixels());
            srcImg.setFromPixels(blackCam.getColorPixels());
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
            
            // misc var
            agedImages.clear();
            lockedFaceFound = false;
            vector<Grid::PixelsItem> pis;
            
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
                    // update averages values
                    faceUtils.updateLandmarksAverage(instance);
                    // Push elements to the grid
//                    pushToGrid();
                    if (showGrid) {
                        for (int i=0; i<5; i++) {
                            ofImage img0 = faceUtils.getLandmarkImg(srcImg, instance, 0, faceImgSize/4, 80);
                            ofImage img1 = faceUtils.getLandmarkImg(srcImg, instance, 1, faceImgSize/4, 30);
                            ofImage img2 = faceUtils.getLandmarkImg(srcImg, instance, 2, faceImgSize/4, 30);
                            ofImage img3 = faceUtils.getLandmarkImg(srcImg, instance, 3, faceImgSize/4, 40);
                            ofImage img4 = faceUtils.getLandmarkImg(srcImg, instance, 4, faceImgSize/4, 40);
                            if (img0.isAllocated()) pis.push_back(Grid::PixelsItem(img0.getPixels(), Grid::face));
                            if (img1.isAllocated()) pis.push_back(Grid::PixelsItem(img1.getPixels(), Grid::leftEye));
                            if (img2.isAllocated()) pis.push_back(Grid::PixelsItem(img2.getPixels(), Grid::rightEye));
                            if (img3.isAllocated()) pis.push_back(Grid::PixelsItem(img3.getPixels(), Grid::nose));
                            if (img4.isAllocated()) pis.push_back(Grid::PixelsItem(img4.getPixels(), Grid::mouth));
                        }
                    }
                    //
                    lockedFaceFound = true;
                }
                
            }
            
            // If the locked face is not found
            if (!lockedFaceFound) {
                // Stop gridTimer
                timerShowGrid.reset(), timerShowGrid.stopTimer();
                showGrid = false;
            } else if (!showGrid) {
                // Start gridTimer
                timerShowGrid.startTimer();
                // set age to Lock to default
                ageToLock = timeToLock/secondToAgeCoef;
            }
            
            // If the face locked does not exist anymore
            if ( !tracker.faceRectanglesTracker.existsCurrent(faceLockedLabel) ) {
                faceLocked = false;
                vidRecorder.close();
            }
            
            // Grid
            if (showGrid) {
                // update
                grid.updatePixels(pis);
                // easing of alpha
                int s = grid.GridElements.size();
                if (s) {
                    for (int i=0; i<s; i++) {
                        float t = 2.f, d = .5;
                        auto startTime = initTimeGrid+(float)i/s*t, endTime = initTimeGrid+(float)i/s*t + d, now = ofGetElapsedTimef();
                        grid.GridElements.at(i).setAlpha( ofxeasing::map_clamp(now, startTime, endTime, 0, 255, &ofxeasing::linear::easeOut) );
                    }
                }
            }
            
            // sort aged Images vector
            std::sort(agedImages.begin(), agedImages.end(), byFirst());
            
        } else {
            updateVideos();
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    
    //
    ofImage alignedFace;
    
    // Draw source image + tracker
    ofPushMatrix();
        ofScale(1/srcImgScale*sceneScale, 1/srcImgScale*sceneScale);
        srcImg.draw(0, 0);
        // Draw tracker landmarks
        tracker.drawDebug();
        // Draw faces
        int x = 0;
        for(auto agedImage : agedImages) {
            agedImage.second.draw(x, srcImg.getHeight());
            if (agedImage.first == faceLockedLabel) {
                ofPushStyle();
                    ofNoFill();
                    ofSetColor(255,0,0);
                    ofDrawRectangle(x, srcImg.getHeight(), faceImgSize, faceImgSize);
                ofPopStyle();
                alignedFace = agedImage.second;
            }
            x += faceImgSize;
        }
    ofPopMatrix();
    
    // Draw Output
    ofPushStyle();
        ofSetColor(255,0,0);
        ofDrawRectangle(outputPositionX-2, outputPositionY-2, outputSizeW+4, outputSizeH+4);
    ofPopStyle();
    ofPushMatrix();
        ofTranslate(outputPositionX, outputPositionY);
        if (isIdle) drawVideos();
        else if (!lockedFaceFound) {
            srcImg.draw(0, 0, outputSizeW, outputSizeW);
            ofPushMatrix();
                ofScale(outputSizeW/srcImg.getWidth(), outputSizeW/srcImg.getWidth());
                tracker.drawDebug();
            ofPopMatrix();
        } else alignedFace.draw(0, 0, outputSizeW, outputSizeW);
        if (showGrid) grid.draw(0,0);
    ofPopMatrix();
    
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
    blackCam.close();
//    if (live.isLoaded()) live.stop();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == ' ') faceLocked = false;
    
    if(key == 'f'){
        fullScreen = !fullScreen;
        if(fullScreen) ofSetFullscreen(true);
        else ofSetFullscreen(false);
    }
    
}

// VAR
//--------------------------------------------------------------
void ofApp::initVar(){
    // general
    isIdle = false, facesFound = false, lockedFaceFound = false,  faceLocked = false, showGrid = false, showText = false;
    outputPositionX = 600, outputPositionY = 600, outputSizeW = 192, outputSizeH = 192;
    
    sceneScale = 1;
    
    // capture
    srcImgScale = 1;
    
    // timers
    timeToSleep = 2000; // time before entering idle mode
    timeToWake = 2000; // time before exiting idle mode
    timeToLock = 2000, // Time before locking up a face
    timeToShowGrid = 2000; // time before grid
    timeToShowText = 20000; // time before showing the text
    timeToShowNextText = 5000; // time before showing the next bunch of text

    // tracker
    trackerFaceDetectorImageSize = 2000000, trackerLandmarkDetectorImageSize = 2000000;
    secondToAgeCoef = 70, ageToLock = timeToLock / secondToAgeCoef;
    trackerIsThreaded = true;
    //
    faceImgSize = 256, desiredLeftEyeX = 0.39, desiredLeftEyeY = 0.43, faceScaleRatio = 2.6;
    faceRotate = true, faceConstrain = true;

    // filter
    filterClaheClipLimit = 2;
    srcImgIsCropped = true, srcImgIsFiltered = true, srcImgIsColored = false;
    
    // video recording + playing
    faceVideoPath = "output"; videosCount = 64;

    // grid
    showGrid = false, showGridElements = false, gridIsSquare = true;
//    gridWidth = 24, gridHeight = 24, gridRes = 16, gridMinSize = 0, gridMaxSize = 12;
    gridWidth = 12, gridHeight = 12, gridRes = 16, gridMinSize = 0, gridMaxSize = 12;
    
    
}

// GUI
//--------------------------------------------------------------
void ofApp::drawGui(){
    gui.begin();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    if (ImGui::CollapsingHeader("General", false)) {
        ImGui::SliderFloat("Scale Source", &srcImgScale, .1, 3);
        ImGui::SliderFloat("Scale Scene", &sceneScale, .1, 2);
        ImGui::SliderInt("filterClaheClipLimit", &filterClaheClipLimit, 0, 6);
        ImGui::Checkbox("Filtered", &srcImgIsFiltered); ImGui::SameLine();
        ImGui::Checkbox("Colored", &srcImgIsColored); ImGui::SameLine();
        ImGui::Checkbox("Cropped", &srcImgIsCropped);
    }
    if (ImGui::CollapsingHeader("Tracker", false)) {
        ImGui::Columns(2);
        //
        ImGui::Text("Detector Size");
        int maxSize = int(srcImg.getWidth()*srcImg.getHeight())*20;
        if (ImGui::SliderInt("Face", &trackerFaceDetectorImageSize, 100000, maxSize, "%.0f px")) tracker.setFaceDetectorImageSize(trackerFaceDetectorImageSize);
        if (ImGui::SliderInt("Landmark", &trackerLandmarkDetectorImageSize, 100000, maxSize*2, "%.0f px")) tracker.setLandmarkDetectorImageSize(trackerLandmarkDetectorImageSize);
        if (ImGui::Checkbox("Threaded", &trackerIsThreaded)) tracker.setThreaded(trackerIsThreaded);
        ImGui::NextColumn();
        //
        ImGui::Text("Face Aligning");
        ImGui::SliderInt("Img Size", &faceImgSize, 64, 512, "%.0f px");
        ImGui::SliderFloat("desiredLeftEyeX", &desiredLeftEyeX, 0, .5, "%.02f %%");
        ImGui::SliderFloat("desiredLeftEyeY", &desiredLeftEyeY, 0, .5, "%.02f %%");
        ImGui::SliderFloat("scale amount", &faceScaleRatio, 1, 5, "%.0f");
        ImGui::Checkbox("Rotate", &faceRotate);
        ImGui::Checkbox("Constrain only", &faceConstrain);
        //
        ImGui::Columns(1);
        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Timers", false)) {
        ImGui::Columns(3);
        //
        ImGui::Text("Idle");
        if (ImGui::SliderInt("Sleep", &timeToSleep, 1000, 20000, "%.0f ms")) timerSleep.setTimer(timeToSleep);
        if (ImGui::SliderInt("Wake", &timeToWake, 1000, 20000, "%.0f ms")) timerWake.setTimer(timeToWake);
        ImGui::NextColumn();
        //
        ImGui::Text("Lock");
        if (ImGui::SliderInt("Lock", &timeToLock, 1000, 20000, "%.0f ms")) ageToLock = timeToLock / secondToAgeCoef;
        if (ImGui::SliderInt("Coef", &secondToAgeCoef, 10, 120)) ageToLock = timeToLock / secondToAgeCoef;
        ImGui::NextColumn();
        //
        ImGui::Text("Other");
        if (ImGui::SliderInt("Grid", &timeToShowGrid, 1000, 20000, "%.0f ms")) timerShowGrid.setTimer(timeToShowGrid);
        if (ImGui::SliderInt("Text", &timeToShowText, 1000, 20000, "%.0f ms")) timerShowText.setTimer(timeToShowText);
        //
        ImGui::Columns(1);
        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Grid ", false)) {
        ImGui::Columns(3);
        //
        ImGui::Text("Shape");
        ImGui::SliderInt("Width", &gridWidth, 1, 24);
        ImGui::SliderInt("Height", &gridHeight, 1, 24);
        ImGui::NextColumn();
        //
        ImGui::Text("Size");
        ImGui::InputInt("Res", &gridRes, 8);
        ImGui::SliderInt("maxSize", &gridMaxSize, 1, 12);
        ImGui::NextColumn();
        //
        ImGui::Text("Options");
        ImGui::Checkbox("grid Is Square", &gridIsSquare);
        ImGui::Checkbox("show Grid", &showGrid);
        ImGui::NextColumn();
        //
        ImGui::Columns(1);
        ImGui::Separator();
        if (ImGui::Button("Refresh")) grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare); ImGui::SameLine();
        if (ImGui::Button("Randomize")) randomizeGrid();
        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Output", false)) {
        ImGui::Columns(2);
        //
        ImGui::Text("Position");
        ImGui::DragInt("X", &outputPositionX, 1, 0, ofGetWindowWidth(), "%.0f ms");
        ImGui::DragInt("Y", &outputPositionY, 1, 0, ofGetWindowHeight(), "%.0f ms");
        ImGui::NextColumn();
        //
        ImGui::Text("Size");
        ImGui::DragInt("W", &outputSizeW, 1, 1, ofGetWindowWidth()-outputPositionX, "%.0f px");
        ImGui::DragInt("H", &outputSizeH, 1, 1, ofGetWindowHeight()-outputPositionY, "%.0f px");
        ImGui::NextColumn();
    }
    if (ImGui::CollapsingHeader("Playback", false)) {
        ImGui::Text("Overall");
        ImGui::SliderInt("videosCount", &videosCount, 1, 144);
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
    // Stop Timer
    timerSleep.stopTimer();
    // Add the wake time to the age to lock
    ageToLock = timeToLock/secondToAgeCoef + timeToWake/secondToAgeCoef;
    // Start Videos
    loadVideos();
    playVideos();
    // Adjust volumes
    isIdle = true;
}

void ofApp::timerWakeFinished(ofEventArgs &e) {
    // Stop Timer
    timerWake.stopTimer();
    // Stop Videos
    stopVideos();
    // Adjust volumes
    isIdle = false;
}

void ofApp::timerShowGridFinished(ofEventArgs &e) {
    // Stop Timer
    timerShowGrid.stopTimer();
    timerShowText.reset(), timerShowText.startTimer();
    // Init and show grid
    grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare);
    // Save the time
    initTimeGrid = ofGetElapsedTimef();
    showGrid = true;
}

void ofApp::timerShowTextFinished(ofEventArgs &e) {
    timerShowText.stopTimer();
}


// GRID
//--------------------------------------------------------------
void ofApp::randomizeGrid(){
    // grid
    if (ofRandom(1)>.4) gridWidth = 6, gridHeight = 6, gridRes = 32, gridMaxSize = ofRandom(6);
    else gridWidth = 12, gridHeight = 12, gridRes = 16, gridMaxSize = ofRandom(12);
    gridIsSquare = (ofRandom(1)>.5) ? true : false;
}


// VIDEO PLAYER
//--------------------------------------------------------------
void ofApp::loadVideos() {
    videosDir.allowExt("mov");
    videosDir.listDir(faceVideoPath);
    videosDir.sort();
//    if(videosDir.size()>videosCount) videosVector.resize(videosCount);
//    else  videosVector.resize(videosDir.size());
    // iterate through the files and load them into the vector
//    int j = 0;
    // reverse browsing the videosDir
//    for(int i=(int)videosDir.size()-1; i>=0 && j<videosVector.size(); i--){
//        if ( videosDir.getFile(i).getSize()>1000000 ) {
//            videosVector[j].load(videosDir.getPath(i));
//            videosVector[j].setLoopState(OF_LOOP_PALINDROME);
////            videosVector[j].setSpeed((int)ofGetFrameRate());
//            videosVector[j].play();
//            j++;
//        }
//    }
    videosVector.resize(videosCount);
    int i = videosDir.size()-1, j = 0;
    while (j<videosVector.size()){
        if ( videosDir.getFile(i).getSize()>100000 ) {
            videosVector[j].load(videosDir.getPath(i));
            videosVector[j].setLoopState(OF_LOOP_PALINDROME);
//            videosVector[j].play();
            j++;
        }
        i = (i>0) ? i-1 : videosDir.size()-1;
    }
}

//--------------------------------------------------------------
void ofApp::drawVideos() {
    int columns = sqrt(videosCount);
    if (videosVector.size()) {
        for(int i = 0; i < pow(columns,2); i++) {
            videosVector[i%videosVector.size()].draw((i%columns)*outputSizeW/columns, (i/columns)*outputSizeH/columns, outputSizeW/columns, outputSizeW/columns);
        }
    }
    cout << videosVector.size() <<endl;
}

//--------------------------------------------------------------
void ofApp::updateVideos() {
    if (videosVector.size()) for(int i = 0; i < videosVector.size(); i++) videosVector[i].update();
}

//--------------------------------------------------------------
void ofApp::playVideos() {
    if (videosVector.size()) for(int i = 0; i < videosVector.size(); i++) videosVector[i].play();
}

//--------------------------------------------------------------
void ofApp::stopVideos() {
    if (videosVector.size()) for(int i = 0; i < videosVector.size(); i++) videosVector[i].stop(), videosVector[i].close();
}

