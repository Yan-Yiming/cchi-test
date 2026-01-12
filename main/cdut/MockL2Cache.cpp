// cchi-test/main/DUT/MockL2Cache.cpp
#include "MockL2Cache.h"
#include <iomanip> // 用于格式化打印 (hex, dec)

// -----------------------------------------------------------------------------
// 构造函数
// -----------------------------------------------------------------------------
MockL2Cache::MockL2Cache() {
    // 为了方便调试，我们可以在启动时预先在内存里填一点数据。
    // 例如：地址 0x8000 处填满 0xAA
    std::array<uint8_t, 64> pattern;
    pattern.fill(0xAA); 
    memory[0x8000] = pattern;
}

// =============================================================================
// 接口实现部分
// =============================================================================

// 处理请求通道 (REQ)
bool MockL2Cache::accept_txreq(const CCHI::BundleChannelREQ& req) {
    // 根据 Opcode (操作码) 决定做什么
    switch ((CCHIOpcodeREQ)req.opcode) {
        // --- 读类操作 ---
        case CCHIOpcodeREQ::ReadNoSnp:
        case CCHIOpcodeREQ::ReadOnce:
        case CCHIOpcodeREQ::ReadShared:
        case CCHIOpcodeREQ::ReadUnique:
        case CCHIOpcodeREQ::MakeReadUnique:
            process_read(req); // 调用读处理函数
            break;

        // --- 写类操作 ---
        case CCHIOpcodeREQ::WriteNoSnpFull:
        case CCHIOpcodeREQ::WriteNoSnpPtl:
        case CCHIOpcodeREQ::WriteUniqueFull:
        case CCHIOpcodeREQ::WriteUniquePtl:
            process_write_req(req); // 调用写请求处理函数
            break;

        default:
            std::cout << "[DUT] 警告: 收到未实现的 REQ Opcode: " 
                      << std::hex << (int)req.opcode << std::dec << std::endl;
            break;
    }
    return true; // 总是返回 true，表示我可以接收 (Always Ready)
}

// 处理事件通道 (EVT - 用于 WriteBack/Evict)
bool MockL2Cache::accept_txevt(const CCHI::BundleChannelEVT& evt) {
    switch ((CCHIOpcodeEVT)evt.opcode) {
        case CCHIOpcodeEVT::WriteBackFull:
            // WriteBack 实际上也是一种写操作，需要回写数据
            process_write_evt(evt);
            break;
        case CCHIOpcodeEVT::Evict:
            // Evict 只是通知 Cache 这一行被驱逐了，不需要传数据
            // 我们只需要回复一个 Comp (完成) 信号
            {
                CCHI::BundleChannelRSP rsp;
                rsp.txnID = evt.txnID;
                rsp.opcode = (uint8_t)CCHIOpcodeRSP_UP::Comp;
                rsp.resp = 0;
                rsp_queue.push_back(rsp); // 放入响应队列
            }
            break;
        default:
            break;
    }
    return true;
}

// 处理数据通道 (DAT)
bool MockL2Cache::accept_txdat(const CCHI::BundleChannelDAT& dat) {
    process_write_data(dat); // 处理真正的数据传输
    return true;
}

// 处理响应通道 (RSP)
bool MockL2Cache::accept_txrsp(const CCHI::BundleChannelRSP& rsp) {
    // 在简单的 L2 Mock 中，我们可能不需要处理 Requester 发回的 Ack。
    // 但在复杂模型中，这里需要用来释放资源。
    (void)rsp; // 避免 "unused parameter" 警告
    return true;
}

// --- 下面是 "Try Get" 系列函数 ---
// 作用：如果内部队列有东西，就弹出一个给调用者 (Simulation Main Loop)

bool MockL2Cache::try_get_rxsnp(CCHI::BundleChannelSNP& snp) {
    if (snp_queue.empty()) return false;
    snp = snp_queue.front(); // 取队头
    snp_queue.pop_front();   // 删队头
    return true;
}

bool MockL2Cache::try_get_rxrsp(CCHI::BundleChannelRSP& rsp) {
    if (rsp_queue.empty()) return false;
    rsp = rsp_queue.front();
    rsp_queue.pop_front();
    return true;
}

bool MockL2Cache::try_get_rxdat(CCHI::BundleChannelDAT& dat) {
    if (dat_queue.empty()) return false;
    dat = dat_queue.front();
    dat_queue.pop_front();
    return true;
}

void MockL2Cache::tick(uint64_t cycles) {
    // 这是一个钩子函数。
    // 如果将来你想模拟 "读取需要 10 个周期"，可以在这里写计数器逻辑。
    // 目前这是空的，意味着处理是瞬时的 (Zero Latency)。
    (void)cycles;
}

// =============================================================================
// 核心逻辑实现 (The Brain)
// =============================================================================

