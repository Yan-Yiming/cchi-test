// cchi-test/main/DUT/MockL2Cache.h
#pragma once // 防止头文件被重复包含

#include <cstdint>
#include <map>
#include <array>
#include <deque>
#include <memory>
#include <iostream>
#include <cstring>
#include "../CCHI/cchi_base.hpp"
#include "../CCHI/cchi_opcode.hpp"

// =============================================================================
// 类: MockL2Cache
// -----------------------------------------------------------------------------
// 这是一个模拟的二级缓存 (L2 Cache) 或内存控制器。
// 角色: Completer (服务端)
// 功能: 接收 Agent 发来的读写请求，并按照 Compact CHI 协议返回数据或响应。
// =============================================================================
class MockL2Cache {
public:
    // 构造函数: 初始化组件
    MockL2Cache();
    // 析构函数: 清理资源 (这里使用默认的即可)
    ~MockL2Cache() = default;

    // -------------------------------------------------------------------------
    // 接口组 1: 接收输入 (Input Interface)
    // -------------------------------------------------------------------------
    // 对应硬件上的: Agent Output -> DUT Input
    // 返回值 bool: 表示是否成功接收 (模拟 Ready 信号)
    // -------------------------------------------------------------------------
    bool accept_txreq(const CCHI::BundleChannelREQ& req); // 接收请求 (Read/Write)
    bool accept_txevt(const CCHI::BundleChannelEVT& evt); // 接收事件 (WriteBack/Evict)
    bool accept_txdat(const CCHI::BundleChannelDAT& dat); // 接收写数据
    bool accept_txrsp(const CCHI::BundleChannelRSP& rsp); // 接收响应 (Snoop Response)

    // -------------------------------------------------------------------------
    // 接口组 2: 发送输出 (Output Interface)
    // -------------------------------------------------------------------------
    // 对应硬件上的: DUT Output -> Agent Input
    // 这里的逻辑是 "Try Get" (尝试获取)，如果队列里有数据就拿出来返回 true
    // -------------------------------------------------------------------------
    bool try_get_rxsnp(CCHI::BundleChannelSNP& snp); // 尝试获取发给 Agent 的 Snoop
    bool try_get_rxrsp(CCHI::BundleChannelRSP& rsp); // 尝试获取发给 Agent 的 响应
    bool try_get_rxdat(CCHI::BundleChannelDAT& dat); // 尝试获取发给 Agent 的 读数据

    // -------------------------------------------------------------------------
    // 时序控制
    // -------------------------------------------------------------------------
    // 模拟时钟沿。每一拍调用一次，用于处理延时、内部状态跳转等
    void tick(uint64_t cycles);

private:
    // =========================================================================
    // 内部存储模型 (Storage Model)
    // =========================================================================
    // 使用 std::map 模拟巨大的内存空间。
    // Key (uint64_t): 内存地址 (Address)
    // Value (std::array): 64字节的数据块 (Cache Line)
    // 优点: 稀疏存储。只有真正写过的地址才占用内存，不用申请几 GB 的数组。
    std::map<uint64_t, std::array<uint8_t, 64>> memory;

    // =========================================================================
    // 事务追踪 (Transaction Tracking)
    // =========================================================================
    // CHI 协议中，写操作是分两步的：
    // 1. 发送 WriteReq -> 得到 DBID (Data Buffer ID)
    // 2. 发送 WriteData (带上这个 DBID)
    // 所以 DUT 必需记住 "哪个 DBID 对应哪个地址"。
    struct WriteContext {
        uint64_t addr;                  // 这是一个写往哪里的操作？
        uint8_t  opcode;                // 写操作类型
        int      received_beats;        // 已经收到了几拍数据？(总共需要2拍)
        std::array<uint8_t, 64> data_buffer; // 临时拼凑数据的缓冲区
    };

    // 记录当前活跃的写操作: Key 是 DBID
    std::map<uint16_t, WriteContext> active_writes; 
    
    // 用于分配下一个可用的 DBID
    uint16_t next_dbid = 0;

    // =========================================================================
    // 输出缓冲队列 (Output Queues)
    // =========================================================================
    // 硬件上通常有 FIFO 队列。这里用 deque 模拟。
    // 当处理完一个读请求，结果不会立即消失，而是放在这里排队等待发送。
    std::deque<CCHI::BundleChannelSNP> snp_queue;
    std::deque<CCHI::BundleChannelRSP> rsp_queue;
    std::deque<CCHI::BundleChannelDAT> dat_queue;

    // =========================================================================
    // 内部处理函数 (Internal Logic)
    // =========================================================================
    void process_read(const CCHI::BundleChannelREQ& req);
    void process_write_req(const CCHI::BundleChannelREQ& req);
    void process_write_evt(const CCHI::BundleChannelEVT& evt);
    void process_write_data(const CCHI::BundleChannelDAT& dat);
    
    uint16_t alloc_dbid(); // 分配一个新的 ID
    void write_to_memory(uint64_t addr, const uint8_t* data); // 写 map
    const uint8_t* read_from_memory(uint64_t addr);           // 读 map
};