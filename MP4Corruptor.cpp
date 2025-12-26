// MP4Corruptor.cpp
//

#include "MP4Corruptor.h"

using namespace std;


bool MP4Corruptor::loadFile(const std::string& filename) {
    string file_ext = filename.substr(filename.find_last_of('.') + 1);
    if (file_ext != "mp4" && file_ext != "MP4") {
        std::cerr << "Error: Not an MP4 file: " << filename << std::endl;
        return false;
    }
    ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filename << std::endl;
        return false;
    }

    streamsize size = file.tellg();
    //move cursor to beginning
    file.seekg(0, ios::beg);

    file_data.resize(size);
    if (!file.read(reinterpret_cast<char*>(file_data.data()), size)) {
        cerr << "读取文件失败" << std::endl;
        return false;
    }
	//initialize frame count
    frmcount = 0;
    mdat_atoms = getMdatInfo();

    for(size_t i=0;i<mdat_atoms.size();i++){
        cout << "Found mdat atom at offset " << mdat_atoms[i].offset 
             << " with size " << mdat_atoms[i].size 
             << (mdat_atoms[i].if_extended ? " (64-bit size)" : " (32-bit size)") << std::endl;
	}

	// compute protected mask
    precomputeProtectedMask();

    cout << "成功加载文件，大小: " << size << " 字节" << std::endl;
    return true;
}


bool MP4Corruptor::saveFile(const std::string& filename) {
    ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "无法创建输出文件: " << filename << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
    return true;
}

//get mdat info
vector<MP4Corruptor::MdatInfo> MP4Corruptor::getMdatInfo() {
	vector<MdatInfo> mdat_atoms;
    size_t file_size = file_data.size();
    // find mdat atom in file

    for (size_t i = 0; i < file_size - 8; ++i) {
        MdatInfo info = { 0, 0 ,0};
		// check mdat signature
        if (file_data[i] == 'm' && file_data[i + 1] == 'd' &&
            file_data[i + 2] == 'a' && file_data[i + 3] == 't') {

			//cout << "found mdat at " << i << endl;
            
            // atom_size includes header size
            uint32_t atom_size = (file_data[i - 4] << 24) |
                (file_data[i - 3] << 16) |
                (file_data[i - 2] << 8) |
                file_data[i - 1];


			// if 64-bit size
            if (atom_size == 1) {
                if (i + 16 > file_size) break; // 
                uint64_t extended_size =
                    ((uint64_t)file_data[i + 8] << 56) |
                    ((uint64_t)file_data[i + 9] << 48) |
                    ((uint64_t)file_data[i + 10] << 40) |
                    ((uint64_t)file_data[i + 11] << 32) |
                    ((uint64_t)file_data[i + 12] << 24) |
                    ((uint64_t)file_data[i + 13] << 16) |
                    ((uint64_t)file_data[i + 14] << 8) |
                    ((uint64_t)file_data[i + 15]);
                info.size = extended_size; // 16字节头部
                info.offset = i - 8; // 原子头起始位置
                info.if_extended = 1;
            }
            else {
                info.size = atom_size;
                info.offset = i - 4; // 原子头起始位置
                info.if_extended = 0;
            }
            mdat_atoms.push_back(info);
        }

    }
    return mdat_atoms;
}

