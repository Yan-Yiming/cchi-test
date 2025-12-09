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
        uint8_t data[CACHE_LINE_SIZE];
    
        // 构造函数：初始化时填充随机数据，模拟内存的初始状态
        inline globalBoardEntry() {
            for (int i = 0; i < CACHE_LINE_SIZE; ++i) {
                data[i] = (uint8_t)rand(); 
            }
        }
    
        // 更新数据 (模拟写回内存 WriteBack)
        // input: new_data 指向 64 字节的完整数据
        void update(const uint8_t* new_data) {
            if (new_data) {
                std::memcpy(this->data, new_data, CACHE_LINE_SIZE);
            }
        }
    
        // 校验数据 (模拟读取验证 Read Verify)
        // input: dut_data 是 DUT (Device Under Test) 读回来的数据
        bool verify(const uint8_t* dut_data) const {
            if (std::memcmp(this->data, dut_data, CACHE_LINE_SIZE) != 0) {
                print_error(dut_data);
                return false;
            }
            return true;
        }
    
    private:
        // 辅助函数：打印数据不一致的详细信息
        void print_error(const uint8_t* dut_data) const {
            std::cerr << "[ScoreBoard] Error: Data Mismatch!" << std::endl;
            std::cerr << "  Expected (Golden): ";
            for (int i = 0; i < CACHE_LINE_SIZE; ++i) 
                std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)this->data[i] << " ";
            std::cerr << std::endl;
    
            std::cerr << "  Actual   (DUT)   : ";
            for (int i = 0; i < CACHE_LINE_SIZE; ++i) 
                std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)dut_data[i] << " ";
            std::cerr << std::dec << std::endl; // 恢复十进制
        }
};
    