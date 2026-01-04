#include<iostream>
#include <cctype>
#include"MP4Corruptor.h"
#include"AVICorruptor.h"
using namespace std;
int main(int argc, char* argv[]) {
    
#if defined(_WIN32) || defined(_WIN64)
    system("chcp 65001>nul");
#endif
    if (argc != 4) {
		cout << "The corruptor supports MP4 and AVI formats." << endl;
        cout << "usage: " << argv[0] << " <input file> <output file> [AVI|MP4]" << endl;
        cout << "example: " << argv[0] << " input.mp4 corrupted_output.mp4 MP4" << endl;
        return 1;
    }

    string input_file = argv[1];
    string output_file = argv[2];
    string fmt = argv[3];
    VideoCorruptor* corruptor = nullptr;
    transform(fmt.begin(), fmt.end(), fmt.begin(), (int (*)(int))tolower);
    if (fmt=="avi") {
		corruptor = new AVICorruptor();
    }
    else if (fmt == "mp4") {
        corruptor = new MP4Corruptor();
    }
    else {
        cerr << "Unsupported format: " << fmt << ". Supported formats are AVI and MP4." << endl;
		return 1;
    }


    if (!corruptor->loadFile(input_file)) {
        return 1;
    }
    corruptor->printFileInfo();
    corruptor->applyCorruption();

    if (corruptor->saveFile(output_file)) {
        delete corruptor;
        cout << "Corrupted video saved to: " << output_file << endl;
    }
    else {
        delete corruptor;
        cerr << "Save failed." << endl;
        return 1;
    }
    
    
    return 0;
}