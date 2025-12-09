#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>
#include <iostream>
#include <cstring>
#include <algorithm> // for std::copy
#include "cchi_base.hpp"

enum class XactType {
    INV = 0,
    EVT = 1,
    SNP = 2,
    REQ = 3
};

namespace Xact {
    /*
        LIST

        ReadNoSnp
    */

    // 事务状态：更清晰的生命周期定义
    enum class Status {
        Ongoing,    // 进行中
        Completed,  // 已完成 (成功)
        Failed      // 失败/错误
    };

    // 历史记录条目：记录“发生了什么”以及“什么时候发生的”
    template <typename T>
    struct HistoryEntry {
        uint64_t cycle;
        T        packet;
    };

    // --- 基类：Xaction (富基类) ---
    class Xaction {
    protected:
        XactType    xactType;
        uint16_t    txnID;
        uint64_t    addr;
        uint8_t     opcode;
        uint64_t    startCycle;
        Status      status;

        // 全生命周期历史记录 (Log)
        // 使用 vector 存储，方便 Dump 和后续分析
        std::vector<HistoryEntry<CCHI::BundleChannelREQ>> req_history;
        std::vector<HistoryEntry<CCHI::BundleChannelEVT>> evt_history;
        std::vector<HistoryEntry<CCHI::BundleChannelSNP>> snp_history;
        std::vector<HistoryEntry<CCHI::BundleChannelRSP>> rxrsp_history; // 收到的 RSP
        std::vector<HistoryEntry<CCHI::BundleChannelRSP>> txrsp_history; // 发出的 RSP
        std::vector<HistoryEntry<CCHI::BundleChannelDAT>> txdat_history; // 发出的 DAT
        std::vector<HistoryEntry<CCHI::BundleChannelDAT>> rxdat_history; // 收到的 DAT

    public:
        CCHI::CacheState finalState = CCHI::CacheState::INV; // 默认状态

        // 构造函数
        Xaction(XactType xt, uint16_t tid, uint64_t address, uint8_t op, uint64_t cycle)
            : xactType(xt), txnID(tid), addr(address), opcode(op), startCycle(cycle), status(Status::Ongoing) {}

        virtual ~Xaction() = default;

        XactType    getXactType()   const { return xactType; }
        uint16_t    getTxnID()      const { return txnID; }
        uint64_t    getAddr()       const { return addr; }
        uint8_t     getOpcode()     const { return opcode; }
        Status      getStatus()     const { return status; }
        bool        isComplete()    const { return status == Status::Completed; }

        // --- 事件处理接口 (Template Method) ---
        // Agent 调用这些 handle 方法，基类负责记录日志，然后分发给子类处理逻辑
        void handleTXREQ(const CCHI::BundleChannelREQ& pkt, uint64_t cycle) {
            req_history.push_back({cycle, pkt});
            onTXREQ(pkt);
        }

        void handleTXEVT(const CCHI::BundleChannelEVT& pkt, uint64_t cycle) {
            evt_history.push_back({cycle, pkt});
            onTXEVT(pkt);
        }

        void handleTXRSP(const CCHI::BundleChannelRSP& pkt, uint64_t cycle) {
            txrsp_history.push_back({cycle, pkt});
            onTXRSP(pkt);
        }

        void handleRXRSP(const CCHI::BundleChannelRSP& pkt, uint64_t cycle) {
            rxrsp_history.push_back({cycle, pkt});
            onRXRSP(pkt);
        }

        void handleTXDAT(const CCHI::BundleChannelDAT& pkt, uint64_t cycle) {
            txdat_history.push_back({cycle, pkt});
            onTXDAT(pkt);
        }

        void handleRXDAT(const CCHI::BundleChannelDAT& pkt, uint64_t cycle) {
            rxdat_history.push_back({cycle, pkt});
            onRXDAT(pkt);
        }

        virtual const uint8_t* getData() const { return nullptr; }
        virtual const bool DataDone() const { return false; }

