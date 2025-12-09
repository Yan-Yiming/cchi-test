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
        if (pendingTXDAT.is_pending()) send_txdat(pendingTXDAT.info);
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

                // 3. 特殊逻辑：WriteBack 收到 DBIDResp 后需要发送数据
                // 这里我们检查 Opcode，如果是 DBIDResp，则触发数据发送
                if ((CCHIOpcodeRSP_UP)chnRSP.opcode == CCHIOpcodeRSP_UP::CompDBIDResp) {
                    
                    // 建立路由映射：后续发数据用 DBID，需要能找回 TxnID
                    this->DBID2TxnID[chnRSP.dbID] = chnRSP.txnID;

                    // 构造数据包 (CopyBackWrData)
                    auto txdat = std::make_shared<CCHI::BundleChannelDAT>();
                    txdat->txnID  = chnRSP.dbID; // 使用 DBID 作为 TxnID
                    if (transaction->getOpcode() == (uint8_t)CCHIOpcodeEVT::WriteBackFull) {
                        txdat->opcode = (uint8_t)CCHIOpcodeDAT_DOWN::CopyBackWrData;
                    }
                    else assert(false);
                    
                    // TODO: 这里应该从 XactionWriteBackFull 中获取真实数据
                    // 目前暂时随机生成，为了跑通流程
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
                    // [校验] 从 GlobalBoard 验证数据
                    if (globalBoardPtr) {
                        const uint8_t* actual_data = transaction->getData();
                        if (actual_data) {
                            paddr_t addr = transaction->getAddr();
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
                    
                    // 如果数据发完了，且事务逻辑认为可以结束 (WriteBack 流程)
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
        // Snoop 处理逻辑暂时留空，通常需要查找 localBoard 并生成 SnpResp
        if (this->port->rxsnp.fire()) {
            // TODO: Implement Snoop logic
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

    bool FCAgent::do_ReadNoSnp(paddr_t addr) {
        if (pendingTXREQ.is_pending()) return false;
        if (localBoard.count(addr) != 0 && localBoard.at(addr).inflight) return false;

        auto txreq = std::make_shared<CCHI::BundleChannelREQ>();
        txreq->txnID  = this->idpool.getid();
        txreq->opcode = (uint8_t)CCHIOpcodeREQ::ReadNoSnp;
        txreq->addr   = addr;
        txreq->size   = 6; // 假设 64B

        auto xact = std::make_shared<Xact::ReadNoSnp>(*txreq, CUR_CYCLE);
        this->Transactions.emplace(txreq->txnID, xact);

        if (localBoard.count(addr) == 0) {
            localBoard.emplace(addr, localBoardEntry(CCHI::CacheState::INV, 0));
        }
        localBoard.at(addr).inflight = true;
        localBoard.at(addr).inflight_req = true;

        pendingTXREQ.init(txreq, 1);
        return true;
    }
    
    bool FCAgent::do_ReadOnce(paddr_t addr) {
        if (pendingTXREQ.is_pending()) return false;
        if (localBoard.count(addr) != 0 && localBoard.at(addr).inflight) return false;

        // 1. 构造 REQ 包
        auto txreq = std::make_shared<CCHI::BundleChannelREQ>();
        txreq->txnID  = this->idpool.getid();
        txreq->opcode = (uint8_t)CCHIOpcodeREQ::ReadOnce;
        txreq->addr   = addr;
        txreq->size   = 6; // 假设 64B

        // 2. 创建并注册事务对象
        auto xact = std::make_shared<Xact::ReadOnce>(*txreq, CUR_CYCLE);
        this->Transactions.emplace(txreq->txnID, xact);

        // 3. 更新本地记分牌
        if (localBoard.count(addr) == 0) {
            localBoard.emplace(addr, localBoardEntry(CCHI::CacheState::INV, 0));
        }
        assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        localBoard.at(addr).inflight = true;
        localBoard.at(addr).inflight_req = true;

        // 4. 初始化发送
        pendingTXREQ.init(txreq, 1);
        return true;
    }
    
    bool FCAgent::do_ReadShared(paddr_t addr) {
        if (localBoard.count(addr)) {
            // assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        }
        return do_ReadAllocateCacheable(addr, (uint8_t)CCHIOpcodeREQ::ReadShared);
    }

    bool FCAgent::do_ReadUnique(paddr_t addr) {
        if (localBoard.count(addr)) {
            // assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        }
        return do_ReadAllocateCacheable(addr, (uint8_t)CCHIOpcodeREQ::ReadUnique);
    }

    bool FCAgent::do_ReadAllocateCacheable(paddr_t addr, uint8_t opcode) {
        if (pendingTXREQ.is_pending()) return false;
        if (localBoard.count(addr) != 0 && localBoard.at(addr).inflight) return false;

        // 1. 构造 REQ 包
        auto txreq = std::make_shared<CCHI::BundleChannelREQ>();
        txreq->txnID  = this->idpool.getid();
        txreq->opcode = opcode;
        txreq->addr   = addr;
        txreq->size   = 6; // 假设 64B

        // 2. 创建并注册事务对象
        auto xact = std::make_shared<Xact::ReadAllocateCacheable>(*txreq, CUR_CYCLE);
        this->Transactions.emplace(txreq->txnID, xact);

        // 3. 更新本地记分牌
        if (localBoard.count(addr) == 0) {
            localBoard.emplace(addr, localBoardEntry(CCHI::CacheState::INV, 0));
        }
        localBoard.at(addr).inflight = true;
        localBoard.at(addr).inflight_req = true;

        // 4. 初始化发送
        pendingTXREQ.init(txreq, 1);
        return true;
    }

    bool FCAgent::do_Evict(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_EVT(addr, (uint8_t)CCHIOpcodeEVT::Evict);
    }

    bool FCAgent::do_WriteBackFull(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_EVT(addr, (uint8_t)CCHIOpcodeEVT::WriteBackFull);
    }

    bool FCAgent::do_EVT(paddr_t addr, uint8_t opcode) {
        if (pendingTXEVT.is_pending()) return false;
        if (localBoard.count(addr) != 0 && localBoard.at(addr).inflight_evt) return false;

        // 1. 构造 EVT 包
        auto txevt = std::make_shared<CCHI::BundleChannelEVT>();
        txevt->txnID  = this->idpool.getid();
        txevt->opcode = opcode;
        txevt->addr   = addr;

        // 2. [优雅] 创建并注册事务对象
        std::shared_ptr<Xact::Xaction> xact;
        if (opcode == (uint8_t)CCHIOpcodeEVT::WriteBackFull) {
            xact = std::make_shared<Xact::WriteBackFull>(*txevt, CUR_CYCLE);
        }
        else if (opcode == (uint8_t)CCHIOpcodeEVT::Evict) {
            xact = std::make_shared<Xact::Evict>(*txevt, CUR_CYCLE);
        }
        else assert(false);
            
        this->Transactions.emplace(txevt->txnID, xact);

        // 3. 更新本地记分牌状态
        if (localBoard.count(addr) == 0) {
            localBoard.emplace(addr, localBoardEntry(CCHI::CacheState::INV, 0));
        }
        auto& entry = localBoard.at(addr);
        entry.inflight = true;
        entry.inflight_evt = true;

        // 4. 初始化物理层发送
        pendingTXEVT.init(txevt, 1);
        return true;
    }

    bool FCAgent::do_MakeUnique(paddr_t addr) {
        if (pendingTXREQ.is_pending()) return false;
        if (localBoard.count(addr) != 0 && localBoard.at(addr).inflight) return false;

        auto txreq = std::make_shared<CCHI::BundleChannelREQ>();
        txreq->txnID  = this->idpool.getid();
        txreq->opcode = (uint8_t)CCHIOpcodeREQ::MakeUnique;
        txreq->addr   = addr;

        auto xact = std::make_shared<Xact::MakeUnique>(*txreq, CUR_CYCLE);
        this->Transactions.emplace(txreq->txnID, xact);

        if (localBoard.count(addr) == 0) {
            localBoard.emplace(addr, localBoardEntry(CCHI::CacheState::INV, 0));
        }
        localBoard.at(addr).inflight = true;
        localBoard.at(addr).inflight_req = true;

        pendingTXREQ.init(txreq, 1);
        return true;
    }

    bool FCAgent::do_CleanShared(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state != CCHI::CacheState::UD);
        return do_CMO(addr, (uint8_t)CCHIOpcodeREQ::CleanShared);
    }

    bool FCAgent::do_CleanInvalid(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_CMO(addr, (uint8_t)CCHIOpcodeREQ::CleanInvalid);
    }

    bool FCAgent::do_MakeInvalid(paddr_t addr) {
        if (localBoard.count(addr)) assert(localBoard.at(addr).state == CCHI::CacheState::INV);
        return do_CMO(addr, (uint8_t)CCHIOpcodeREQ::MakeInvalid);
    }

    bool FCAgent::do_CMO(paddr_t addr, uint8_t opcode) {
        if (pendingTXREQ.is_pending()) return false;
        if (localBoard.count(addr) != 0 && localBoard.at(addr).inflight) return false;

        auto txreq = std::make_shared<CCHI::BundleChannelREQ>();
        txreq->txnID  = this->idpool.getid();
        txreq->opcode = opcode;
        txreq->addr   = addr;
        
        if (localBoard.count(addr) == 0) {
            localBoard.emplace(addr, localBoardEntry(CCHI::CacheState::INV, 0));
        }
        auto& entry = localBoard.at(addr);
        entry.inflight = true;
        entry.inflight_req = true;

        auto xact = std::make_shared<Xact::CacheManagementOperation>(*txreq, CUR_CYCLE, localBoard.at(addr).state);
        this->Transactions.emplace(txreq->txnID, xact);
        
        pendingTXREQ.init(txreq, 1);
        return true;
    }

    void FCAgent::random_test() {
        if (rand() % 100 < 5) { // 5% 概率
            do_ReadUnique(0x8000 + (rand() % 16) * 64); // 随机地址
        }
    }

} // namespace CCHIAgent