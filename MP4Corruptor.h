// MP4Corruptor.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#ifndef MP4CORRUPTOR_H
#define MP4CORRUPTOR_H
#include <iostream>
#include <fstream>
#include "VideoCorruptor.h"
#include <iomanip>
#include <map>

// 帧头部保护字节数
#define MP4_FRAME_HEADER_PROTECT_SIZE 512
// 最小帧间隔
#define MP4_MIN_FRAME_INTERVAL 256
// 每处理100万字节报告一次进度
#define MP4_PROGRESS_REPORT_INTERVAL 100

/**
*  MP4Corruptor
* @brief A class for corrupting MP4 video files.
* @details This class extends VideoCorruptor to implement MP4-specific corruption logic.
* @author AXIS5 with assistance from LLM
*/
class MP4Corruptor :virtual public VideoCorruptor {
   
public:
	// Constructor
    MP4Corruptor() :VideoCorruptor(){
        //start_ratio, end_ratio, intensity, burst_size
        stages= {
        {0.0, 0.05, 0.0002,1},
        {0.05, 0.15, 0.001,1},
        {0.15, 0.30, 0.0025,2},
        {0.30, 0.50, 0.005,3},
        {0.50, 0.70, 0.01,5},
        {0.70, 0.90, 0.02,8},
        {0.90, 1.00, 0.04,15}
        };
    }

	//Load MP4 file into memory
    bool loadFile(const string& filename) override;

	//Save corrupted MP4 file to disk
    bool saveFile(const string& filename) override;

    void applyCorruption() override;

    void printFileInfo() override;
private:

    // 新增关键区域保护
    //void protectCriticalRegions();
    //find potential frame start positions
    vector<size_t> findPotentialFrameStarts() override;

    //pre-compute protected mask
    void precomputeProtectedMask() override;
    
    void corruptBytesBatch(const std::vector<size_t>& positions, double intensity, int phase,int burst_size);
};

#endif // !MP4CORRUPTOR_H