    protected:
        // --- 子类钩子 (Hooks) ---
        virtual void onTXREQ(const CCHI::BundleChannelREQ& pkt) {}
        virtual void onTXEVT(const CCHI::BundleChannelEVT& pkt) {}
        virtual void onTXRSP(const CCHI::BundleChannelRSP& pkt) {}
        virtual void onRXRSP(const CCHI::BundleChannelRSP& pkt) {}
        virtual void onTXDAT(const CCHI::BundleChannelDAT& pkt) {}
        virtual void onRXDAT(const CCHI::BundleChannelDAT& pkt) {}

        void markComplete() { status = Status::Completed; }
    };

    class ReadNoSnp : public Xaction {
    private:
        uint8_t dataBuffer[64]; // 内部数据缓冲区
        int     beatsReceived;
        int     expectedBeats;
        bool    gotCompData;

    public:
        ReadNoSnp(const CCHI::BundleChannelREQ& req, uint64_t cycle)
            : Xaction(XactType::REQ, req.txnID, req.addr, req.opcode, cycle),
              beatsReceived(0), gotCompData(false)
        {
            std::memset(dataBuffer, 0, 64);
            expectedBeats = 2; 

            finalState = CCHI::CacheState::INV; // 读独占，预期状态 UC
        }

        // 处理收到的数据
        void onRXDAT(const CCHI::BundleChannelDAT& pkt) override {
            int offset = beatsReceived * 32;
            assert(offset < 64);
            std::memcpy(dataBuffer + offset, pkt.data, 32);

            beatsReceived++;
            if (beatsReceived >= expectedBeats) {
                gotCompData = true;
            }
        }

        const uint8_t* getData() const override {
            return dataBuffer;
        }

        const bool DataDone() const override {
            return gotCompData;
        }
    };

    class ReadOnce : public Xaction {
    private:
        uint8_t dataBuffer[64]; // 内部数据缓冲区
        int     beatsReceived;
        int     expectedBeats;
        bool    gotCompData;

    public:
            ReadOnce(const CCHI::BundleChannelREQ& req, uint64_t cycle)
            : Xaction(XactType::REQ, req.txnID, req.addr, req.opcode, cycle),
              beatsReceived(0), gotCompData(false)
        {
            std::memset(dataBuffer, 0, 64);
            expectedBeats = 2; 

            finalState = CCHI::CacheState::INV; // 读独占，预期状态 UC
        }

        // 处理收到的数据
        void onRXDAT(const CCHI::BundleChannelDAT& pkt) override {
            int offset = beatsReceived * 32;
            assert(offset < 64);
            std::memcpy(dataBuffer + offset, pkt.data, 32);

            beatsReceived++;
            if (beatsReceived >= expectedBeats) {
                gotCompData = true;
            }
        }

        const uint8_t* getData() const override {
            return dataBuffer;
        }

        const bool DataDone() const override {
            return gotCompData;
        }
    };

    class ReadAllocateCacheable : public Xaction {
    private:
        uint8_t dataBuffer[64]; // 内部数据缓冲区
        int     beatsReceived;
        int     expectedBeats;
        bool    gotCompData;
        bool    sentCompAck;

    public:
        ReadAllocateCacheable(const CCHI::BundleChannelREQ& req, uint64_t cycle)
            : Xaction(XactType::REQ, req.txnID, req.addr, req.opcode, cycle),
              beatsReceived(0), gotCompData(false), sentCompAck(false)
        {
            std::memset(dataBuffer, 0, 64);
            expectedBeats = 2; 
            
            // handleTXREQ(req, cycle);
            if ((CCHIOpcodeREQ)req.opcode == CCHIOpcodeREQ::ReadUnique)
                finalState = CCHI::CacheState::UC; // 读独占，预期状态 UC
            else if ((CCHIOpcodeREQ)req.opcode == CCHIOpcodeREQ::ReadShared)
                // TODO: UC/UD/SC how to decide
                finalState = CCHI::CacheState::UC;
            else if ((CCHIOpcodeREQ)req.opcode == CCHIOpcodeREQ::MakeReadUnique)
                finalState = CCHI::CacheState::UC;
            else 
                assert(false);
        }

