<<<<<<< HEAD
#include "ofApp.h"

// CORE
//--------------------------------------------------------------
void ofApp::setup(){
//    ofSetLogLevel(OF_LOG_VERBOSE);
    //
    ofSetFullscreen(true);
    ofSetBackgroundColor(0);
    ofTrueTypeFont::setGlobalDpi(72);
    
    #ifdef _USE_LIVE_VIDEO
        #ifdef _USE_BLACKMAGIC
            blackCam.setup(1920, 1080, 30);
        #else
            grabber.setDeviceID(1); grabber.setup(1024, 576);
        #endif
    #else
        video.load("vids/motinas_multi_face_frontal.mp4"); video.play();
    #endif
    initVar();
    loadTextFile();
    log.start();
    initTracker();
    initVidRecorder();
    initTimers();
    live.setup();
    initLive();
    //
    displayFbo.allocate(displaySizeW, displaySizeH);
    guiFbo.allocate(ofGetWidth(), ofGetHeight());
//    glitch.setup(&displayFbo);



}

//--------------------------------------------------------------
void ofApp::update(){
    
    //
    live.update();
    refreshLive();
    
    //
    noiseOfTime = ofSignedNoise(ofGetElapsedTimef()/noiseFreq,0);
    if (int(ofGetElapsedTimef())%glitchFreq == 0) {
        float rand = ofRandom(0, abs(noiseOfTime));
        if (!glitchOn) {
            if (rand>glitchThresold) {
                glitchStart();
            }
        } else {
            if (rand>glitchThresold) {
                glitchStop();
            }
        }
    }

    //
    if (isIdle) updateVideos();
    
    // ============
    // update image
    newFrame = false;
    #ifdef _USE_LIVE_VIDEO
        #ifdef _USE_BLACKMAGIC
            newFrame = blackCam.update();
        #else
            grabber.update(); newFrame = grabber.isFrameNew();
        #endif
    #else
        video.update(); newFrame = video.isFrameNew();
    #endif
    
    if(newFrame){
        
        // capture
        #ifdef _USE_LIVE_VIDEO
            #ifdef _USE_BLACKMAGIC
//                srcImg.setFromPixels(blackCam.getGrayPixels());
                ofTexture t = ofTexture(blackCam.getGrayTexture());
                ofPixels p;
                t.readToPixels(p);
                srcImg.setFromPixels(p);
            #else
                srcImg.setFromPixels(grabber.getPixels());
            #endif
        #else
            srcImg.setFromPixels(video.getPixels());
        #endif
        
        // Resize, mirror, crop, filter
        srcImg.resize(srcImg.getWidth()*srcImgScale, srcImg.getHeight()*srcImgScale);
        if (srcImgIsCropped) srcImg.crop((srcImg.getWidth()-srcImg.getHeight())/2, 0, srcImg.getHeight(), srcImg.getHeight());
        srcImg.mirror(false, true);
        if (srcImgIsFiltered) clahe.filter(srcImg, srcImg, filterClaheClipLimit, srcImgIsColored);
        srcImg.update();
        
        // Update Tracker
        tracker.update(srcImg.getPixels());
    }

    // ============
    // Sleep wake logic
    if ( tracker.size() == 0 ) {
        // Faces were found before
        if (facesFound) {
            timerSleep.reset(), timerSleep.startTimer(); // reset and start sleep timer
            timerWake.stopTimer(), timerShowGrid.stopTimer(), timerShowText.stopTimer(); // stop the timers
            vidRecorder.close();
            facesFound = false; // reset bools
        }
    } else if (!facesFound) {
        // It comes from idle mode
        if (isIdle) timerWake.reset(), timerWake.startTimer(); // reset and start the wake timer
        timerSleep.stopTimer(); // stop the sleep timer
        facesFound = true;
    }

    // ============
    // If not in Idle mode
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
            agedImages.push_back(IndexedImages(instance.getLabel(),img));
            
            // if the instance is the face locked and it's visible
            if ( img.isAllocated() && instance.getLabel()==faceLockedLabel) {
                // Save x, y
                faceLockedX = instance.getBoundingBox().getX(), faceLockedY = instance.getBoundingBox().getY();
                // add image to vid recorder
                if (vidRecorder.isInitialized()) vidRecorder.addFrame(img.getPixels());
                // update averages values
                faceUtils.updateLandmarksAverage(instance);
                // Push elements to the grid
                if (showGrid) {
                    for (int i=0; i<5; i++) {
                        if (!mixElements) {
                            ofPixels pix;
                            switch(elementsID) {
                                case 0: pix = faceUtils.getLandmarkPixels(srcImg, instance, 0, faceImgSize/4, 80, (offsetElements) ? i*4 : i); break;
                                case 1: pix = faceUtils.getLandmarkPixels(srcImg, instance, 1, faceImgSize/4, 30, (offsetElements) ? i*4 : i); break;
                                case 2: pix = faceUtils.getLandmarkPixels(srcImg, instance, 2, faceImgSize/4, 30, (offsetElements) ? i*4 : i); break;
                                case 3: pix = faceUtils.getLandmarkPixels(srcImg, instance, 3, faceImgSize/4, 40, (offsetElements) ? i*4 : i); break;
                                case 4: pix = faceUtils.getLandmarkPixels(srcImg, instance, 4, faceImgSize/4, 40, (offsetElements) ? i*4 : i); break;
                            }
                            pis.push_back(Grid::PixelsItem(pix, Grid::face));
                        } else {
                            ofPixels pix00 = faceUtils.getLandmarkPixels(srcImg, instance, 0, faceImgSize/4, 80, (offsetElements) ? i*4 : i);
                            ofPixels pix01 = faceUtils.getLandmarkPixels(srcImg, instance, 1, faceImgSize/4, 30, (offsetElements) ? i*4 : i);
                            ofPixels pix02 = faceUtils.getLandmarkPixels(srcImg, instance, 2, faceImgSize/4, 30, (offsetElements) ? i*4 : i);
                            ofPixels pix03 = faceUtils.getLandmarkPixels(srcImg, instance, 3, faceImgSize/4, 40, (offsetElements) ? i*4 : i);
                            ofPixels pix04 = faceUtils.getLandmarkPixels(srcImg, instance, 4, faceImgSize/4, 40, (offsetElements) ? i*4 : i);
                            pis.push_back(Grid::PixelsItem(pix00, Grid::face));
                            pis.push_back(Grid::PixelsItem(pix01, Grid::leftEye));
                            pis.push_back(Grid::PixelsItem(pix02, Grid::rightEye));
                            pis.push_back(Grid::PixelsItem(pix03, Grid::nose));
                            pis.push_back(Grid::PixelsItem(pix04, Grid::mouth));
                        }
                    }
                }
                //
                lockedFaceFound = true;
            }
        }
        
        // If the locked face is not visible
        if (!lockedFaceFound) {
            // Stop timers
            timerShowGrid.reset(), timerShowGrid.stopTimer();
            timerShowText.reset(), timerShowText.stopTimer();
            showGrid = false, showText = false;
        } else if (!showGrid) {
            // Start gridTimer
            timerShowGrid.startTimer();
            // set age to Lock to default
            ageToLock = timeToLock / secondToAgeCoef;
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
        
    }
    
    // text + speech
    if (log.speechUpdate()) {
        textContent.at(textContentIndex).append(log.getCurrentWord() + " ");
    }
    
    // FBOs
    ofImage alignedFace;
    // Gui outputs
    guiFbo.begin();
        ofBackground(0);
        ofPushMatrix();
            ofScale(1/srcImgScale*sceneScale, 1/srcImgScale*sceneScale);
            // Draw source image + tracker
            if (showTracker) {
                srcImg.draw(0, 0);
                // Draw tracker landmarks
                tracker.drawDebug();
            }
            // Draw Faces
            for(int i=0; i<agedImages.size(); i++) {
                agedImages[i].second.draw(i*faceImgSize, (showTracker) ? srcImg.getHeight() : 0);
                if (agedImages[i].first == faceLockedLabel) {
                    ofPushStyle();
                        ofNoFill();
                        ofSetColor(colorBright);
                        ofDrawRectangle(i*faceImgSize, (showTracker) ? srcImg.getHeight() : 0, faceImgSize, faceImgSize);
                    ofPopStyle();
                    alignedFace = agedImages[i].second;
                }
            }
        ofPopMatrix();
        
        if (showTextUI) {
            int x = srcImg.getWidth()*srcImgScale*sceneScale + 20;
//            int x = displaySizeW + 20;
            // Draw text UI
            ofDrawBitmapStringHighlight("Framerate : " + ofToString(ofGetFrameRate()), x, 20);
            ofDrawBitmapStringHighlight("Tracker thread framerate : " + ofToString(tracker.getThreadFps()), x, 40);
            //
            if (!isIdle) {
                if (!facesFound) ofDrawBitmapStringHighlight("Idle in: " + ofToString(timerSleep.getTimeLeftInSeconds()), x, 60, ofColor(200,0,0), ofColor(255));
                if (faceLocked) ofDrawBitmapStringHighlight("LOCKED - Face label: " + ofToString(faceLockedLabel), x, 80, ofColor(150,0,0), ofColor(255));
                if (lockedFaceFound) {
                    string grid = (showGrid) ? "GRID" : "Time to Grid: " + ofToString(timerShowGrid.getTimeLeftInSeconds());
                    ofDrawBitmapStringHighlight(grid, x, 100, ofColor(100,0,0), ofColor(255));
                }
            } else {
                string idle = (!facesFound) ? "IDLE" : "IDLE - Wake in: " + ofToString(timerWake.getTimeLeftInSeconds());
                ofDrawBitmapStringHighlight(idle, x, 60, ofColor(200,0,0), ofColor(255));
            }
        }
    guiFbo.end();
    

    // Display Output
    displayFbo.begin();
        // Draw the videos if idle
        if (isIdle) drawVideos();
        // srcImg + tracker if no faces are locked
        else if (!lockedFaceFound) {
            // Draw srcImg
            int s = srcImg.getHeight();
            int cropW = s, cropH = s;
            int cropX = (srcImgIsCropped) ? (srcImg.getWidth()-srcImg.getHeight())/2 : ofClamp(int(faceLockedX/s)*s, 0, srcImg.getWidth()-cropW);
            int cropY = 0;
            srcImg.drawSubsection(0, 0, displaySizeW, displaySizeH, cropX, cropY, cropW, cropH);
            ofPushMatrix();
                ofScale(displaySizeW/srcImg.getHeight(), displaySizeH/srcImg.getHeight());
                ofTranslate(-cropX, cropY);
                tracker.drawDebug();
            ofPopMatrix();
        } else alignedFace.draw(0, 0, displaySizeW, displaySizeH);
        // Grid
        if (showGrid) grid.draw(0,0);
        // Text
        if (showText) {
            int padding = 3*textScale, w = 32*textScale;
            drawTextFrame(textFont, textContent[textContentIndex], textX, textY, w, w, padding);
//            int padding = 4, s = 32, w = s*2;
//            if (textContentIndex==0) drawTextFrame(textFont, textContent[0], s*2, 0, w, w, padding);
//            if (textContentIndex==1) drawTextFrame(textFont, textContent[1], s, s*3, w, w, padding);
//            if (textContentIndex==2) drawTextFrame(textFont, textContent[2], s*4, s*4, w, w, padding);
        }
        drawCounter(161,174);
    displayFbo.end();
}