// 逻辑 1: 处理读请求
void MockL2Cache::process_read(const CCHI::BundleChannelREQ& req) {
    // 1. 从模拟内存中读取 64字节 数据
    const uint8_t* data = read_from_memory(req.addr);
    
    // 2. 切分数据并打包发送
    // Compact CHI 数据通道宽度通常是 256bit (32 Byte)。
    // 一个 Cache Line 是 64 Byte，所以需要发 2 个包 (Beats)。
    for (int i = 0; i < 2; ++i) {
        CCHI::BundleChannelDAT respDat;
        respDat.txnID  = req.txnID; // 必须带上请求者的 ID，否则请求者不知道这是谁回的数据
        respDat.dbID   = 0;         // 读数据不需要 DBID
        respDat.opcode = (uint8_t)CCHIOpcodeDAT_UP::CompData; // Opcode: 完成并带数据
        respDat.resp   = 0;         // Resp: 0 表示 OK (无错误)
        
        // 内存拷贝: data是指针，+0 是低32字节，+32 是高32字节
        std::memcpy(respDat.data, data + (i * 32), 32);
        
        // 放入发送队列
        dat_queue.push_back(respDat);
    }
}

// 逻辑 2: 处理写请求 (第一阶段)
void MockL2Cache::process_write_req(const CCHI::BundleChannelREQ& req) {
    // 1. 分配一个 DBID (Data Buffer ID)
    // 就像你去餐厅吃饭，服务员给你个号码牌，说 "等下送菜(数据)来的时候报这个号"
    uint16_t dbid = alloc_dbid();
    
    // 2. 记录案底 (Context)
    // 我们需要记住这个号码牌对应的是哪个地址的写操作
    WriteContext ctx;
    ctx.addr = req.addr;
    ctx.opcode = req.opcode;
    ctx.received_beats = 0; // 还没收到数据
    active_writes[dbid] = ctx;

    // 3. 回复 Requester: "我准备好了，请把数据发到这个 DBID"
    CCHI::BundleChannelRSP rsp;
    rsp.txnID  = req.txnID;
    rsp.dbID   = dbid; // 关键: 告诉对方用这个 ID 发数据
    rsp.opcode = (uint8_t)CCHIOpcodeRSP_UP::CompDBIDResp; // 允许发送数据
    rsp_queue.push_back(rsp);
}

// 逻辑 3: 处理 WriteBack (类似于写请求)
void MockL2Cache::process_write_evt(const CCHI::BundleChannelEVT& evt) {
    // 逻辑和 WriteReq 几乎一样：分配 ID -> 记录 -> 回复 DBIDResp
    uint16_t dbid = alloc_dbid();
    
    WriteContext ctx;
    ctx.addr = evt.addr;
    ctx.opcode = evt.opcode;
    ctx.received_beats = 0;
    active_writes[dbid] = ctx;

    CCHI::BundleChannelRSP rsp;
    rsp.txnID  = evt.txnID;
    rsp.dbID   = dbid;
    rsp.opcode = (uint8_t)CCHIOpcodeRSP_UP::CompDBIDResp;
    rsp_queue.push_back(rsp);
}

// 逻辑 4: 处理写数据 (第二阶段)
void MockL2Cache::process_write_data(const CCHI::BundleChannelDAT& dat) {
    // 注意: 对于写数据，dat.txnID 字段填的是 DBID！
    uint16_t dbid = dat.txnID; 
    
    // 1. 查表: 这个 DBID 对应哪个写操作？
    if (active_writes.find(dbid) == active_writes.end()) {
        std::cerr << "[DUT] 错误: 收到未知 DBID 的数据包: " << dbid << std::endl;
        return;
    }

    // 引用这个 Context (方便修改)
    WriteContext& ctx = active_writes[dbid];
    
    // 2. 将收到的 32字节 数据拼接到临时缓冲区
    int offset = ctx.received_beats * 32;
    if (offset < 64) {
        std::memcpy(ctx.data_buffer.data() + offset, dat.data, 32);
    }
    
    ctx.received_beats++;

    // 3. 检查是否收满了 2 拍 (64字节)
    if (ctx.received_beats >= 2) {
        // 数据收齐了！写入真正的模拟内存
        write_to_memory(ctx.addr, ctx.data_buffer.data());
        
        // 任务完成，销毁案底
        active_writes.erase(dbid);
        
        // Debug 信息
        // std::cout << "[DUT] 写入完成. Addr: 0x" << std::hex << ctx.addr << std::dec << std::endl;
    }
}

// 辅助函数: 分配 ID
uint16_t MockL2Cache::alloc_dbid() {
    return next_dbid++; // 简单的自增，不做溢出处理 (Demo用)
}

// 辅助函数: 读内存 (带默认值处理)
const uint8_t* MockL2Cache::read_from_memory(uint64_t addr) {
    // 对齐地址到 64字节 边界 (Cache Line 对齐)
    uint64_t aligned_addr = addr & ~(0x3FULL);
    
    // 如果 map 里没这个地址，说明是第一次访问
    if (memory.find(aligned_addr) == memory.end()) {
        // 初始化一些随机数据，假装内存里原来就有东西
        std::array<uint8_t, 64> rand_data;
        for(auto& b : rand_data) b = rand() % 256;
        memory[aligned_addr] = rand_data;
    }
    // 返回数据指针
    return memory[aligned_addr].data();
}

// 辅助函数: 写内存
void MockL2Cache::write_to_memory(uint64_t addr, const uint8_t* data) {
    uint64_t aligned_addr = addr & ~(0x3FULL);
    std::array<uint8_t, 64> arr;
    std::memcpy(arr.data(), data, 64);
    memory[aligned_addr] = arr; // 存入 map
}