        // 处理收到的数据
        void onRXDAT(const CCHI::BundleChannelDAT& pkt) override {
            int offset = beatsReceived * 32;
            assert(offset < 64);
            std::memcpy(dataBuffer + offset, pkt.data, 32);

            beatsReceived++;
            if (beatsReceived >= expectedBeats) {
                gotCompData = true;
            }
        }

        void onTXRSP(const CCHI::BundleChannelRSP& pkt) override {
            assert((CCHIOpcodeRSP_DOWN)pkt.opcode == CCHIOpcodeRSP_DOWN::CompAck);
            sentCompAck = true;
            markComplete();
        }

        const uint8_t* getData() const override {
            return dataBuffer;
        }

        const bool DataDone() const override {
            return gotCompData;
        }
    };

    class Evict : public Xaction {
    private:
        bool gotComp;
    
    public:
        Evict(const CCHI::BundleChannelEVT& evt, uint64_t cycle)
            : Xaction(XactType::EVT, evt.txnID, evt.addr, evt.opcode, cycle),
              gotComp(false)
        {
            finalState = CCHI::CacheState::INV;
        }

        void onRXRSP(const CCHI::BundleChannelRSP& pkt) override {
            assert((CCHIOpcodeRSP_UP)pkt.opcode == CCHIOpcodeRSP_UP::Comp);
            gotComp = true;
        }
    };

    class WriteBackFull : public Xaction {
    private:
        bool gotDBIDResp;
        bool sentCompData;
        int  beatsSent;

    public:
        WriteBackFull(const CCHI::BundleChannelEVT& evt, uint64_t cycle)
            : Xaction(XactType::EVT, evt.txnID, evt.addr, evt.opcode, cycle),
              gotDBIDResp(false), sentCompData(false), beatsSent(0)
        {
            // handleTXEVT(evt, cycle);
            finalState = CCHI::CacheState::INV;
        }

        void onRXRSP(const CCHI::BundleChannelRSP& pkt) override {
            assert((CCHIOpcodeRSP_UP)pkt.opcode == CCHIOpcodeRSP_UP::CompDBIDResp);
            gotDBIDResp = true;
        }

        void onTXDAT(const CCHI::BundleChannelDAT& pkt) override {
            assert((CCHIOpcodeDAT_UP)pkt.opcode == CCHIOpcodeDAT_UP::CompData);
            beatsSent++;
            if (beatsSent >= 2) {
                sentCompData = true;
                markComplete();
            }
        }

        const bool DataDone() const override {
            return sentCompData;
        }
    };

    class MakeUnique : public Xaction {
    private:
        bool gotComp;
        bool sentCompAck;

    public:
        MakeUnique(const CCHI::BundleChannelREQ& req, uint64_t cycle)
            : Xaction(XactType::REQ, req.txnID, req.addr, req.opcode, cycle),
            gotComp(false), sentCompAck(false)
        {
            // handleTXEVT(evt, cycle);
            finalState = CCHI::CacheState::UD;
        }

        void onRXRSP(const CCHI::BundleChannelRSP& pkt) override {
            assert((CCHIOpcodeRSP_UP)pkt.opcode == CCHIOpcodeRSP_UP::Comp);
            gotComp = true;
        }

        void onTXRSP(const CCHI::BundleChannelRSP& pkt) override {
            assert((CCHIOpcodeRSP_DOWN)pkt.opcode == CCHIOpcodeRSP_DOWN::CompAck);
            sentCompAck = true;
            markComplete();
        }
    };

    class CacheManagementOperation : public Xaction {
    private:
        bool gotCompCMO;

    public:
        CacheManagementOperation(const CCHI::BundleChannelREQ& req, uint64_t cycle, CCHI::CacheState state)
            : Xaction(XactType::REQ, req.txnID, req.addr, req.opcode, cycle),
            gotCompCMO(false)
        {
            finalState = state;
        }

        void onRXRSP(const CCHI::BundleChannelRSP& pkt) override {
            assert((CCHIOpcodeRSP_UP)pkt.opcode == CCHIOpcodeRSP_UP::CompCMO);
            gotCompCMO = true;
            markComplete();
        }
    };
}