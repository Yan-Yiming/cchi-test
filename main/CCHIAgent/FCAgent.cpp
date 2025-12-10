// cchi-test/main/CCHIAgent/FCAgent.cpp
#include "BaseAgent.h"

#define CUR_CYCLE (this->cycles ? *this->cycles : 0)

namespace CCHIAgent {

    void FCAgent::FIRE() {
        fire_txevt();
        fire_txreq();
        fire_rxsnp();
        fire_txrsp();
        fire_txdat();
        fire_rxrsp();
        fire_rxdat();
    }

    void FCAgent::SEND() {
        // 轮询发送队列，如果有待发送的 Beat，尝试发送
        if (pendingTXEVT.is_pending()) send_txevt(pendingTXEVT.info);
        if (pendingTXREQ.is_pending()) send_txreq(pendingTXREQ.info);
        if (pendingTXDAT.is_pending()) {
            auto& info = pendingTXDAT.info;
            int current_beat = pendingTXDAT.nr_beat - pendingTXDAT.beat_cnt;
            
            if (DBID2TxnID.count(info->txnID)) {
                uint16_t tid = DBID2TxnID[info->txnID];
                if (Transactions.count(tid)) {
                    Transactions[tid]->getBeatData(current_beat, info->data);
                }
            } 
            // snoop
            else if (SnpID2TxnID.count(info->txnID)) {
                auto originTxnID = SnpID2TxnID[info->txnID];
                auto& xact = Transactions[originTxnID];
                xact->getBeatData(current_beat, info->data);
            }
            else assert(false);
            send_txdat(info);
        }
        if (pendingTXRSP.is_pending()) send_txrsp(pendingTXRSP.info);
    }

    void FCAgent::fire_txevt() {
        if (this->port->txevt.fire()) {
            auto& chn = this->port->txevt;
            chn.valid = false; 

            assert(pendingTXEVT.is_pending());
            pendingTXEVT.update(); 

            auto it = Transactions.find(chn.txnID);
            if (it != Transactions.end()) {
                it->second->handleTXEVT(chn, CUR_CYCLE);
            }
        }
    }

    void FCAgent::fire_txreq() {
        if (this->port->txreq.fire()) {
            auto& chn = this->port->txreq;
            chn.valid = false;

            assert(pendingTXREQ.is_pending());
            pendingTXREQ.update();

            auto it = Transactions.find(chn.txnID);
            if (it != Transactions.end()) {
                it->second->handleTXREQ(chn, CUR_CYCLE);
            }
        }
    }

    void FCAgent::fire_rxrsp() {
        if (this->port->rxrsp.fire()) {
            auto& chnRSP = this->port->rxrsp;

            auto it = Transactions.find(chnRSP.txnID);
            if (it != Transactions.end()) {
                auto& transaction = it->second;
                transaction->handleRXRSP(chnRSP, CUR_CYCLE);

                if ((CCHIOpcodeRSP_UP)chnRSP.opcode == CCHIOpcodeRSP_UP::CompDBIDResp) {
                    this->DBID2TxnID[chnRSP.dbID] = chnRSP.txnID;

                    auto txdat = std::make_shared<CCHI::BundleChannelDAT>();

                    txdat->txnID  = chnRSP.dbID;
                    auto op = transaction->getOpcode();
                    switch (op) {
                        case (uint8_t)CCHIOpcodeEVT::WriteBackFull:
                            txdat->opcode = (uint8_t)CCHIOpcodeDAT_DOWN::CopyBackWrData;
                            break;
                        case (uint8_t)CCHIOpcodeREQ::WriteNoSnpFull:
                        case (uint8_t)CCHIOpcodeREQ::WriteNoSnpPtl:
                        case (uint8_t)CCHIOpcodeREQ::WriteUniqueFull:
                        case (uint8_t)CCHIOpcodeREQ::WriteUniquePtl:
                            txdat->opcode = (uint8_t)CCHIOpcodeDAT_DOWN::NonCopyBackWrData;
                            break;
                        default:
                            assert(false);
                            break;
                    }

                    // TODO: 这里应该从 localboard? 中获取真实数据
                    // 现在只有一拍数据，暂时随机生成，为了跑通流程
                    for (int i = 0; i < 32; ++i) txdat->data[i] = (uint8_t)rand();
                    
                    pendingTXDAT.init(txdat, 2);
                }
                else if ((CCHIOpcodeRSP_UP)chnRSP.opcode == CCHIOpcodeRSP_UP::Comp) {
                    if (transaction->getOpcode() == (uint8_t)CCHIOpcodeREQ::MakeUnique)  {
                        this->DBID2TxnID[chnRSP.dbID] = chnRSP.txnID;
                        auto txrsp = std::make_shared<CCHI::BundleChannelRSP>();
                        txrsp->txnID  = chnRSP.dbID; // 使用 DBID 作为 TxnID
                        txrsp->opcode = (uint8_t)CCHIOpcodeRSP_DOWN::CompAck;
                    }
                    else if (transaction->getOpcode() == (uint8_t)CCHIOpcodeEVT::Evict) { }
                    else assert(false);
                }
                else if ((CCHIOpcodeRSP_UP)chnRSP.opcode == CCHIOpcodeRSP_UP::CompCMO) { }
                else if ((CCHIOpcodeRSP_UP)chnRSP.opcode == CCHIOpcodeRSP_UP::CompStash) { }
                else assert(false);
                
                if (transaction->isComplete()) {
                    // 更新本地记分牌状态
                    paddr_t addr = it->second->getAddr();
                    localBoard.at(addr).state = transaction->finalState;
                    offFlight(addr, transaction->getXactType());
                    
                    // 释放 ID 和事务对象
                    idpool.freeid(it->second->getTxnID());
                    Transactions.erase(it);
                }
            }
        }
    }

