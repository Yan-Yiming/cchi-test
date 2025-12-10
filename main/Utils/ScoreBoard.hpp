#include <cstdint>
#include <cstring> // for memcpy, memcmp
#include <iostream>
#include <iomanip> // for hex output
#include <vector>
#include "../CCHI/cchi_base.hpp"
#include "../CCHI/cchi_xact.h"

class localBoardEntry {
public:
    bool            inflight;
    bool            inflight_evt;
    bool            inflight_snp;
    bool            inflight_req;
    
    CCHI::CacheState    state;
    uint8_t             dirty;

    inline localBoardEntry(CCHI::CacheState state, uint8_t dirty) :
        state(state), dirty(dirty), inflight(false), inflight_evt(false), inflight_snp(false), inflight_req(false) { }
    
    // 默认析构函数即可
    inline ~localBoardEntry() = default;
};

constexpr int CACHE_LINE_SIZE = 64;

class globalBoardEntry {
    public:
        // 存储 64 字节的缓存行数据
        std::array<uint8_t, CACHE_LINE_SIZE> data;
    
        // 构造函数：初始化时填充随机数据，模拟内存的初始状态
        inline globalBoardEntry() {
            for (auto& byte : data) byte = static_cast<uint8_t>(rand());
        }

        explicit globalBoardEntry(const uint8_t* src_data) {
            if (src_data) {
                std::memcpy(data.data(), src_data, CACHE_LINE_SIZE);
            } else {
                // 异常情况给全0或随机
                 data.fill(0);
            }
        }

        // 更新数据 (模拟写回内存 WriteBack)
        // input: new_data 指向 64 字节的完整数据
        void update(const uint8_t* new_data) {
            if (new_data) {
                std::memcpy(data.data(), new_data, CACHE_LINE_SIZE);
            }
        }

        // 校验数据 (模拟 Read Verify)
        // 返回 true 表示校验通过
        bool verify(const uint8_t* dut_data, uint64_t addr = 0) const {
            if (std::memcmp(data.data(), dut_data, CACHE_LINE_SIZE) != 0) {
                print_error(dut_data, addr);
                return false;
            }
            return true;
        }
    
    private:
    void print_error(const uint8_t* dut_data, uint64_t addr) const {
        std::cerr << "\n[ScoreBoard] \033[1;31mError: Data Mismatch!\033[0m Address: 0x" 
                    << std::hex << addr << std::dec << std::endl;
        
        auto print_line = [](const char* label, const uint8_t* d) {
            std::cerr << label;
            for (size_t i = 0; i < CACHE_LINE_SIZE; ++i) {
                if (i % 16 == 0 && i != 0) std::cerr << "\n" << std::string(strlen(label), ' ');
                std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)d[i] << " ";
            }
            std::cerr << std::dec << std::endl;
        };

        print_line("  Expected (Golden): ", data.data());
        print_line("  Actual   (DUT)   : ", dut_data);
        std::cerr << std::endl;
    }
};
    