//--------------------------------------------------------------
void ofApp::draw(){
    
    guiFbo.draw(0,displaySizeH);
    glitch.generateFx();
    displayFbo.draw(displayPositionX, displayPositionY);
    // Draw GUI
    if (showGUI) drawGui();
}

//--------------------------------------------------------------
void ofApp::exit(){
    //
    ofRemoveListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::vidRecordingComplete);
    vidRecorder.close();
    blackCam.close();
    if (live.isLoaded()) live.stop();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == ' ') faceLocked = false;
    if(key == 't') showTextUI = !showTextUI;
    if(key == 'r') showTracker = !showTracker;
    if(key == 'g') showGUI = !showGUI;
    if(key == 'f'){
        fullScreen = !fullScreen;
        if(fullScreen) ofSetFullscreen(true);
        else ofSetFullscreen(false);
    }
    // Live
    if (key == 'z') initLive();
    if (key == 'x') live.printAll();
    if (key == '.') {
        float v = live.getVolume();
        live.setVolume( ofClamp(v + 0.1, 0, 1) );
    }
    if (key == ',') {
        float v = live.getVolume();
        live.setVolume( ofClamp(v - 0.1, 0, 1) );
    }
    // Glitch
    if (key == '1') glitch.setFx(OFXPOSTGLITCH_CR_HIGHCONTRAST  , true);
    if (key == '2') glitch.setFx(OFXPOSTGLITCH_SHAKER           , true);
    if (key == '3') glitch.setFx(OFXPOSTGLITCH_CUTSLIDER		, true);
    if (key == '4') glitch.setFx(OFXPOSTGLITCH_NOISE			, true);
    if (key == '5') glitch.setFx(OFXPOSTGLITCH_SLITSCAN         , true);
    if (key == '6') glitch.setFx(OFXPOSTGLITCH_INVERT			, true);
}

void ofApp::keyReleased(int key) {
    // Glitch
    if (key == '1') glitch.setFx(OFXPOSTGLITCH_CR_HIGHCONTRAST, false);
    if (key == '2') glitch.setFx(OFXPOSTGLITCH_SHAKER			, false);
    if (key == '3') glitch.setFx(OFXPOSTGLITCH_CUTSLIDER		, false);
    if (key == '4') glitch.setFx(OFXPOSTGLITCH_NOISE			, false);
    if (key == '5') glitch.setFx(OFXPOSTGLITCH_SLITSCAN         , false);
    if (key == '6') glitch.setFx(OFXPOSTGLITCH_INVERT			, false);
}

