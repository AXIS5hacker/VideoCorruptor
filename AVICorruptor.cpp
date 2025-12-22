#include "AVICorruptor.h"

using namespace std;

// 查找可能的视频帧起始位置
std::vector<size_t> AVICorruptor::findPotentialFrameStarts() {
    std::vector<size_t> frame_starts;
    for (size_t i = AVI_HEADER_PROTECT_SIZE; i < file_data.size() - 8; ++i) {
        // 检测视频帧标识（常见FourCC）
        if (file_data[i] == '0' && file_data[i + 1] == '0' &&
            file_data[i + 2] == 'd' && file_data[i + 3] == 'c') {
            frame_starts.push_back(i);
            i += 3; // 跳过00dc标识
        }
        else if (file_data[i] == 'd' && file_data[i + 1] == 'b') {
            frame_starts.push_back(i);
            i += 1; // 跳过db标识
        }
    }
    // 去重排序并过滤
    std::sort(frame_starts.begin(), frame_starts.end());
    frame_starts.erase(std::unique(frame_starts.begin(), frame_starts.end()), frame_starts.end());
    std::vector<size_t> filtered;
    size_t last = 0;
    for (auto pos : frame_starts) {
        if (pos - last >= AVI_MIN_FRAME_INTERVAL) {
            filtered.push_back(pos);
            last = pos;
        }
    }
    
    return filtered;
}

// 预计算保护区域
void AVICorruptor::precomputeProtectedMask() {
    protected_mask.assign(file_data.size(), false);

    // 保护AVI头
    size_t header_size = min((size_t)AVI_HEADER_PROTECT_SIZE, file_data.size());
    std::fill(protected_mask.begin(), protected_mask.begin() + header_size, true);

    // 保护movi列表头
    size_t movi_pos = file_data.size();
    for (size_t i = 0; i < file_data.size() - 8; ++i) {
        if (file_data[i] == 'l' && file_data[i + 1] == 'i' &&
            file_data[i + 2] == 's' && file_data[i + 3] == 't' &&
            file_data[i + 4] == 'm' && file_data[i + 5] == 'o' &&
            file_data[i + 6] == 'v' && file_data[i + 7] == 'i') {
            movi_pos = i;
            break;
        }
    }
    size_t movi_end = std::min(movi_pos + AVI_MOVI_LIST_PROTECT_SIZE, file_data.size());
    std::fill(protected_mask.begin() + movi_pos, protected_mask.begin() + movi_end, true);
	vector<size_t> frame_starts = findPotentialFrameStarts();
    frmcount = frame_starts.size();
    // 保护已检测到的帧头
    for (auto pos : frame_starts) {
        size_t frame_end = std::min(pos + AVI_FRAME_HEADER_SIZE, file_data.size());
        std::fill(protected_mask.begin() + pos, protected_mask.begin() + frame_end, true);
    }
}

bool AVICorruptor::loadFile(const std::string& filename) {
	string file_ext = filename.substr(filename.find_last_of('.') + 1);
    if(file_ext != "avi" && file_ext != "AVI"){
        std::cerr << "Error: Not an AVI file: " << filename << std::endl;
        return false;
	}
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return false;
    }

    file_data.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(file_data.data()), file_data.size());
    file.close();

    precomputeProtectedMask();
    std::cout << "Loaded AVI file (" << file_data.size() << " bytes)" << std::endl;
    return true;
}

bool AVICorruptor::saveFile(const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Error creating output file: " << filename << std::endl;
        return false;
    }
    out.write(reinterpret_cast<char*>(file_data.data()), file_data.size());
    out.close();
    return true;
}