    void FCAgent::fire_rxdat() {
        if (this->port->rxdat.fire()) {
            auto& chnDAT = this->port->rxdat;

            auto it = Transactions.find(chnDAT.txnID);
            if (it != Transactions.end()) {
                auto& transaction = it->second;
                transaction->handleRXDAT(chnDAT, CUR_CYCLE);

                if (this->DBID2TxnID.count(chnDAT.dbID) == 0) {
                    // 建立路由映射：后续发数据用 DBID，需要能找回 TxnID
                    this->DBID2TxnID[chnDAT.dbID] = chnDAT.txnID;
                }

                if (transaction->DataDone()) {
                    if (globalBoardPtr) {
                        const uint8_t* actual_data = transaction->getData();
                        paddr_t addr = transaction->getAddr();
                        if (globalBoardPtr->find(addr) == globalBoardPtr->end()) {
                            (*globalBoardPtr)[addr].verify(actual_data);
                        }
                    }

                    // 不会阻塞才可行
                    auto txrsp = std::make_shared<CCHI::BundleChannelRSP>();
                    txrsp->txnID  = chnDAT.dbID; // 使用 Home 返回的 DBID
                    txrsp->opcode = (uint8_t)CCHIOpcodeRSP_DOWN::CompAck;
                    pendingTXRSP.init(txrsp, 1);

                }
                if (transaction->isComplete()) {
                    paddr_t addr = transaction->getAddr();
                    localBoard.at(addr).state = transaction->finalState;
                    offFlight(addr, transaction->getXactType());
                    
                    idpool.freeid(transaction->getTxnID());
                    Transactions.erase(it);
                }
            }
        }
    }

    void FCAgent::fire_txdat() {
        if (this->port->txdat.fire()) {
            auto& chn = this->port->txdat;
            chn.valid = false;
            
            assert(pendingTXDAT.is_pending());
            pendingTXDAT.update();

            // 通过 DBID 找回原始事务 ID
            if (DBID2TxnID.count(chn.txnID)) {
                uint16_t origTxnID = DBID2TxnID[chn.txnID];
                auto it = Transactions.find(origTxnID);
                
                if (it != Transactions.end()) {
                    auto& transaction = it->second;
                    transaction->handleTXDAT(chn, CUR_CYCLE);
                    
                    if (transaction->DataDone()) { 
                        if (globalBoardPtr) {
                            const uint8_t* written_data = transaction->getData();
                            if (written_data) {
                                (*globalBoardPtr)[transaction->getAddr()].update(written_data);
                            }
                        }
                    }
                    if (transaction->isComplete()) {
                        paddr_t addr = transaction->getAddr();
                        localBoard.at(addr).state = transaction->finalState;
                        offFlight(addr, transaction->getXactType());
                        
                        idpool.freeid(origTxnID);
                        Transactions.erase(it);
                        DBID2TxnID.erase(chn.txnID); // 清理路由表
                    }
                }
            }
        }
    }

