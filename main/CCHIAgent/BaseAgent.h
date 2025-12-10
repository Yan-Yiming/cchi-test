// cchi-test/main/CCHIAgent/BaseAgent.h
#pragma once

#include <memory>
#include <set>
#include <random>
#include <cstdint>
#include <assert.h>
#include <unordered_map>
#include <iostream>

#include "../CCHI/cchi_xact.h"
#include "../CCHI/cchi_base.hpp"
#include "../Utils/ScoreBoard.hpp"

typedef uint64_t paddr_t;
#define SNPID_OFFSET  0x100

namespace CCHIAgent {

    template<typename T>
    class PendingTrans {
    public:
        int beat_cnt;
        int nr_beat;
        std::shared_ptr<T> info;

        PendingTrans() : beat_cnt(0), nr_beat(0) {}
        ~PendingTrans() = default;

        bool is_pending() const { return (beat_cnt > 0); }
        
        // 初始化发送任务
        void init(std::shared_ptr<T> info, int beats) {
            this->info = info;
            this->nr_beat = beats;
            this->beat_cnt = beats;
        }

        // 每发送一个 Beat 调用一次
        void update() {
            if (beat_cnt > 0) beat_cnt--;
        }
    };

    // ID 管理池：防止 TxnID 冲突
    class IDPool {
    private:
        std::set<int> idle_ids;
        std::set<int> used_ids;
    public:
        IDPool(int start, int end) {
            for (int i = start; i < end; i++) {
                idle_ids.insert(i);
            }
        }
        ~IDPool() = default;

        int getid() {
            if (idle_ids.empty()) return -1;
            int ret = *idle_ids.begin();
            idle_ids.erase(ret);
            used_ids.insert(ret);
            return ret;
        }

        void freeid(int id) {
            if (used_ids.count(id)) {
                used_ids.erase(id);
                idle_ids.insert(id);
            }
        }
    };

    class BaseAgent {
    public:
        IDPool idpool;
        uint64_t* cycles;

        BaseAgent(int id_start, int id_end) : idpool(id_start, id_end), cycles(nullptr) {}
        virtual ~BaseAgent() = default;

        void setCycles(uint64_t* c) { cycles = c; }
    };

    // Type 1: Fully Coherent Agent
    class FCAgent : public BaseAgent {
    private:
        // 核心存储：TxnID -> 事务对象 (多态基类指针)
        std::unordered_map<uint16_t, std::shared_ptr<Xact::Xaction>> Transactions;
        
        // 路由表：DBID -> TxnID, SnpID -> TxnID
        std::unordered_map<uint16_t, uint16_t> DBID2TxnID;
        std::unordered_map<uint16_t, uint16_t> SnpID2TxnID;
        
        // 本地记分牌：维护 Cache 状态 (MESI)
        std::unordered_map<paddr_t, localBoardEntry> localBoard;

        // 物理层发送队列 (Flow Control)
        PendingTrans<CCHI::BundleChannelEVT> pendingTXEVT;
        PendingTrans<CCHI::BundleChannelREQ> pendingTXREQ;
        PendingTrans<CCHI::BundleChannelRSP> pendingTXRSP;
        PendingTrans<CCHI::BundleChannelDAT> pendingTXDAT;
        PendingTrans<CCHI::BundleChannelSNP> pendingRXSNP;

        CCHI::FCBundle* port;
        
        // 全局记分牌指针 (用于校验数据真值)
        std::unordered_map<paddr_t, globalBoardEntry>* globalBoardPtr = nullptr;

    public:
        FCAgent(int id_start, int id_end) : BaseAgent(id_start, id_end) {}

        void bindPort(CCHI::FCBundle* p) { port = p; }
        void setGlobalBoard(std::unordered_map<paddr_t, globalBoardEntry>* board) {
            globalBoardPtr = board;
        }
        void offFlight(paddr_t addr, XactType tp) { 
            switch (tp)
            {
                case XactType::EVT:
                    this->localBoard.at(addr).inflight_evt = false;
                    break;
                case XactType::SNP:
                    this->localBoard.at(addr).inflight_snp = false;
                    break;
                case XactType::REQ:
                    this->localBoard.at(addr).inflight_req = false;
                    break;
                default:
                    assert(false);
                    break;
            }
            if (this->localBoard.at(addr).inflight_evt == false &&
                this->localBoard.at(addr).inflight_snp == false &&
                this->localBoard.at(addr).inflight_req == false) {
                this->localBoard.at(addr).inflight = false;
            }
        }

        // 核心循环
        void FIRE(); // 处理所有通道的握手
        void SEND(); // 处理发送队列
        void random_test(); // 产生随机激励

        // 通道处理函数
        void fire_txevt();
        void fire_txreq();
        void fire_rxsnp();
        void fire_txrsp();
        void fire_txdat();
        void fire_rxrsp();
        void fire_rxdat();
        
        // 底层发包函数
        void send_txevt(std::shared_ptr<CCHI::BundleChannelEVT> txevt);
        void send_txreq(std::shared_ptr<CCHI::BundleChannelREQ> txreq);
        void send_txdat(std::shared_ptr<CCHI::BundleChannelDAT> txdat);
        void send_txrsp(std::shared_ptr<CCHI::BundleChannelRSP> txrsp);

        // 事务发起接口
        bool do_REQ(paddr_t addr, uint8_t opcode, uint8_t size, bool expCompStash);
        bool do_EVT(paddr_t addr, uint8_t opcode);

        bool do_ReadNoSnp(paddr_t addr);
        bool do_ReadOnce(paddr_t addr);
        bool do_ReadShared(paddr_t addr);
        bool do_ReadUnique(paddr_t addr);
        bool do_MakeReadUnique(paddr_t addr);

        bool do_Evict(paddr_t addr);
        bool do_WriteBackFull(paddr_t addr);

        bool do_MakeUnique(paddr_t addr);

        bool do_CleanShared(paddr_t addr);
        bool do_CleanInvalid(paddr_t addr);
        bool do_MakeInvalid(paddr_t addr);

        bool do_Stash(paddr_t addr, uint8_t opcode, bool expCompStash);
        bool do_StashShared(paddr_t addr, bool expCompStash);
        bool do_StashUnique(paddr_t addr, bool expCompStash);

        bool do_WriteNoSnp(paddr_t addr, bool full);
        bool do_WriteUnique(paddr_t addr, bool full);

    };
}