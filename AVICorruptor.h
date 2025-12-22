// AVICorruptor.h
#ifndef AVICORRUPTOR_H
#define AVICORRUPTOR_H
#include <iostream>
#include <fstream>
#include"VideoCorruptor.h"
#include <algorithm>
#include <iomanip>


#define AVI_HEADER_PROTECT_SIZE 32768       // 保护AVI头32KB
#define AVI_MOVI_LIST_PROTECT_SIZE 8192    // 保护movi列表头8KB
#define AVI_FRAME_HEADER_SIZE 4096          // 保护视频帧头512字节
#define AVI_MIN_FRAME_INTERVAL 2048           
#define AVI_PROGRESS_REPORT_INTERVAL 10     //report every 10 glitches

/**
*  AVICorruptor
* @brief A class for corrupting AVI video files.
* @details This class extends VideoCorruptor to implement AVI-specific corruption logic.
* @author AXIS5 with assistance from LLM
*/
class AVICorruptor:virtual public VideoCorruptor{
private:

    vector<size_t> findPotentialFrameStarts() override;
    //pre-compute protected mask
    void precomputeProtectedMask() override;

public:
    AVICorruptor() : VideoCorruptor() {
        //start_ratio, end_ratio, intensity, burst_size
        stages= {
        {0.0, 0.1, 0.01,2},
        {0.1, 0.25, 0.02,3},
        {0.25, 0.40, 0.05,6},
        {0.40, 0.60, 0.10,12},
        {0.60, 0.75, 0.20,18},
        {0.75, 0.85, 0.35,25},
        {0.85, 1.00, 0.60,50}
        };
    }

    bool loadFile(const string& filename) override;

    bool saveFile(const string& filename) override;

    void applyCorruption() override;

    void printFileInfo() override;
};
#endif