void AVICorruptor::applyCorruption() {
    std::cout << "Starting corruption process..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    auto frame_starts = findPotentialFrameStarts();
    std::cout << "Found " << frame_starts.size() << " potential frame starts" << std::endl;

    for (size_t stage_idx = 0; stage_idx < stages.size(); ++stage_idx) {
        const auto& stage = stages[stage_idx];
        size_t start = static_cast<size_t>(stage.start_ratio * file_data.size());
        size_t end = static_cast<size_t>(stage.end_ratio * file_data.size());
        size_t target_glitches = static_cast<size_t>(stage.intensity * frmcount);
        
        std::cout << "Stage " << (stage_idx + 1) << ": "
            << (stage.start_ratio * 100) << "% - "
            << (stage.end_ratio * 100) << "% intensity "
            << (stage.intensity * 100) << "%, target " << target_glitches
            << " glitches" << std::endl;

        std::vector<size_t> corruption_positions;
        corruption_positions.reserve(target_glitches);
        std::uniform_int_distribution<> pos_dist(start, end - 1);
        
        // 生成破坏位置
        for (size_t i = 0; i < target_glitches; ++i) {
            size_t pos;
            do {
                pos = pos_dist(rng);
            } while (protected_mask[pos]);
            corruption_positions.push_back(pos);
        }

        // 批量破坏
        stage_idx = stage_idx > 6 ? 6 : stage_idx;
        std::uniform_int_distribution<int> dist(min(0,(int)stage_idx-2), stage_idx);
        

        size_t processed = 0;
        //glitch size (bytes)
		int burst_size = stage.burst_size;
        size_t report_interval = min((size_t)AVI_PROGRESS_REPORT_INTERVAL, target_glitches);
        std::uniform_int_distribution<int> byte_dist(0, 255);
        std::uniform_int_distribution<int> flip_dist(0, 7);
        std::uniform_int_distribution<int> dir_dist(0, 1);
        while (processed < target_glitches) {
            size_t chunk = min(report_interval, target_glitches - processed);
            std::vector<size_t> chunk_list(corruption_positions.begin() + processed,
                corruption_positions.begin() + processed + chunk);
            
            for (auto pos : chunk_list) {
                //int rand_val = dist(rng);
                // 随机破坏方式
                int rand_val = 4;
                int bit_pos;
                switch (rand_val) {
                case 0:
                    for (int j = 0; j < burst_size; j++) {
                        // bit flip
                        bit_pos = flip_dist(rng);
                        if (!protected_mask[pos + j])file_data[pos+j] ^= (1 << bit_pos);
                    }
                    break;
                case 1:
                    for (int j = 0; j < burst_size; j++) {
                    // low bits substitution
                        if (!protected_mask[pos + j]) file_data[pos + j] = (file_data[pos + j] & 0xF0) | (static_cast<uint8_t>(byte_dist(rng)) & 0x0F);
                    }
                    break;
                case 2:
                    for (int j = 0; j < burst_size; j++) {
                    // set to 0x80 (gray)
                        if (!protected_mask[pos + j]) file_data[pos + j] = 0x80;
                    }
                    break;
                case 3:
                    for (int j = 0; j < burst_size; j++) {
                        // shift
                        if (!protected_mask[pos + j]) {
                            if (dir_dist(rng)) {
                                file_data[pos + j] <<= 1;
                            }
                            else {
                                file_data[pos + j] >>= 1;
                            }
                        }
                    }
                    break;
                case 4:
                    // lag simulation
                    for (int j = 0; j < burst_size; j++) {
                        if (!protected_mask[pos + j])file_data[pos + j] = file_data[pos];
                    }
                    break;
                
                case 5:
					// voltage spike simulation
                    for (int j = 0; j < burst_size; j++) {
                        if (!protected_mask[pos + j])file_data[pos + j] ^= 0x55;
                    }
                    break;
                case 6:
                    // copying from previous location

                    size_t copy_offset;

                    copy_offset = 5000 + pos_dist(rng) % 20000;

                    for (int j = 0; j < burst_size; j++) {
                        if (!protected_mask[pos + j])file_data[pos + j] = file_data[pos - copy_offset + j];
                    }
                    break;
                }

            }

            processed += chunk;
            double progress = (static_cast<double>(processed) / target_glitches) * 100.0;
            std::cout << "\rProgress: " << std::fixed << std::setprecision(1)
                << progress << "%" << std::flush;
        }
        std::cout << std::endl;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Corruption completed in " << duration.count() << "ms" << std::endl;
}

void AVICorruptor::printFileInfo() {
    std::cout << "File size: " << file_data.size() << " bytes" << std::endl;
    std::cout << "Stages: " << stages.size() << std::endl;
    for (size_t i = 0; i < stages.size(); ++i) {
        std::cout << "Stage " << (i + 1) << ": "
            << stages[i].start_ratio * 100 << "% - "
            << stages[i].end_ratio * 100 << "% intensity "
            << stages[i].intensity * 100 << "%" << std::endl;
    }
    std::cout << "Protected regions:" << std::endl;
    std::cout << "- Header: " << AVI_HEADER_PROTECT_SIZE << " bytes" << std::endl;
    std::cout << "- MOVI list: " << AVI_MOVI_LIST_PROTECT_SIZE << " bytes" << std::endl;
    std::cout << "- Frame headers: " << AVI_FRAME_HEADER_SIZE << " bytes" << std::endl;
}