// VAR
//--------------------------------------------------------------
void ofApp::initVar(){
    // general
    isIdle = false, facesFound = false, lockedFaceFound = false,  faceLocked = false;
    showText = false, showTextUI = true, showTracker = true, showGUI = true;
    displayPositionX = 0, displayPositionY = 0, displaySizeW = 192, displaySizeH = 192, sceneScale = .5;
    colorDark = ofColor(100,0,0,230), colorBright = ofColor::crimson;
    noiseOfTime = 0, noiseFreq = 30, glitchThresold = .5, glitchFreq = 2;
    
    // capture
    srcImgScale = .66666666666;
    
    // timers
    timeToSleep = 600000; // time before entering idle mode
    timeToWake = 2000; // time before exiting idle mode
    timeToLock = 2000, // Time before locking up a face
    timeToShowGrid = 2000; // time before grid
    timeToShowText = 4000; // time before showing the text
    timeToShowNextText = 4000; // time before showing the next bunch of text

    // tracker
    trackerFaceDetectorImageSize = 1000000, trackerLandmarkDetectorImageSize = 2000000;
    secondToAgeCoef = 140, ageToLock = timeToLock / secondToAgeCoef;
    trackerIsThreaded = false;
    //
    faceImgSize = 192, desiredLeftEyeX = 0.39, desiredLeftEyeY = 0.43, faceScaleRatio = 2.5;
    faceRotate = true, faceConstrain = false;

    // filter
    filterClaheClipLimit = 6;
    srcImgIsCropped = true, srcImgIsFiltered = true, srcImgIsColored = false;
    
    // video recording + playing
    faceVideoPath = "output"; videosCount = 64;

    // grid
    showGrid = false, showGridElements = false, gridIsSquare = true;
    gridWidth = 24, gridHeight = 24, gridRes = 16, gridMinSize = 0, gridMaxSize = 12;
    mixElements = false, offsetElements = false, elementsID = 0;
    
    // text
    textContent.resize(3);
    textFont.load("fonts/Verdana.ttf", 12, false);
    textFont.setLineHeight(10);
    textFileIndex = 0, textContentIndex = 0;
    textScale = 2, textX = 0, textY = 0;

    
    // live
    volumes.assign(5,0), initTimesVolumes.assign(5,0), startVolumes.assign(5,0), endVolumes.assign(5,0);
    resetLive = true;

}


// GENERAL
//--------------------------------------------------------------
void ofApp::randomizeSettings(){
    // grid
    float a = ofRandom(1);
    if (a<.2) gridWidth = 24, gridHeight = 24, gridRes = 8, gridMaxSize = ofRandom(24);
    else if (a<.4) gridWidth = 12, gridHeight = 12, gridRes = 16, gridMaxSize = ofRandom(12);
    else gridWidth = 6, gridHeight = 6, gridRes = 32, gridMaxSize = ofRandom(6);
    gridIsSquare = (ofRandom(1)>.5) ? true : false;
    // grid content
    if (ofRandom(1)>.5) mixElements = !mixElements;
    if (mixElements) elementsID = int(ofRandom(5));
    if (ofRandom(1)>.5) offsetElements = !offsetElements;
    // Video Playback
    vector<int> s = {1, 2, 3, 6, 8};
    int r = s[(int)ofRandom(0,s.size())];
    videosCount = pow(r,2);
    // face
}


// GUI
//--------------------------------------------------------------
void ofApp::drawGui(){
    gui.begin();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    if (ImGui::CollapsingHeader("General", false)) {
        ImGui::DragFloat("Scale Source", &srcImgScale, .1, 3);
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
        ImGui::SliderFloat("scale amount", &faceScaleRatio, 1, 5);
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
        if (ImGui::SliderInt("Coef", &secondToAgeCoef, 10, 200)) ageToLock = timeToLock / secondToAgeCoef;
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
        //
        ImGui::Columns(1);
        ImGui::Separator();
        if (ImGui::Button("Refresh")) grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare); ImGui::SameLine();
        if (ImGui::Button("Randomize")) {
            randomizeSettings();
            grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare);
        }
        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Output", false)) {
        ImGui::Columns(2);
        //
        ImGui::Text("Position");
        ImGui::DragInt("X", &displayPositionX, 1, 0, ofGetWindowWidth(), "%.0f ms");
        ImGui::DragInt("Y", &displayPositionY, 1, 0, ofGetWindowHeight(), "%.0f ms");
        ImGui::NextColumn();
        //
        ImGui::Text("Size");
        if (ImGui::DragInt("W", &displaySizeW, 1, 1, ofGetWindowWidth()-displayPositionX, "%.0f px")) displayFbo.allocate(displaySizeW, displaySizeH);
        if (ImGui::DragInt("H", &displaySizeH, 1, 1, ofGetWindowHeight()-displayPositionY, "%.0f px")) displayFbo.allocate(displaySizeW, displaySizeH);
        //
        ImGui::Columns(1);
        ImGui::Separator();

    }
    if (ImGui::CollapsingHeader("other", false)) {
        ImGui::Text("Overall");
        ImGui::SliderInt("videosCount", &videosCount, 1, 144);
        ImGui::Text("Glitch: %.c", glitchOn);
        ImGui::SliderFloat("Freq", &noiseFreq, 1, 300);
        ImGui::SliderFloat("glitchThresold", &glitchThresold, .1, 1);
        ImGui::SliderInt("Freq (higher = less often)", &glitchFreq, 1, 10);
        ImGui::SliderFloat("noiseOfTime", &noiseOfTime, -1, 1);
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
    tracker.faceRectanglesTracker.setMaximumDistance(150);
    tracker.faceRectanglesTracker.setPersistence(20);
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
    ageToLock = (timeToLock + timeToWake ) / secondToAgeCoef;
    // Start Videos
    loadVideos();
    playVideos();
    // start timer to show text
    timerShowText.reset(), timerShowText.startTimer();
    // Adjust volumes
    liveVolumeDown();
    // Idle
    isIdle = true;
}

void ofApp::timerWakeFinished(ofEventArgs &e) {
    // Stop Timer
    timerWake.stopTimer();
    // Stop Videos
    stopVideos();
    // Adjust volumes
    liveVolumeUp();
    // Not idle
    isIdle = false;
}

void ofApp::timerShowGridFinished(ofEventArgs &e) {
    // Stop Timer
    timerShowGrid.stopTimer();
    timerShowText.reset(), timerShowText.startTimer();
    // Init and show grid
    randomizeSettings();
    grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare);
    // Save the time
    initTimeGrid = ofGetElapsedTimef();
    showGrid = true;
}

void ofApp::timerShowTextFinished(ofEventArgs &e) {
    
    // start log speech
    if (!log.startSpeaking) {
        // build speech settings
        string voice = "Kate", msg = textFileLines.at(textFileIndex), misc = "";
        log.logAudio(voice, "", "", "130", "1", msg);
        cout << "SPEECH: " + ofToString(textContentIndex) + " " + msg << endl;
    }
    
    // increment indexes
    textFileIndex = (textFileIndex+1)%textFileLines.size();
    textContentIndex = (textContentIndex+1)%textContent.size();
    
    // clear current text
    textContent.at(textContentIndex).clear();
    
    //
    textScale = ofRandom(3, 7);
    textX = (int)ofRandom(0, 7-textScale) * 32;
    textY = (int)ofRandom(0, 7-textScale) * 32;
    
    // loadfont 6 => 64
    textFont.load("fonts/Verdana.ttf", 4*textScale, false);
    textFont.setLineHeight(5*textScale);
    textFont.setLetterSpacing(1.015);


    // restart timer
    float t = (textContentIndex == 0) ? ofRandom(timeToShowText, timeToShowText*2) : timeToShowNextText+ofRandom(timeToShowNextText/2);
    timerShowText.setTimer(t);
    timerShowText.startTimer();
    
    //
    showText = true;
}

