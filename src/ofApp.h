#pragma once

#include "ofMain.h"
#include "ofxFaceTracker2.h"
#include "ofxVideoRecorder.h"
#include "ofxImGui.h"
#include "ofxCv.h"
#include "ofxOpenCv.h"
#include "ofxTimer.h"
#include "ofxEasing.h"
#include "ofxHAPAVPlayer.h"
#include "ofxBlackMagic.h"
// local files
#include "Clahe.h"
#include "FaceUtils.h"
#include "Grid.h"
#include "LogAudio.h"
#include "MiscUtils.h"

#define _USE_LIVE_VIDEO

class ofApp : public ofBaseApp{
    
public:
    void setup();
    void update();
    void draw();
    void exit();
    void keyPressed(int key);
    
    // General
    void initVar();
    bool isIdle, facesFound, faceLocked, lockedFaceFound, showGrid, showText, newFrame, fullScreen;
    int outputPositionX, outputPositionY, outputSizeW, outputSizeH;
    float sceneScale;
    // Draw
    void drawCounter(const int &x, const int &y);
    
    // Capture
    ofVideoGrabber grabber;
    ofVideoPlayer video;
    ofImage srcImg;
    float srcImgScale;
    
    // Timers
    ofxTimer timerSleep, timerWake, timerLock, timerShowGrid, timerShowText, timerShowNextText;
    int timeToSleep, timeToWake, timeToLock, timeToShowGrid, timeToShowText, timeToShowNextText;
    void initTimers();
    void timerSleepFinished(ofEventArgs &e);
    void timerWakeFinished(ofEventArgs &e);
    void timerLockFinished(ofEventArgs &e);
    void timerShowGridFinished(ofEventArgs &e);
    void timerShowTextFinished(ofEventArgs &e);
    void timerShowNextTextFinished(ofEventArgs &e);
    
    // Tracker
    ofxFaceTracker2 tracker;
    int trackerFaceDetectorImageSize, trackerLandmarkDetectorImageSize, ageToLock, secondToAgeCoef, faceLockedLabel;
    bool trackerIsThreaded;
    void initTracker();
    // Face alignment
    FaceUtils faceUtils;
    int faceImgSize;
    float desiredLeftEyeX, desiredLeftEyeY, faceScaleRatio;
    bool faceRotate, faceConstrain;
    // Storage structure for the faces
    typedef std::pair<int, ofImage> IndexedImages;
    std::vector<IndexedImages> agedImages;
    // Sorting utils - sort by first element cresc
    struct byFirst {
        template <class First, class Second>
        bool operator()(std::pair<First, Second> const &a, std::pair<First, Second> const &b) { return a.first < b.first; }
    };

    // GUI
    ofxImGui::Gui gui;
    void drawGui();
    
    // Filters
    Clahe clahe;
    int filterClaheClipLimit;
    bool srcImgIsCropped, srcImgIsFiltered, srcImgIsColored;
    
    // Video recording
    ofxVideoRecorder vidRecorder;
    string faceVideoPath;
    void initVidRecorder();
    void vidRecordingComplete(ofxVideoRecorderOutputFileCompleteEventArgs &args);
    
    // Grid
    bool showGridElements, gridIsSquare;
    int gridWidth, gridHeight, gridRes, gridMinSize, gridMaxSize;
    float initTimeGrid;
    Grid grid;
    void randomizeGrid();
    
    // Video Player
    void loadVideos(), drawVideos(), playVideos(), stopVideos(), updateVideos();
    int dirSize, videosCount;
    ofDirectory videosDir;
    vector<ofxHAPAVPlayer> videosVector;
//    vector<ofVideoPlayer> videosVector;
    
    // BlackMagic
    ofxBlackMagic blackCam;
    
    //Text
    vector<ofTrueTypeFont> textDisplay;
    vector<string> textFileLines;
    vector<string> textContent;
    int textFileIndex, textContentIndex;
    MiscUtils utils;
//    string wrapString(string text, int width, ofTrueTypeFont textField);
    void loadTextFile();
    void drawTextFrame(const ofTrueTypeFont &txtFont, string &txt, const int &x, const int &y, const int &w, const int &h, const int &padding);

    // TTS
    LogAudio log;
    
    
};