    void FCAgent::fire_txrsp() {
        if (this->port->txrsp.fire()) {
            auto& chnRSP = this->port->txrsp;
            chnRSP.valid = false;
            
            assert(pendingTXRSP.is_pending());
            pendingTXRSP.update();

            if (DBID2TxnID.count(chnRSP.txnID)) {
                uint16_t origTxnID = DBID2TxnID[chnRSP.txnID];
                auto it = Transactions.find(origTxnID);
                
                if (it != Transactions.end()) {
                    auto& transaction = it->second;
                    transaction->handleTXRSP(chnRSP, CUR_CYCLE);

                    if (transaction->isComplete()) {
                        paddr_t addr = transaction->getAddr();
                        localBoard.at(addr).state = transaction->finalState;
                        offFlight(addr, transaction->getXactType());
                        
                        idpool.freeid(origTxnID);
                        Transactions.erase(it);
                        DBID2TxnID.erase(chnRSP.txnID); // 清理路由表
                    }
                }
                else assert(false);
            }
        }
    }

    void FCAgent::fire_rxsnp() {
        bool resource_available = !pendingTXRSP.is_pending() && !pendingTXDAT.is_pending();
        this->port->rxsnp.ready = resource_available;
        if (!this->port->rxsnp.fire()) return;

        auto& chnSNP = this->port->rxsnp; 

        paddr_t addr = chnSNP.addr;
        uint16_t snpTxnID = chnSNP.txnID;
        uint8_t opcode = chnSNP.opcode;

        CCHI::CacheState curState = CCHI::CacheState::INV;
        bool isDirty = false;
        
        if (localBoard.count(addr)) {
            curState = localBoard.at(addr).state;
            isDirty = (curState == CCHI::CacheState::UD);
        }

        CCHI::CacheState newState = CCHI::CacheState::INV;
        bool sendData = false;

        switch ((CCHIOpcodeSNP)opcode) {
            case CCHIOpcodeSNP::SnpMakeInvalid:
                newState = CCHI::CacheState::INV;
                sendData = false; 
                break;
            case CCHIOpcodeSNP::SnpToInvalid:
                newState = CCHI::CacheState::INV;
                sendData = isDirty; // Dirty 必须回写
                break;
            case CCHIOpcodeSNP::SnpToShared:
                newState = (curState == CCHI::CacheState::INV) ? CCHI::CacheState::INV : CCHI::CacheState::SC;
                sendData = isDirty; // Dirty 变 Shared，同时也需要把脏数据吐出来给 Home/Req
                break;
            default: // SnpToClean 等
                newState = CCHI::CacheState::INV;
                sendData = isDirty;
                break;
        }

        auto snpXact = std::make_shared<Xact::SnoopOperation>(chnSNP, CUR_CYCLE, sendData);
        snpXact->finalState = newState;
        
        snpXact->handleRXSNP(chnSNP, CUR_CYCLE); 

        // 注意：TxnID 可能会冲突，工业级 VIP 会用独立 Map，这里简化复用 Transactions
        // 或者我们可以给 Snoop ID 加一个 offset 区分
        auto txnID = idpool.getid();
        this->SnpID2TxnID[snpTxnID] = txnID;
        this->Transactions.emplace(txnID, snpXact); 

        if (sendData) {
            auto txdat = std::make_shared<CCHI::BundleChannelDAT>();
            txdat->txnID = snpTxnID; 
            txdat->opcode = (uint8_t)CCHIOpcodeDAT_DOWN::SnpRespData;
            pendingTXDAT.init(txdat, 2); 
        } else {
            auto txrsp = std::make_shared<CCHI::BundleChannelRSP>();
            txrsp->txnID = snpTxnID;
            txrsp->opcode = (uint8_t)CCHIOpcodeRSP_DOWN::SnpResp;
            pendingTXRSP.init(txrsp, 1);
        }
    }

    void FCAgent::send_txevt(std::shared_ptr<CCHI::BundleChannelEVT> txevt) {
        this->port->txevt.valid  = true;
        this->port->txevt.opcode = txevt->opcode;
        this->port->txevt.txnID  = txevt->txnID;
        this->port->txevt.addr   = txevt->addr;
        // ... 其他字段 ...
    }