// TEXT
//--------------------------------------------------------------
void ofApp::loadTextFile() {
    // text loading
    ofBuffer buffer = ofBufferFromFile("txt/love_lyrics.txt");
    //
    if(buffer.size()) {
        for (ofBuffer::Line it = buffer.getLines().begin(), end = buffer.getLines().end(); it != end; ++it) {
            string line = *it;
            if(line.empty() == false) {
                textFileLines.push_back(line);
            }
        }
    }
    textFileIndex = ofRandom(textFileLines.size()-1);
}

//--------------------------------------------------------------
void ofApp::drawTextFrame(const ofTrueTypeFont &txtFont, string &txt, const int &x, const int &y, const int &w, const int &h, const int &padding) {
    ofPushStyle();
//        ofSetColor(colorDark);
//        ofDrawRectangle(x, y, w, h);
        ofSetColor(0,230);
        ofDrawRectangle(x, y, w, h);
        ofSetColor(255);
        txtFont.drawString(utils.wrapString(txt, w-padding*2, txtFont), x+padding, y+padding*1.2+txtFont.getSize()/2);
    ofPopStyle();
}

//--------------------------------------------------------------
void ofApp::drawCounter(const int &x, const int &y) {
    // draw Smiley + counter
    ofPushStyle();
        ofPushMatrix();
            ofTranslate(x, y);
            ofSetColor(255);
//            ofDrawCircle(6, 5, 1);
//            ofDrawCircle(12, 5, 1);
//            ofDrawRectangle(5, 10, 8, 1);
            ofDrawBitmapString(ofToString(tracker.size()), 18, 12);
        ofPopStyle();
    ofPopMatrix();
}


// VIDEO PLAYER
//--------------------------------------------------------------
void ofApp::loadVideos() {
    videosDir.allowExt("mov");
    videosDir.listDir(faceVideoPath);
    videosDir.sort();
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
            videosVector[i%videosVector.size()].draw((i%columns)*displaySizeW/columns, (i/columns)*displaySizeH/columns, displaySizeW/columns, displaySizeW/columns);
        }
    }
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


// LIVE
//--------------------------------------------------------------
void ofApp::initLive(){
    if (live.isLoaded()) {
        for ( int x=0; x<live.getNumTracks() ; x++ ){
            ofxAbletonLiveTrack *track = live.getTrack(x);
            track->setVolume(0);
        }
        live.setVolume(.8);
        live.stop();
        live.play();
        live.setTempo(90);
        liveVolumeDown();
    }
}

//--------------------------------------------------------------
void ofApp::refreshLive() {
    if (!live.isLoaded()) return;
    if (resetLive) {
        initLive();
        resetLive = false;
    }
    // Easing of volumes
    auto duration = 2.f;
    for (int i=0; i<volumes.size(); i++) {
        if ( initTimesVolumes.at(i)!=0 ) {
            auto endTime = initTimesVolumes.at(i) + duration;
            auto now = ofGetElapsedTimef();
            volumes[i] = ofxeasing::map_clamp(now, initTimesVolumes[i], endTime, startVolumes[i], endVolumes[i], &ofxeasing::linear::easeIn);
        }
    }
    // set volumes
    for ( int i=0; i<live.getNumTracks() ; i++ ) live.getTrack(i)->setVolume(volumes[i]);
    // Distortion for glitches
    if (glitchOn) {
        live.getTrack(1)->getDevice("Crew Cut")->getParameter("Dry/Wet")->setValue(abs(noiseOfTime));
        live.getTrack(2)->getDevice("Crew Cut")->getParameter("Dry/Wet")->setValue(abs(noiseOfTime));
        live.getTrack(4)->getDevice("Distort")->getParameter("Drive")->setValue(abs(noiseOfTime)*100);
    } else {
        live.getTrack(1)->getDevice("Crew Cut")->getParameter("Dry/Wet")->setValue(.1);
        live.getTrack(2)->getDevice("Crew Cut")->getParameter("Dry/Wet")->setValue(.1);
        live.getTrack(4)->getDevice("Distort")->getParameter("Drive")->setValue(20);
    }

    
}

//--------------------------------------------------------------
void ofApp::liveVolumeUp() {
    initTimesVolumes[0] = ofGetElapsedTimef(), startVolumes[0] = 0, endVolumes[0] = 0;
    initTimesVolumes[1] = ofGetElapsedTimef(), startVolumes[1] = .5, endVolumes[1] = .9;
    initTimesVolumes[2] = ofGetElapsedTimef(), startVolumes[2] = .4, endVolumes[2] = .75;
    initTimesVolumes[3] = ofGetElapsedTimef(), startVolumes[3] = 0, endVolumes[3] = 0;
    initTimesVolumes[4] = ofGetElapsedTimef(), startVolumes[4] = .45, endVolumes[4] = .7;
}

//--------------------------------------------------------------
void ofApp::liveVolumeDown() {
    initTimesVolumes[0] = ofGetElapsedTimef(), startVolumes[0] = 0, endVolumes[0] = 0;
    initTimesVolumes[1] = ofGetElapsedTimef(), startVolumes[1] = .9, endVolumes[1] = .5;
    initTimesVolumes[2] = ofGetElapsedTimef(), startVolumes[2] = .75, endVolumes[2] = .4;
    initTimesVolumes[3] = ofGetElapsedTimef(), startVolumes[3] = 0, endVolumes[3] = 0;
    initTimesVolumes[4] = ofGetElapsedTimef(), startVolumes[4] = .7, endVolumes[4] = .45;
}


// GLITCH
//--------------------------------------------------------------
void ofApp::glitchStart(){
    changeGlitchState(ofRandom(1,7), true);
    if (ofRandomuf()>.5) {
        changeGlitchState(ofRandom(1,7), true);
        if (ofRandomuf()>.5) {
            changeGlitchState(ofRandom(1,7), true);
        };
    };
    glitchOn = true;
}

//--------------------------------------------------------------
void ofApp::glitchStop() {
    for (int i=1; i<=6; i++) changeGlitchState(i, false);
    glitchOn = false;
}

//--------------------------------------------------------------
void ofApp::changeGlitchState(int id, bool state) {
    switch (id) {
        case 1:
            glitch.setFx(OFXPOSTGLITCH_CR_HIGHCONTRAST, state);
            break;
        case 2:
            glitch.setFx(OFXPOSTGLITCH_SHAKER, state);
            break;
        case 3:
            glitch.setFx(OFXPOSTGLITCH_CUTSLIDER, state);
            break;
        case 4:
            glitch.setFx(OFXPOSTGLITCH_NOISE, state);
            break;
        case 5:
            glitch.setFx(OFXPOSTGLITCH_SLITSCAN, state);
            break;
        case 6:
            glitch.setFx(OFXPOSTGLITCH_INVERT, state);
            break;
        default:
            break;
    }
}
=======
#include "ofApp.h"

