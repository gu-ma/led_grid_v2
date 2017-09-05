//
//  MiscUtils.h
//  led_grid_v2
//
//  Created by Masterstudio on 05.09.17.
//
//

#ifndef MiscUtils_h
#define MiscUtils_h


class MiscUtils {
    
public:
    
    //--------------------------------------------------------------
    string wrapString(const string &text, int width, const ofTrueTypeFont &textField) {
        string typeWrapped = "";
        string tempString = "";
        vector <string> words = ofSplitString(text, " ");
        for(int i=0; i<words.size(); i++) {
            string wrd = words[i];
            // if we aren't on the first word, add a space
            if (i > 0) {
                tempString += " ";
            }
            tempString += wrd;
            int stringwidth = textField.stringWidth(tempString);
            if(stringwidth >= width) {
                typeWrapped += "\n";
                tempString = wrd;		// make sure we're including the extra word on the next line
            } else if (i > 0) {
                // if we aren't on the first word, add a space
                typeWrapped += " ";
            }
            typeWrapped += wrd;
        }
        return typeWrapped;
    }

    
};

#endif /* MiscUtils_h */