    void FCAgent::send_txreq(std::shared_ptr<CCHI::BundleChannelREQ> txreq) {
        this->port->txreq.valid  = true;
        this->port->txreq.opcode = txreq->opcode;
        this->port->txreq.txnID  = txreq->txnID;
        this->port->txreq.addr   = txreq->addr;
        this->port->txreq.size   = txreq->size;
    }

    void FCAgent::send_txdat(std::shared_ptr<CCHI::BundleChannelDAT> txdat) {
        this->port->txdat.valid  = true;
        this->port->txdat.opcode = txdat->opcode;
        this->port->txdat.txnID  = txdat->txnID;
        
        // 正确拷贝 32 字节数据
        std::memcpy(this->port->txdat.data, txdat->data, 32);
    }

    void FCAgent::send_txrsp(std::shared_ptr<CCHI::BundleChannelRSP> txrsp) {
        this->port->txrsp.valid  = true;
        this->port->txrsp.opcode = txrsp->opcode;
        this->port->txrsp.txnID  = txrsp->txnID;
    }

    bool FCAgent::do_EVT(paddr_t addr, uint8_t opcode) {
        if (pendingTXEVT.is_pending()) return false;
        if (localBoard.count(addr) != 0 && localBoard.at(addr).inflight_evt) return false;

        // TODO: 
        auto txevt = std::make_shared<CCHI::BundleChannelEVT>();
        txevt->txnID  = this->idpool.getid();
        txevt->opcode = opcode;
        txevt->addr   = addr;

        std::shared_ptr<Xact::Xaction> xact;
        if (opcode == (uint8_t)CCHIOpcodeEVT::WriteBackFull) {
            xact = std::make_shared<Xact::WriteBackFull>(*txevt, CUR_CYCLE);
        }
        else if (opcode == (uint8_t)CCHIOpcodeEVT::Evict) {
            xact = std::make_shared<Xact::Evict>(*txevt, CUR_CYCLE);
        }
        else assert(false);
    
        this->Transactions.emplace(txevt->txnID, xact);

        if (localBoard.count(addr) == 0) {
            localBoard.emplace(addr, localBoardEntry(CCHI::CacheState::INV, 0));
        }
        auto& entry = localBoard.at(addr);
        entry.inflight = true;
        entry.inflight_evt = true;

        pendingTXEVT.init(txevt, 1);
        return true;
    }