// CORE
//--------------------------------------------------------------
void ofApp::setup(){
//    ofSetLogLevel(OF_LOG_VERBOSE);
    //
    ofSetFullscreen(true);
    ofSetBackgroundColor(0);
    ofTrueTypeFont::setGlobalDpi(72);
    
    #ifdef _USE_LIVE_VIDEO
        #ifdef _USE_BLACKMAGIC
            blackCam.setup(1920, 1080, 30);
        #else
            grabber.setDeviceID(0); grabber.setup(1920, 1080);
        #endif
    #else
        video.load("vids/motinas_multi_face_frontal.mp4"); video.play();
    #endif
    initVar();
    loadTextFile();
    log.start();
    initTracker();
    initVidRecorder();
    initTimers();
    live.setup();
    initLive();
    //
    displayFbo.allocate(displaySizeW, displaySizeH);
    guiFbo.allocate(ofGetWidth(), ofGetHeight());
    glitch.setup(&displayFbo);



}

//--------------------------------------------------------------
void ofApp::update(){
    
    //
    live.update();
    refreshLive();
    
    //
    noiseOfTime = ofSignedNoise(ofGetElapsedTimef()/noiseFreq,0);
    if (int(ofGetElapsedTimef())%glitchFreq == 0) {
        float rand = ofRandom(0, abs(noiseOfTime));
        if (!glitchOn) {
            if (rand>glitchThresold) {
                glitchStart();
            }
        } else {
            if (rand>glitchThresold) {
                glitchStop();
            }
        }
    }

    //
    if (isIdle) updateVideos();
    
    // ============
    // update image
    newFrame = false;
    #ifdef _USE_LIVE_VIDEO
        #ifdef _USE_BLACKMAGIC
            newFrame = blackCam.update();
        #else
            grabber.update(); newFrame = grabber.isFrameNew();
        #endif
    #else
        video.update(); newFrame = video.isFrameNew();
    #endif
    
    if(newFrame){
        
        // capture
        #ifdef _USE_LIVE_VIDEO
            #ifdef _USE_BLACKMAGIC
//                srcImg.setFromPixels(blackCam.getGrayPixels());
                ofTexture t = ofTexture(blackCam.getGrayTexture());
                ofPixels p;
                t.readToPixels(p);
                srcImg.setFromPixels(p);
            #else
                srcImg.setFromPixels(grabber.getPixels());
            #endif
        #else
            srcImg.setFromPixels(video.getPixels());
        #endif
        
        // Resize, mirror, crop, filter
        srcImg.resize(srcImg.getWidth()*srcImgScale, srcImg.getHeight()*srcImgScale);
        if (srcImgIsCropped) srcImg.crop((srcImg.getWidth()-srcImg.getHeight())/2, 0, srcImg.getHeight(), srcImg.getHeight());
        srcImg.mirror(false, true);
        if (srcImgIsFiltered) clahe.filter(srcImg, srcImg, filterClaheClipLimit, srcImgIsColored);
        srcImg.update();
        
        // Update Tracker
        tracker.update(srcImg.getPixels());
    }

    // ============
    // Sleep wake logic
    if ( tracker.size() == 0 ) {
        // Faces were found before
        if (facesFound) {
            timerSleep.reset(), timerSleep.startTimer(); // reset and start sleep timer
            timerWake.stopTimer(), timerShowGrid.stopTimer(), timerShowText.stopTimer(); // stop the timers
            vidRecorder.close();
            facesFound = false; // reset bools
        }
    } else if (!facesFound) {
        // It comes from idle mode
        if (isIdle) timerWake.reset(), timerWake.startTimer(); // reset and start the wake timer
        timerSleep.stopTimer(); // stop the sleep timer
        facesFound = true;
    }

    // ============
    // If not in Idle mode
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
            agedImages.push_back(IndexedImages(instance.getLabel(),img));
            
            // if the instance is the face locked and it's visible
            if ( img.isAllocated() && instance.getLabel()==faceLockedLabel) {
                // Save x, y
                faceLockedX = instance.getBoundingBox().getX(), faceLockedY = instance.getBoundingBox().getY();
                // add image to vid recorder
                if (vidRecorder.isInitialized()) vidRecorder.addFrame(img.getPixels());
                // update averages values
                faceUtils.updateLandmarksAverage(instance);
                // Push elements to the grid
                if (showGrid) {
                    for (int i=0; i<5; i++) {
                        if (!mixElements) {
                            ofPixels pix;
                            switch(elementsID) {
                                case 0: pix = faceUtils.getLandmarkPixels(srcImg, instance, 0, faceImgSize/4, 80, (offsetElements) ? i*4 : i); break;
                                case 1: pix = faceUtils.getLandmarkPixels(srcImg, instance, 1, faceImgSize/4, 30, (offsetElements) ? i*4 : i); break;
                                case 2: pix = faceUtils.getLandmarkPixels(srcImg, instance, 2, faceImgSize/4, 30, (offsetElements) ? i*4 : i); break;
                                case 3: pix = faceUtils.getLandmarkPixels(srcImg, instance, 3, faceImgSize/4, 40, (offsetElements) ? i*4 : i); break;
                                case 4: pix = faceUtils.getLandmarkPixels(srcImg, instance, 4, faceImgSize/4, 40, (offsetElements) ? i*4 : i); break;
                            }
                            pis.push_back(Grid::PixelsItem(pix, Grid::face));
                        } else {
                            ofPixels pix00 = faceUtils.getLandmarkPixels(srcImg, instance, 0, faceImgSize/4, 80, (offsetElements) ? i*4 : i);
                            ofPixels pix01 = faceUtils.getLandmarkPixels(srcImg, instance, 1, faceImgSize/4, 30, (offsetElements) ? i*4 : i);
                            ofPixels pix02 = faceUtils.getLandmarkPixels(srcImg, instance, 2, faceImgSize/4, 30, (offsetElements) ? i*4 : i);
                            ofPixels pix03 = faceUtils.getLandmarkPixels(srcImg, instance, 3, faceImgSize/4, 40, (offsetElements) ? i*4 : i);
                            ofPixels pix04 = faceUtils.getLandmarkPixels(srcImg, instance, 4, faceImgSize/4, 40, (offsetElements) ? i*4 : i);
                            pis.push_back(Grid::PixelsItem(pix00, Grid::face));
                            pis.push_back(Grid::PixelsItem(pix01, Grid::leftEye));
                            pis.push_back(Grid::PixelsItem(pix02, Grid::rightEye));
                            pis.push_back(Grid::PixelsItem(pix03, Grid::nose));
                            pis.push_back(Grid::PixelsItem(pix04, Grid::mouth));
                        }
                    }
                }
                //
                lockedFaceFound = true;
            }
        }
        
        // If the locked face is not visible
        if (!lockedFaceFound) {
            // Stop timers
            timerShowGrid.reset(), timerShowGrid.stopTimer();
            timerShowText.reset(), timerShowText.stopTimer();
            showGrid = false, showText = false;
        } else if (!showGrid) {
            // Start gridTimer
            timerShowGrid.startTimer();
            // set age to Lock to default
            ageToLock = timeToLock / secondToAgeCoef;
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
        
    }
    
    // text + speech
    if (log.speechUpdate()) {
        textContent.at(textContentIndex).append(log.getCurrentWord() + " ");
    }
    
    // FBOs
    ofImage alignedFace;
    // Gui outputs
    guiFbo.begin();
        ofBackground(0);
        ofPushMatrix();
            ofScale(1/srcImgScale*sceneScale, 1/srcImgScale*sceneScale);
            // Draw source image + tracker
            if (showTracker) {
                srcImg.draw(0, 0);
                // Draw tracker landmarks
                tracker.drawDebug();
            }
            // Draw Faces
            for(int i=0; i<agedImages.size(); i++) {
                agedImages[i].second.draw(i*faceImgSize, (showTracker) ? srcImg.getHeight() : 0);
                if (agedImages[i].first == faceLockedLabel) {
                    ofPushStyle();
                        ofNoFill();
                        ofSetColor(colorBright);
                        ofDrawRectangle(i*faceImgSize, (showTracker) ? srcImg.getHeight() : 0, faceImgSize, faceImgSize);
                    ofPopStyle();
                    alignedFace = agedImages[i].second;
                }
            }
        ofPopMatrix();
        
        if (showTextUI) {
            int x = srcImg.getWidth()*srcImgScale*sceneScale + 20;
//            int x = displaySizeW + 20;
            // Draw text UI
            ofDrawBitmapStringHighlight("Framerate : " + ofToString(ofGetFrameRate()), x, 20);
            ofDrawBitmapStringHighlight("Tracker thread framerate : " + ofToString(tracker.getThreadFps()), x, 40);
            //
            if (!isIdle) {
                if (!facesFound) ofDrawBitmapStringHighlight("Idle in: " + ofToString(timerSleep.getTimeLeftInSeconds()), x, 60, ofColor(200,0,0), ofColor(255));
                if (faceLocked) ofDrawBitmapStringHighlight("LOCKED - Face label: " + ofToString(faceLockedLabel), x, 80, ofColor(150,0,0), ofColor(255));
                if (lockedFaceFound) {
                    string grid = (showGrid) ? "GRID" : "Time to Grid: " + ofToString(timerShowGrid.getTimeLeftInSeconds());
                    ofDrawBitmapStringHighlight(grid, x, 100, ofColor(100,0,0), ofColor(255));
                }
            } else {
                string idle = (!facesFound) ? "IDLE" : "IDLE - Wake in: " + ofToString(timerWake.getTimeLeftInSeconds());
                ofDrawBitmapStringHighlight(idle, x, 60, ofColor(200,0,0), ofColor(255));
            }
        }
    guiFbo.end();
    

    // Display Output
    displayFbo.begin();
        // Draw the videos if idle
        if (isIdle) drawVideos();
        // srcImg + tracker if no faces are locked
        else if (!lockedFaceFound) {
            // Draw srcImg
            int s = srcImg.getHeight();
            int cropW = s, cropH = s;
            int cropX = (srcImgIsCropped) ? (srcImg.getWidth()-srcImg.getHeight())/2 : ofClamp(int(faceLockedX/s)*s, 0, srcImg.getWidth()-cropW);
            int cropY = 0;
            srcImg.drawSubsection(0, 0, displaySizeW, displaySizeH, cropX, cropY, cropW, cropH);
            ofPushMatrix();
                ofScale(displaySizeW/srcImg.getHeight(), displaySizeH/srcImg.getHeight());
                ofTranslate(-cropX, cropY);
                tracker.drawDebug();
            ofPopMatrix();
        } else alignedFace.draw(0, 0, displaySizeW, displaySizeH);
        // Grid
        if (showGrid) grid.draw(0,0);
        // Text
        if (showText) {
            int padding = 3*textScale, w = 32*textScale;
            drawTextFrame(textFont, textContent[textContentIndex], textX, textY, w, w, padding);
//            int padding = 4, s = 32, w = s*2;
//            if (textContentIndex==0) drawTextFrame(textFont, textContent[0], s*2, 0, w, w, padding);
//            if (textContentIndex==1) drawTextFrame(textFont, textContent[1], s, s*3, w, w, padding);
//            if (textContentIndex==2) drawTextFrame(textFont, textContent[2], s*4, s*4, w, w, padding);
        }
        drawCounter(161,174);
    displayFbo.end();
}

