 #include <iostream>
 #include <vector>
 #include <cstring>
 #include <cassert>
 
 #include "CHISequencer.hpp"
 #include "DUT/MockL2Cache.h"
 
 #include "CCHI/cchi_base.hpp"
 
 #define MAX_SIM_CYCLES 10000
 
 int main(int argc, char** argv) {
     std::cout << "[Main] Initializing Compact CHI Verification Environment..." << std::endl;
 
     // ============================================================
     // 1. 组件实例化
     // ============================================================
     
     // 实例化 Sequencer (包含 Agent)
     CHISequencer* sequencer = new CHISequencer();
     
     // 实例化 DUT (模拟 L2 Cache / Memory)
     MockL2Cache* dut = new MockL2Cache();
 
     // 实例化物理通道 Bundle (信号线)
     // Agent 和 DUT 通过读写这个 Bundle 的成员变量来交换数据
     CCHI::FCBundle* bundle = new CCHI::FCBundle();
     
     // 将 Bundle 绑定到 Agent
     sequencer->fcagent->bindPort(bundle);
 
     // ============================================================
     // 2. 仿真主循环
     // ============================================================
     
     uint64_t current_time = 0;
     std::cout << "[Main] Simulation Started." << std::endl;
 
     while (sequencer->IsAlive() && current_time < MAX_SIM_CYCLES) {
         
         // --------------------------------------------------------
         // Phase 1: Upstream (DUT -> Agent)
         // DUT 产生的响应 (RSP/DAT/SNP) 驱动到 Bundle 上
         // --------------------------------------------------------
         
         // 1.1 处理 RX RSP (Response from Home)
         CCHI::BundleChannelRSP rsp_pkt;
         if (dut->try_get_rxrsp(rsp_pkt)) {
             bundle->rxrsp.valid  = true;
             bundle->rxrsp.txnID  = rsp_pkt.txnID;
             bundle->rxrsp.dbID   = rsp_pkt.dbID;
             bundle->rxrsp.opcode = rsp_pkt.opcode;
             bundle->rxrsp.resp   = rsp_pkt.resp;
         } else {
             bundle->rxrsp.valid = false;
         }
 
         // 1.2 处理 RX DAT (Read Data from Home)
         CCHI::BundleChannelDAT dat_pkt;
         if (dut->try_get_rxdat(dat_pkt)) {
             bundle->rxdat.valid  = true;
             bundle->rxdat.txnID  = dat_pkt.txnID;
             bundle->rxdat.dbID   = dat_pkt.dbID;
             bundle->rxdat.opcode = dat_pkt.opcode;
             bundle->rxdat.resp   = dat_pkt.resp;
             std::memcpy(bundle->rxdat.data, dat_pkt.data, 32); // 32 Bytes
         } else {
             bundle->rxdat.valid = false;
         }
 
         // 1.3 处理 RX SNP (Snoop from Home)
         CCHI::BundleChannelSNP snp_pkt;
         if (dut->try_get_rxsnp(snp_pkt)) {
             bundle->rxsnp.valid  = true;
             bundle->rxsnp.txnID  = snp_pkt.txnID;
             bundle->rxsnp.addr   = snp_pkt.addr;
             bundle->rxsnp.opcode = snp_pkt.opcode;
             // 处理其他 Snoop 字段...
         } else {
             bundle->rxsnp.valid = false;
         }
 
         // --------------------------------------------------------
         // Phase 2: Agent Execution
         // Agent 根据输入信号更新状态，并产生新的请求
         // --------------------------------------------------------
 
         // 设置 Agent 发送通道的 Ready 信号
         // 在此 Mock 环境中，假设 DUT 永远有能力接收请求 (Infinite Buffer)
         bundle->txreq.ready = true;
         bundle->txevt.ready = true;
         bundle->txdat.ready = true;
         bundle->txrsp.ready = true;
 
         // Sequencer 和 Agent 的时钟行为
         sequencer->Tick(current_time);
         
         // FIRE: 检查握手，更新状态机
         sequencer->fcagent->FIRE();
         
         // Random Test: 随机生成新的 Transaction
         sequencer->fcagent->random_test();
         
         // SEND: 将待发送的 Transaction 填入 Bundle (置 Valid = 1)
         sequencer->fcagent->SEND();
         
         sequencer->Tock();
 
 
         // --------------------------------------------------------
         // Phase 3: Downstream (Agent -> DUT)
         // 捕获 Agent 发出的 Valid 请求，传递给 DUT
         // --------------------------------------------------------
 
         // 3.1 捕获 TX REQ
         if (bundle->txreq.valid) {
             dut->accept_txreq(bundle->txreq);
             // 注意：在真实的 Verilog 仿真中，Valid 和 Ready 握手后，信号会在下一拍清除。
             // 这里的 C++ Agent 在下一次 FIRE() 时会检测到 valid && ready 并清除 valid。
         }
 
         // 3.2 捕获 TX EVT (WriteBack/Evict)
         if (bundle->txevt.valid) {
             dut->accept_txevt(bundle->txevt);
         }
 
         // 3.3 捕获 TX DAT (Write Data)
         if (bundle->txdat.valid) {
             dut->accept_txdat(bundle->txdat);
         }
 
         // 3.4 捕获 TX RSP (Snoop Response)
         if (bundle->txrsp.valid) {
             dut->accept_txrsp(bundle->txrsp);
         }
 
         // --------------------------------------------------------
         // Phase 4: DUT Execution & Time Advance
         // --------------------------------------------------------
         dut->tick(current_time);
         
         current_time++;
 
         // 定期打印进度
         if (current_time % 1000 == 0) {
             std::cout << "[Sim] Cycle: " << current_time << std::endl;
         }
     }
 
     std::cout << "[Main] Simulation Finished at cycle " << current_time << "." << std::endl;
 
     // ============================================================
     // 3. 清理内存
     // ============================================================
     delete bundle;
     delete dut;
     delete sequencer;
 
     return 0;
 }