void MP4Corruptor::precomputeProtectedMask() {
    protected_mask.resize(file_data.size(), false);

    // 保护文件头区域
    for (size_t i = 0; i < min(size_t(1024), file_data.size()); i++) {
        protected_mask[i] = true;
    }

    // protect moov atom
    for (size_t i = 4; i < file_data.size() - 8; i++) {
        if (file_data[i] == 'm' && file_data[i + 1] == 'o' &&
            file_data[i + 2] == 'o' && file_data[i + 3] == 'v') {
            uint32_t atom_size = (file_data[i - 4] << 24) | (file_data[i - 3] << 16) |
                (file_data[i - 2] << 8) | file_data[i - 1];
            size_t start = i - 4;
            size_t end = min(i + atom_size, file_data.size());
            for (size_t j = start; j < end; j++) {
                if (j < protected_mask.size()) protected_mask[j] = true;
            }
        }
    }

    // protect ftyp atom
    for (size_t i = 4; i < file_data.size() - 8; i++) {
        if (file_data[i] == 'f' && file_data[i + 1] == 't' &&
            file_data[i + 2] == 'y' && file_data[i + 3] == 'p') {
            uint32_t atom_size = (file_data[i - 4] << 24) | (file_data[i - 3] << 16) |
                (file_data[i - 2] << 8) | file_data[i - 1];
            size_t start = i - 4;
            size_t end = min(i + atom_size, file_data.size());
            for (size_t j = start; j < end; j++) {
                if (j < protected_mask.size()) protected_mask[j] = true;
            }
        }
    }

    //protect mdat header
    for(MdatInfo &mdat : mdat_atoms){
        if (mdat.if_extended == 1) {
			//16 bytes header
            for (size_t j = mdat.offset; j < mdat.offset+16 ; j++) {
                if (j < protected_mask.size()) protected_mask[j] = true;
            }
        }
        else {
            //8 bytes header
            for (size_t j = mdat.offset; j < mdat.offset + 8; j++) {
                if (j < protected_mask.size()) protected_mask[j] = true;
            }
        }
	}

    // protect frame start
    auto frame_starts = findPotentialFrameStarts();
    frmcount += frame_starts.size();
    for (size_t frame_start : frame_starts) {
        size_t end = min(frame_start + MP4_FRAME_HEADER_PROTECT_SIZE, file_data.size());
        for (size_t j = frame_start; j < end; j++) {
            if (j < protected_mask.size()) protected_mask[j] = true;
        }
    }

    // protect audio frame start
    auto audio_starts = findPotentialAudioFrameStarts();
    frmcount += audio_starts.size();
    for (size_t audio_start : audio_starts) {
        size_t end = min(audio_start + MP4_AUDIO_FRAME_HEADER_PROTECT_SIZE, file_data.size());
        for (size_t j = audio_start; j < end; j++) {
            if (j < protected_mask.size()) protected_mask[j] = true;
        }
    }

	//protect SPS/PPS NALUs
    //protectCriticalRegions();
}

// check potential frame start positions
vector<size_t> MP4Corruptor::findPotentialFrameStarts() {
	//stores the potential frame start positions
    std::vector<size_t> frame_starts;
    for (size_t i = 1024; i < file_data.size() - 8; i++) {
        // 检查NALU起始码
        if (file_data[i] == 0x00 && file_data[i + 1] == 0x00) {
			if (file_data[i + 2] == 0x01) { // 3-bit start code
                frame_starts.push_back(i);
				i += 3; // skip ahead
            }
            else if (i + 3 < file_data.size() && file_data[i + 2] == 0x00 && file_data[i + 3] == 0x01) { 
                // 4-bit start code
                frame_starts.push_back(i);
                i += 4; // skip ahead
            }
        }
    }

    // 去重并排序
    std::sort(frame_starts.begin(), frame_starts.end());
    frame_starts.erase(std::unique(frame_starts.begin(), frame_starts.end()), frame_starts.end());

    // 确保帧之间有最小间隔
    std::vector<size_t> filtered_starts;
    size_t last_start = 0;
    for (size_t pos : frame_starts) {

        if (pos - last_start >= MP4_MIN_FRAME_INTERVAL || filtered_starts.empty()) {
            if(last_start!=0) filtered_starts.push_back(last_start);
            last_start = pos;
        }
    }

    return filtered_starts;
}

// check potential audio frame start positions
vector<size_t> MP4Corruptor::findPotentialAudioFrameStarts() {
    std::vector<size_t> frame_starts;

    // 查找常见音频帧同步字
    for (size_t i = 0; i < file_data.size() - 4; i++) {
        // AAC ADTS同步字 (0xFFFx)
        if ((file_data[i] == 0xFF) && ((file_data[i + 1] & 0xF0) == 0xF0)) {
            frame_starts.push_back(i);
        }
        // MP3帧同步字 (0xFFEx)
        else if ((file_data[i] == 0xFF) && ((file_data[i + 1] & 0xE0) == 0xE0)) {
            frame_starts.push_back(i);
        }
        // ALAC帧同步字
        else if (memcmp(file_data.data() + i, "alac", 4) == 0) {
            frame_starts.push_back(i);
        }
        // FLAC帧同步字 (0xFLAC)
        else if (i + 4 < file_data.size() &&
            file_data[i] == 0x66 && file_data[i + 1] == 0x4C &&
            file_data[i + 2] == 0x61 && file_data[i + 3] == 0x43) {
            frame_starts.push_back(i);
        }
    }

    // 去重并排序
    std::sort(frame_starts.begin(), frame_starts.end());
    frame_starts.erase(std::unique(frame_starts.begin(), frame_starts.end()), frame_starts.end());

    // 确保帧之间有最小间隔
    std::vector<size_t> filtered_starts;
    size_t last_start = 0;
    for (size_t pos : frame_starts) {
        if (pos - last_start >= MP4_MIN_AUDIO_FRAME_INTERVAL || filtered_starts.empty()) {
            filtered_starts.push_back(pos);
            last_start = pos;
        }
    }

    return filtered_starts;
}