    bool FCAgent::do_REQ(paddr_t addr, uint8_t opcode, uint8_t size, bool expCompStash) {
        if (pendingTXREQ.is_pending()) return false;
        if (localBoard.count(addr) != 0 && localBoard.at(addr).inflight) return false;

        auto txreq = std::make_shared<CCHI::BundleChannelREQ>();
        txreq->txnID        = this->idpool.getid();
        txreq->opcode       = opcode;
        txreq->size         = size;
        txreq->addr         = addr;
        txreq->ns           = 0;        // TODO
        txreq->order        = 0;        // TODO
        txreq->memAttr      = 0;        // TODO
        txreq->excl         = 0;        // TODO
        txreq->expCompStash = expCompStash;
        txreq->wayValid     = 0;        // TODO
        txreq->way          = 0;        // TODO
        txreq->traceTag     = 0;        // TODO
        txreq->allowRetry   = 0;        // TODO

        if (localBoard.count(addr) == 0) {
            localBoard.emplace(addr, localBoardEntry(CCHI::CacheState::INV, 0));
        }
        localBoard.at(addr).inflight = true;
        localBoard.at(addr).inflight_req = true;
        CCHI::CacheState current_state = localBoard.at(addr).state;

        std::shared_ptr<Xact::Xaction> xact;
        switch ((CCHIOpcodeREQ)opcode) {
            case CCHIOpcodeREQ::StashShared:
            case CCHIOpcodeREQ::StashUnique:
                xact = std::make_shared<Xact::Stash>(*txreq, CUR_CYCLE, current_state);
                break;
            case CCHIOpcodeREQ::ReadNoSnp:
                xact = std::make_shared<Xact::ReadNoSnp>(*txreq, CUR_CYCLE);
                break;
            case CCHIOpcodeREQ::ReadOnce:
                xact = std::make_shared<Xact::ReadOnce>(*txreq, CUR_CYCLE);
                break;
            case CCHIOpcodeREQ::ReadShared:
            case CCHIOpcodeREQ::ReadUnique:
            case CCHIOpcodeREQ::MakeReadUnique:
                xact = std::make_shared<Xact::ReadAllocateCacheable>(*txreq, CUR_CYCLE);
                break;
            case CCHIOpcodeREQ::MakeUnique:
                xact = std::make_shared<Xact::MakeUnique>(*txreq, CUR_CYCLE);
                break;
            case CCHIOpcodeREQ::CleanShared:
            case CCHIOpcodeREQ::CleanInvalid:
            case CCHIOpcodeREQ::MakeInvalid:
                xact = std::make_shared<Xact::CacheManagementOperation>(*txreq, CUR_CYCLE, current_state);
                break;
            case CCHIOpcodeREQ::WriteNoSnpFull:
            case CCHIOpcodeREQ::WriteNoSnpPtl:
                xact = std::make_shared<Xact::WriteNoSnp>(*txreq, CUR_CYCLE);
                break;
            case CCHIOpcodeREQ::WriteUniqueFull:
            case CCHIOpcodeREQ::WriteUniquePtl:
                xact = std::make_shared<Xact::WriteUnique>(*txreq, CUR_CYCLE);
                break;
            default:
                assert(false);
                break;
        }
        this->Transactions.emplace(txreq->txnID, xact);

        pendingTXREQ.init(txreq, 1);
        return true;
    }

/*
    FOR Requester
    =====================================================
    Require/CacheState  |   I   |   UC  |   UD  |   SC  |
    ReadOnce            |   Y   |       |       |       |
    ReadShared          |   Y   |   Y   |   Y   |   Y   |
    ReadUnique          |   Y   |   Y   |   Y   |   Y   |
    MakeReadUnique      |       |       |       |   Y   |
    =====================================================
*/
    bool FCAgent::do_ReadNoSnp(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::ReadNoSnp, 0, 0);
    }
    
    bool FCAgent::do_ReadOnce(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::ReadOnce, 0, 0);
    }
    
    bool FCAgent::do_ReadShared(paddr_t addr) {
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::ReadShared, 0, 0);
    }

    bool FCAgent::do_ReadUnique(paddr_t addr) {
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::ReadUnique, 0, 0);
    }

    bool FCAgent::do_MakeReadUnique(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::SC);
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::MakeReadUnique, 0, 0);
    }

    bool FCAgent::do_Evict(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_EVT(addr, (uint8_t)CCHIOpcodeEVT::Evict);
    }

    bool FCAgent::do_WriteBackFull(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_EVT(addr, (uint8_t)CCHIOpcodeEVT::WriteBackFull);
    }

    bool FCAgent::do_MakeUnique(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state != CCHI::CacheState::UD);
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::MakeUnique, 0, 0);
    }

    bool FCAgent::do_CleanShared(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::CleanShared, 0, 0);
    }

    bool FCAgent::do_CleanInvalid(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::CleanInvalid, 0, 0);
    }

    bool FCAgent::do_MakeInvalid(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::MakeInvalid, 0, 0);
    }

    bool FCAgent::do_StashShared(paddr_t addr, bool expCompStash) {
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::StashShared, 0, expCompStash);
    }

    bool FCAgent::do_StashUnique(paddr_t addr, bool expCompStash) {
        return do_REQ(addr, (uint8_t)CCHIOpcodeREQ::StashUnique, 0, expCompStash);
    }

    bool FCAgent::do_WriteNoSnp(paddr_t addr, bool full) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        uint8_t opcode = (uint8_t)(full ? CCHIOpcodeREQ::WriteNoSnpFull : CCHIOpcodeREQ::WriteNoSnpPtl);
        return do_REQ(addr, opcode, 0, 0);
    }

    bool FCAgent::do_WriteUnique(paddr_t addr, bool full) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        uint8_t opcode = (uint8_t)(full ? CCHIOpcodeREQ::WriteUniqueFull : CCHIOpcodeREQ::WriteUniquePtl);
        return do_REQ(addr, opcode, 0, 0);
    }

    void FCAgent::random_test() {
        if (rand() % 100 < 5) { // 5% 概率
            do_ReadUnique(0x8000 + (rand() % 16) * 64); // 随机地址
        }
    }

} // namespace CCHIAgent