//--------------------------------------------------------------
void ofApp::draw(){
    
    guiFbo.draw(0,displaySizeH);
    glitch.generateFx();
    displayFbo.draw(displayPositionX, displayPositionY);
    // Draw GUI
    if (showGUI) drawGui();
}

//--------------------------------------------------------------
void ofApp::exit(){
    //
    ofRemoveListener(vidRecorder.outputFileCompleteEvent, this, &ofApp::vidRecordingComplete);
    vidRecorder.close();
    blackCam.close();
    if (live.isLoaded()) live.stop();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == ' ') faceLocked = false;
    if(key == 't') showTextUI = !showTextUI;
    if(key == 'r') showTracker = !showTracker;
    if(key == 'g') showGUI = !showGUI;
    if(key == 'f'){
        fullScreen = !fullScreen;
        if(fullScreen) ofSetFullscreen(true);
        else ofSetFullscreen(false);
    }
    // Live
    if (key == 'z') initLive();
    if (key == 'x') live.printAll();
    if (key == '.') {
        float v = live.getVolume();
        live.setVolume( ofClamp(v + 0.1, 0, 1) );
    }
    if (key == ',') {
        float v = live.getVolume();
        live.setVolume( ofClamp(v - 0.1, 0, 1) );
    }
    // Glitch
    if (key == '1') glitch.setFx(OFXPOSTGLITCH_CR_HIGHCONTRAST  , true);
    if (key == '2') glitch.setFx(OFXPOSTGLITCH_SHAKER           , true);
    if (key == '3') glitch.setFx(OFXPOSTGLITCH_CUTSLIDER		, true);
    if (key == '4') glitch.setFx(OFXPOSTGLITCH_NOISE			, true);
    if (key == '5') glitch.setFx(OFXPOSTGLITCH_SLITSCAN         , true);
    if (key == '6') glitch.setFx(OFXPOSTGLITCH_INVERT			, true);
}

void ofApp::keyReleased(int key) {
    // Glitch
    if (key == '1') glitch.setFx(OFXPOSTGLITCH_CR_HIGHCONTRAST, false);
    if (key == '2') glitch.setFx(OFXPOSTGLITCH_SHAKER			, false);
    if (key == '3') glitch.setFx(OFXPOSTGLITCH_CUTSLIDER		, false);
    if (key == '4') glitch.setFx(OFXPOSTGLITCH_NOISE			, false);
    if (key == '5') glitch.setFx(OFXPOSTGLITCH_SLITSCAN         , false);
    if (key == '6') glitch.setFx(OFXPOSTGLITCH_INVERT			, false);
}

