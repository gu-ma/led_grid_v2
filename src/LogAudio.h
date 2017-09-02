//
//  LogAudio.h
//  testOfxSpeech
//
//  Only working for Mac
//
//
// To do: auto assign voices, encoding for terminal view

#ifndef LogAudio_h
#define LogAudio_h

class LogAudio : public ofThread{
public:
    
    string voice, misc, msg, rawMsg, rawMsgProgress;
    vector<string> rawMsgWords;
    string pbas,pmod,rate,volm;
    string options;
    string currentWord;
    bool term;
    bool startSpeaking = false;
    bool randomVoice = false;
    const int defaultRate = 175;
    float startTime, endTime = 0;
    int wordIndex;
    
    void start() {
        startThread();
    }
    
    void logAudio(string _voice, string _pbas, string _pmod, string _rate, string _volm, std::string _rawMsg, bool _term=false, string _misc="") {
        //
        term = _term;
        voice = _voice;
        pbas = _pbas;
        pmod = _pmod;
        rate = _rate;
        volm = _volm;
        rawMsg = _rawMsg;
        rawMsgProgress = "";
        wordIndex = -1;
        //
        options = "-v " + _voice + " " + _misc;
        msg = "[[pbas " + _pbas + "; pmod " + _pmod + "; rate " + _rate + "; volm " + _volm + "]] " + _rawMsg;
        //
        startTime = ofGetElapsedTimef();
        rawMsgWords = ofSplitString(rawMsg, " ", false, true);
        float timePerWord = 45 / ofToFloat(rate); // Bad rate !!
        float timePerMsg = rawMsgWords.size() * timePerWord;
        endTime = startTime + timePerMsg;
        //
        startSpeaking = true;
    }
    
    void listVoices() {
        cout << ofSystem("say -v '?'") << endl;
    }
    
    void threadedFunction() {
        while(isThreadRunning()) {
            if(startSpeaking) {
                string cmd = "say " + options + " '" + msg + "' ";   // create the command
                system(cmd.c_str());
                startSpeaking = false;
            }
            
        }
    }
    
    bool speechUpdate() {
        int rawMsgWordsIndex = ofMap(ofGetElapsedTimef(), startTime, endTime, 0, rawMsgWords.size()-1);
        if( rawMsgWordsIndex < rawMsgWords.size() && rawMsgWordsIndex!=wordIndex){
            currentWord = rawMsgWords[rawMsgWordsIndex];
            wordIndex = rawMsgWordsIndex;
            return true;
        } else {
            return false;
        }
    }
    
    string getCurrentWord() {
        return currentWord;
    }
};

#endif /* LogAudio_h */