// 批量破坏函数
void MP4Corruptor::corruptBytesBatch(const std::vector<size_t>& positions, double intensity, int phase,int burst_size) {
	phase = phase > 6 ? 6 : phase;
    std::uniform_int_distribution<int> dist(min(0, (int)phase - 3), phase);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_int_distribution<int> flip_dist(0, 7);
    std::uniform_int_distribution<int> dir_dist(0, 1);
    std::uniform_int_distribution<int> x_dist(0, 30000);

    int bit_pos;
    for (size_t pos : positions) {

        if (pos >= file_data.size() || protected_mask[pos]) continue;
        int rand_val = dist(rng);

        //int rand_val = 6;
        uint8_t original = file_data[pos];
        
        switch (rand_val) {
        case 0:
            for (int j = 0; j < burst_size; j++) {
                // bit flip
                bit_pos = flip_dist(rng);
                if (!protected_mask[pos + j])file_data[pos + j] ^= (1 << bit_pos);
            }
            break;
        case 1:
            for (int j = 0; j < burst_size; j++) {
                // low bits substitution
                if (!protected_mask[pos + j]) file_data[pos + j] = (file_data[pos + j] & 0xFC) | (static_cast<uint8_t>(byte_dist(rng)) & 0x03);
            }
            break;
        case 2:
            for (int j = 0; j < burst_size; j++) {
                // set to zero
                if (!protected_mask[pos + j]) file_data[pos + j] = 0;
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
			// Invert bits (voltage spike simulation)
            for (int j = 0; j < burst_size; j++) {
                if (!protected_mask[pos + j])file_data[pos + j] ^= 0xFF;
            }
            break;
        case 6:
            // copying from previous location

            size_t copy_offset;
            for (int j = 0; j < burst_size; j++) {
                copy_offset = 5000 + x_dist(rng);
                if (!protected_mask[pos + j])file_data[pos + j] = file_data[pos - copy_offset + j];
            }
            break;
        }
    }
}

void MP4Corruptor::applyCorruption() {
    std::cout << "Corruption start..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    auto frame_starts = findPotentialFrameStarts();
    
    std::cout << "检测到 " << frame_starts.size() << " 个大于"<<MP4_MIN_FRAME_INTERVAL<<"字节的NALU单元" << std::endl;

    auto audio_starts = findPotentialAudioFrameStarts();
    std::cout << "检测到 " << audio_starts.size() << " 个可能的音频帧起始位置" << std::endl;

    for (int i = 0; i < stages.size();i++) {
        const auto& stage = stages[i];
        auto stage_start = std::chrono::high_resolution_clock::now();
        vector<size_t> start_pos_list,end_pos_list,region_size_list;
        size_t start_pos;
        size_t end_pos;
        for (const auto& mdat : mdat_atoms) {
            start_pos = static_cast<size_t>(mdat.offset + stage.start_ratio * mdat.size);
			end_pos = static_cast<size_t>(mdat.offset + stage.end_ratio * mdat.size);
            start_pos_list.push_back(start_pos);
            end_pos_list.push_back(end_pos);
			region_size_list.push_back(end_pos - start_pos);
        }

        size_t glitches = static_cast<size_t>(max(frmcount * stage.intensity, 50* stage.end_ratio));

        std::cout << "阶段: " << stage.start_ratio * 100 << "% - "
            << stage.end_ratio * 100 << "%, 强度: " << stage.intensity * 100
            << "%, 目标破坏: " << glitches << " glitch" << std::endl;


        // 生成破坏位置
        vector<size_t> corruption_positions;
        corruption_positions.reserve(glitches);

		discrete_distribution<int> mdat_select(region_size_list.begin(), region_size_list.end());
        vector<uniform_int_distribution<size_t>> pos_dist_list;
        
        for (int x = 0; x < mdat_atoms.size(); x++) {
			cout << "mdat:"<<x<<" start position: " << start_pos_list[x] << " - end position: " << end_pos_list[x] << endl;
			pos_dist_list.push_back(uniform_int_distribution<size_t>(start_pos_list[x], end_pos_list[x] - 1));
			
        }

        // 生成所有随机位置
        for (size_t i = 0; i < glitches; i++) {
			int mdat_index = mdat_select(rng);
			
            size_t pos = pos_dist_list[mdat_index](rng);

            //cout << "current position: " << pos << endl;
            corruption_positions.push_back(pos);
        }
        

        // 批量破坏所有字节
        size_t total_processed = 0;
        size_t report_threshold = min((size_t)MP4_PROGRESS_REPORT_INTERVAL, glitches);

        while (total_processed < glitches) {
            size_t chunk_size = min(static_cast<size_t>(100), glitches - total_processed);
            std::vector<size_t> chunk(corruption_positions.begin() + total_processed,
                corruption_positions.begin() + total_processed + chunk_size);

            corruptBytesBatch(chunk, stage.intensity,i, stage.burst_size);
            total_processed += chunk_size;

            // 进度报告
            if (total_processed >= report_threshold) {
                double progress = static_cast<double>(total_processed) / glitches * 100.0;
                std::cout << "\r阶段进度: " << fixed << setprecision(2)
                    << progress << "% (" << total_processed << "/" << glitches << ")";
                report_threshold += MP4_PROGRESS_REPORT_INTERVAL;
            }
        }

        std::cout << "\n阶段完成: " << glitches << "/" << glitches << std::endl;

        auto stage_end = std::chrono::high_resolution_clock::now();
        auto stage_duration = std::chrono::duration_cast<std::chrono::milliseconds>(stage_end - stage_start);
        std::cout << "阶段耗时: " << stage_duration.count() << "ms" << std::endl;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "破坏完成! 总耗时: " << total_duration.count() << "ms" << std::endl;
}

void MP4Corruptor::printFileInfo() {
    std::cout << "文件大小: " << file_data.size() << " 字节" << std::endl;
    std::cout << "破坏阶段数: " << stages.size() << std::endl;

    for (size_t i = 0; i < stages.size(); i++) {
        std::cout << "阶段 " << i + 1 << ": " << stages[i].start_ratio * 100 << "%-"
            << stages[i].end_ratio * 100 << "%, 强度 " << stages[i].intensity * 100 << "%" << std::endl;
    }

    std::vector<size_t> frame_starts = findPotentialFrameStarts();
    std::cout << "每个音频/视频帧头部保护字节数: " << MP4_FRAME_HEADER_PROTECT_SIZE << " 字节" << std::endl;
}

// VPS/SPS/PPS保护（H.264/H.265）
/*
void MP4Corruptor::protectCriticalRegions() {
    for (size_t i = 0; i < file_data.size(); ) {
        // 检测NAL单元起始码（支持3/4字节）
        size_t start_code_len = 4;
        if (i + 4 > file_data.size()) break;
        if (file_data[i] == 0x00 && file_data[i + 1] == 0x00 &&
            file_data[i + 2] == 0x00 && file_data[i + 3] == 0x01) {
            // 正常4字节起始码
        }
        else if (i + 3 <= file_data.size() &&
            file_data[i] == 0x00 && file_data[i + 1] == 0x00 &&
            file_data[i + 2] == 0x01) {
            start_code_len = 3; // 处理3字节起始码
        }
        else {
            i++;
            continue;
        }

        // 解析NAL单元类型
        size_t nal_pos = i + start_code_len;
        if (nal_pos >= file_data.size()) break;
        uint8_t nal_header = file_data[nal_pos];

        // 判断H.264/H.265格式
        bool is_h265 = (nal_header & 0x80) != 0; // H.265的NAL头高位标志位

        // 获取NAL单元类型
        uint8_t nal_type;
        if (is_h265) {
            nal_type = (nal_header >> 1) & 0x3F; // H.265类型在高位
        }
        else {
            nal_type = nal_header & 0x1F; // H.264类型在低5位
        }

        // 定义需要保护的NAL单元类型
        std::vector<uint8_t> protected_types;
        if (is_h265) {
            protected_types = { 32, 33, 34 }; // VPS/SPS/PPS
        }
        else {
            protected_types = { 7, 8 }; // SPS/PPS (H.264无VPS)
        }

        // 检查是否是需要保护的类型
        if (std::find(protected_types.begin(), protected_types.end(), nal_type) == protected_types.end()) {
            i += start_code_len + 1; // 跳过起始码和NAL头
            continue;
        }

        // 计算NAL单元长度
        size_t length = 0;
        if (is_h265) {
            // H.265长度字段（4字节大端序）
            if (nal_pos + 5 > file_data.size()) break;
            length = (file_data[nal_pos + 1] << 24) |
                (file_data[nal_pos + 2] << 16) |
                (file_data[nal_pos + 3] << 8) |
                file_data[nal_pos + 4];
        }
        else {
            // H.264长度字段（4字节大端序）
            if (nal_pos + 4 > file_data.size()) break;
            length = (file_data[nal_pos + 1] << 24) |
                (file_data[nal_pos + 2] << 16) |
                (file_data[nal_pos + 3] << 8) |
                file_data[nal_pos + 4];
        }

        // 计算保护区域
        size_t protect_start = std::max(nal_pos - 16, (size_t)0);
        size_t protect_end = std::min(nal_pos + 4 + length + 16, file_data.size());

        // 应用保护
        for (size_t j = protect_start; j < protect_end; ++j) {
            if (j < protected_mask.size()) {
                protected_mask[j] = true;
            }
        }

        // 跳过当前NAL单元
        i += start_code_len + 1 + length;
    }
}
*/
