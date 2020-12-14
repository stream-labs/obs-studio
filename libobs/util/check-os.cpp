#include "check-os.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

int is_BigSur_OS() {
    #ifndef __APPLE__
        return 0;
    #endif
    fstream f;
    f.open("/System/Library/CoreServices/SystemVersion.plist", ios::in);
    if (f.fail()) {
        return 0;
    }
    string file_str((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    if (file_str.find("<key>ProductUserVisibleVersion</key>\n\t<string>11") != string::npos) {
        f.close();
        return 1;
    }
    return 0;
}