// VAR
//--------------------------------------------------------------
void ofApp::initVar(){
    // general
    isIdle = false, facesFound = false, lockedFaceFound = false,  faceLocked = false;
    showText = false, showTextUI = true, showTracker = true, showGUI = true;
    displayPositionX = 0, displayPositionY = 0, displaySizeW = 192, displaySizeH = 192, sceneScale = .5;
    colorDark = ofColor(100,0,0,230), colorBright = ofColor::crimson;
    noiseOfTime = 0, noiseFreq = 30, glitchThresold = .5, glitchFreq = 2;
    
    // capture
    srcImgScale = 1;
    
    // timers
    timeToSleep = 6000; // time before entering idle mode
    timeToWake = 2000; // time before exiting idle mode
    timeToLock = 2000, // Time before locking up a face
    timeToShowGrid = 2000; // time before grid
    timeToShowText = 4000; // time before showing the text
    timeToShowNextText = 4000; // time before showing the next bunch of text

    // tracker
    trackerFaceDetectorImageSize = 1300000, trackerLandmarkDetectorImageSize = 2000000;
    secondToAgeCoef = 140, ageToLock = timeToLock / secondToAgeCoef;
    trackerIsThreaded = false;
    //
    faceImgSize = 192, desiredLeftEyeX = 0.39, desiredLeftEyeY = 0.43, faceScaleRatio = 2.5;
    faceRotate = true, faceConstrain = false;

    // filter
    filterClaheClipLimit = 4;
    srcImgIsCropped = false, srcImgIsFiltered = true, srcImgIsColored = false;
    
    // video recording + playing
    faceVideoPath = "output"; videosCount = 64;

    // grid
    showGrid = false, showGridElements = false, gridIsSquare = true;
    gridWidth = 24, gridHeight = 24, gridRes = 16, gridMinSize = 0, gridMaxSize = 12;
    mixElements = false, offsetElements = false, elementsID = 0;
    
    // text
    textContent.resize(3);
    textFont.load("fonts/Verdana.ttf", 12, false);
    textFont.setLineHeight(10);
    textFileIndex = 0, textContentIndex = 0;
    textScale = 2, textX = 0, textY = 0;

    
    // live
    volumes.assign(5,0), initTimesVolumes.assign(5,0), startVolumes.assign(5,0), endVolumes.assign(5,0);
    resetLive = true;

}


// GENERAL
//--------------------------------------------------------------
void ofApp::randomizeSettings(){
    // grid
    float a = ofRandom(1);
    if (a<.2) gridWidth = 24, gridHeight = 24, gridRes = 8, gridMaxSize = ofRandom(24);
    else if (a<.4) gridWidth = 12, gridHeight = 12, gridRes = 16, gridMaxSize = ofRandom(12);
    else gridWidth = 6, gridHeight = 6, gridRes = 32, gridMaxSize = ofRandom(6);
    gridIsSquare = (ofRandom(1)>.5) ? true : false;
    // grid content
    if (ofRandom(1)>.5) mixElements = !mixElements;
    if (mixElements) elementsID = int(ofRandom(5));
    if (ofRandom(1)>.5) offsetElements = !offsetElements;
    // Video Playback
    vector<int> s = {1, 2, 3, 6, 8};
    int r = s[(int)ofRandom(0,s.size())];
    videosCount = pow(r,2);
    // face
}


// GUI
//--------------------------------------------------------------
void ofApp::drawGui(){
    gui.begin();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    if (ImGui::CollapsingHeader("General", false)) {
        ImGui::DragFloat("Scale Source", &srcImgScale, .1, 3);
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
        ImGui::SliderFloat("scale amount", &faceScaleRatio, 1, 5);
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
        if (ImGui::SliderInt("Coef", &secondToAgeCoef, 10, 200)) ageToLock = timeToLock / secondToAgeCoef;
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
        //
        ImGui::Columns(1);
        ImGui::Separator();
        if (ImGui::Button("Refresh")) grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare); ImGui::SameLine();
        if (ImGui::Button("Randomize")) {
            randomizeSettings();
            grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare);
        }
        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Output", false)) {
        ImGui::Columns(2);
        //
        ImGui::Text("Position");
        ImGui::DragInt("X", &displayPositionX, 1, 0, ofGetWindowWidth(), "%.0f ms");
        ImGui::DragInt("Y", &displayPositionY, 1, 0, ofGetWindowHeight(), "%.0f ms");
        ImGui::NextColumn();
        //
        ImGui::Text("Size");
        if (ImGui::DragInt("W", &displaySizeW, 1, 1, ofGetWindowWidth()-displayPositionX, "%.0f px")) displayFbo.allocate(displaySizeW, displaySizeH);
        if (ImGui::DragInt("H", &displaySizeH, 1, 1, ofGetWindowHeight()-displayPositionY, "%.0f px")) displayFbo.allocate(displaySizeW, displaySizeH);
        //
        ImGui::Columns(1);
        ImGui::Separator();

    }
    if (ImGui::CollapsingHeader("other", false)) {
        ImGui::Text("Overall");
        ImGui::SliderInt("videosCount", &videosCount, 1, 144);
        ImGui::Text("Glitch: %.c", glitchOn);
        ImGui::SliderFloat("Freq", &noiseFreq, 1, 300);
        ImGui::SliderFloat("glitchThresold", &glitchThresold, .1, 1);
        ImGui::SliderInt("Freq (higher = less often)", &glitchFreq, 1, 10);
        ImGui::SliderFloat("noiseOfTime", &noiseOfTime, -1, 1);
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
    tracker.faceRectanglesTracker.setMaximumDistance(150);
    tracker.faceRectanglesTracker.setPersistence(20);
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
    ageToLock = (timeToLock + timeToWake ) / secondToAgeCoef;
    // Start Videos
    loadVideos();
    playVideos();
    // start timer to show text
    timerShowText.reset(), timerShowText.startTimer();
    // Adjust volumes
    liveVolumeDown();
    // Idle
    isIdle = true;
}

void ofApp::timerWakeFinished(ofEventArgs &e) {
    // Stop Timer
    timerWake.stopTimer();
    // Stop Videos
    stopVideos();
    // Adjust volumes
    liveVolumeUp();
    // Not idle
    isIdle = false;
}

void ofApp::timerShowGridFinished(ofEventArgs &e) {
    // Stop Timer
    timerShowGrid.stopTimer();
    timerShowText.reset(), timerShowText.startTimer();
    // Init and show grid
    randomizeSettings();
    grid.init(gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize, gridIsSquare);
    // Save the time
    initTimeGrid = ofGetElapsedTimef();
    showGrid = true;
}

void ofApp::timerShowTextFinished(ofEventArgs &e) {
    
    // start log speech
    if (!log.startSpeaking) {
        // build speech settings
        string voice = "Kate", msg = textFileLines.at(textFileIndex), misc = "";
        log.logAudio(voice, "", "", "130", "1", msg);
        cout << "SPEECH: " + ofToString(textContentIndex) + " " + msg << endl;
    }
    
    // increment indexes
    textFileIndex = (textFileIndex+1)%textFileLines.size();
    textContentIndex = (textContentIndex+1)%textContent.size();
    
    // clear current text
    textContent.at(textContentIndex).clear();
    
    //
    textScale = ofRandom(3, 7);
    textX = (int)ofRandom(0, 7-textScale) * 32;
    textY = (int)ofRandom(0, 7-textScale) * 32;
    
    // loadfont 6 => 64
    textFont.load("fonts/Verdana.ttf", 4*textScale, false);
    textFont.setLineHeight(5*textScale);
    textFont.setLetterSpacing(1.015);


    // restart timer
    float t = (textContentIndex == 0) ? ofRandom(timeToShowText, timeToShowText*2) : timeToShowNextText+ofRandom(timeToShowNextText/2);
    timerShowText.setTimer(t);
    timerShowText.startTimer();
    
    //
    showText = true;
}

