#ifndef VIDEOCORRUPTOR_H
#define VIDEOCORRUPTOR_H


#include <vector>
#include <string>
#include <random>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <algorithm>
using std::vector;
using std::mt19937;
using std::string;

/**
*  VideoCorruptor
* @brief A class for corrupting video files.
* @details This is an abstract base class that provides the interface and common functionality for video file corruption.
* @author AXIS5 with assistance from LLM
*/
class VideoCorruptor {
protected:
    vector<uint8_t> file_data;
    mt19937 rng;
    vector<bool> protected_mask;
    int frmcount;

	// Corruption stage definition
    struct CorruptionStage {
		double start_ratio; // Start position ratio (0.0-1.0)
        double end_ratio; // End position ratio (0.0-1.0)
		double intensity; // Corruption intensity (0.0-1.0)
		int burst_size; // Number of bytes to corrupt per glitch
    };
    vector<CorruptionStage> stages;
public:

    VideoCorruptor(): rng(std::chrono::steady_clock::now().time_since_epoch().count()), frmcount(0) {}
    virtual ~VideoCorruptor() = default;

    //Load file into memory
    virtual bool loadFile(const string& filename) = 0;

    //Save corrupted file to disk
    virtual bool saveFile(const string& filename)=0;

    //Corrupt
    virtual void applyCorruption()=0;

    virtual void printFileInfo()=0;
protected:
	//find potential frame start positions
    virtual vector<size_t> findPotentialFrameStarts()=0;

	//pre-compute protected mask
    virtual void precomputeProtectedMask()=0;

};
#endif