// TEXT
//--------------------------------------------------------------
void ofApp::loadTextFile() {
    // text loading
    ofBuffer buffer = ofBufferFromFile("txt/love_lyrics.txt");
    //
    if(buffer.size()) {
        for (ofBuffer::Line it = buffer.getLines().begin(), end = buffer.getLines().end(); it != end; ++it) {
            string line = *it;
            if(line.empty() == false) {
                textFileLines.push_back(line);
            }
        }
    }
    textFileIndex = ofRandom(textFileLines.size()-1);
}

//--------------------------------------------------------------
void ofApp::drawTextFrame(const ofTrueTypeFont &txtFont, string &txt, const int &x, const int &y, const int &w, const int &h, const int &padding) {
    ofPushStyle();
//        ofSetColor(colorDark);
//        ofDrawRectangle(x, y, w, h);
        ofSetColor(0,230);
        ofDrawRectangle(x, y, w, h);
        ofSetColor(255);
        txtFont.drawString(utils.wrapString(txt, w-padding*2, txtFont), x+padding, y+padding*1.2+txtFont.getSize()/2);
    ofPopStyle();
}

//--------------------------------------------------------------
void ofApp::drawCounter(const int &x, const int &y) {
    // draw Smiley + counter
    ofPushStyle();
        ofPushMatrix();
            ofTranslate(x, y);
            ofSetColor(255);
//            ofDrawCircle(6, 5, 1);
//            ofDrawCircle(12, 5, 1);
//            ofDrawRectangle(5, 10, 8, 1);
            ofDrawBitmapString(ofToString(tracker.size()), 18, 12);
        ofPopStyle();
    ofPopMatrix();
}


// VIDEO PLAYER
//--------------------------------------------------------------
void ofApp::loadVideos() {
    videosDir.allowExt("mov");
    videosDir.listDir(faceVideoPath);
    videosDir.sort();
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
            videosVector[i%videosVector.size()].draw((i%columns)*displaySizeW/columns, (i/columns)*displaySizeH/columns, displaySizeW/columns, displaySizeW/columns);
        }
    }
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


// LIVE
//--------------------------------------------------------------
void ofApp::initLive(){
    if (live.isLoaded()) {
        for ( int x=0; x<live.getNumTracks() ; x++ ){
            ofxAbletonLiveTrack *track = live.getTrack(x);
            track->setVolume(0);
        }
        live.setVolume(.8);
        live.stop();
        live.play();
        live.setTempo(90);
        liveVolumeDown();
    }
}

//--------------------------------------------------------------
void ofApp::refreshLive() {
    if (!live.isLoaded()) return;
    if (resetLive) {
        initLive();
        resetLive = false;
    }
    // Easing of volumes
    auto duration = 2.f;
    for (int i=0; i<volumes.size(); i++) {
        if ( initTimesVolumes.at(i)!=0 ) {
            auto endTime = initTimesVolumes.at(i) + duration;
            auto now = ofGetElapsedTimef();
            volumes[i] = ofxeasing::map_clamp(now, initTimesVolumes[i], endTime, startVolumes[i], endVolumes[i], &ofxeasing::linear::easeIn);
        }
    }
    // set volumes
    for ( int i=0; i<live.getNumTracks() ; i++ ) live.getTrack(i)->setVolume(volumes[i]);
    // Distortion for glitches
    if (glitchOn) {
        live.getTrack(1)->getDevice("Crew Cut")->getParameter("Dry/Wet")->setValue(abs(noiseOfTime));
        live.getTrack(2)->getDevice("Crew Cut")->getParameter("Dry/Wet")->setValue(abs(noiseOfTime));
        live.getTrack(4)->getDevice("Distort")->getParameter("Drive")->setValue(abs(noiseOfTime)*100);
    } else {
        live.getTrack(1)->getDevice("Crew Cut")->getParameter("Dry/Wet")->setValue(.1);
        live.getTrack(2)->getDevice("Crew Cut")->getParameter("Dry/Wet")->setValue(.1);
        live.getTrack(4)->getDevice("Distort")->getParameter("Drive")->setValue(20);
    }

    
}

//--------------------------------------------------------------
void ofApp::liveVolumeUp() {
    initTimesVolumes[0] = ofGetElapsedTimef(), startVolumes[0] = 0, endVolumes[0] = 0;
    initTimesVolumes[1] = ofGetElapsedTimef(), startVolumes[1] = .5, endVolumes[1] = .9;
    initTimesVolumes[2] = ofGetElapsedTimef(), startVolumes[2] = .4, endVolumes[2] = .75;
    initTimesVolumes[3] = ofGetElapsedTimef(), startVolumes[3] = 0, endVolumes[3] = 0;
    initTimesVolumes[4] = ofGetElapsedTimef(), startVolumes[4] = .45, endVolumes[4] = .7;
}

//--------------------------------------------------------------
void ofApp::liveVolumeDown() {
    initTimesVolumes[0] = ofGetElapsedTimef(), startVolumes[0] = 0, endVolumes[0] = 0;
    initTimesVolumes[1] = ofGetElapsedTimef(), startVolumes[1] = .9, endVolumes[1] = .5;
    initTimesVolumes[2] = ofGetElapsedTimef(), startVolumes[2] = .75, endVolumes[2] = .4;
    initTimesVolumes[3] = ofGetElapsedTimef(), startVolumes[3] = 0, endVolumes[3] = 0;
    initTimesVolumes[4] = ofGetElapsedTimef(), startVolumes[4] = .7, endVolumes[4] = .45;
}


// GLITCH
//--------------------------------------------------------------
void ofApp::glitchStart(){
    changeGlitchState(ofRandom(1,7), true);
    if (ofRandomuf()>.5) {
        changeGlitchState(ofRandom(1,7), true);
        if (ofRandomuf()>.5) {
            changeGlitchState(ofRandom(1,7), true);
        };
    };
    glitchOn = true;
}

//--------------------------------------------------------------
void ofApp::glitchStop() {
    for (int i=1; i<=6; i++) changeGlitchState(i, false);
    glitchOn = false;
}

//--------------------------------------------------------------
void ofApp::changeGlitchState(int id, bool state) {
    switch (id) {
        case 1:
            glitch.setFx(OFXPOSTGLITCH_CR_HIGHCONTRAST, state);
            break;
        case 2:
            glitch.setFx(OFXPOSTGLITCH_SHAKER, state);
            break;
        case 3:
            glitch.setFx(OFXPOSTGLITCH_CUTSLIDER, state);
            break;
        case 4:
            glitch.setFx(OFXPOSTGLITCH_NOISE, state);
            break;
        case 5:
            glitch.setFx(OFXPOSTGLITCH_SLITSCAN, state);
            break;
        case 6:
            glitch.setFx(OFXPOSTGLITCH_INVERT, state);
            break;
        default:
            break;
    }
}
>>>>>>> c9aeb41c6f631ae64757c6b96c93d375425ef786
