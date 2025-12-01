#include "BaseAgent.h"

void CCHIAgent::FCAgent::FIRE() {
    fire_txevt();
    fire_txreq();
    fire_rxsnp();
    fire_txrsp();
    fire_txdat();
    fire_rxrsp();
    fire_rxdat();
}

void CCHIAgent::FCAgent::SEND() {
    if (pendingTXEVT.is_pending()) {
        send_txevt(pendingTXEVT.info, 0);
    }
    if (pendingTXREQ.is_pending()) {
        send_txreq(pendingTXREQ.info, 0);
    }
    if (pendingTXDAT.is_pending()) {
        send_txdat(pendingTXDAT.info, 0);
    }
    if (pendingTXRSP.is_pending()) {
        send_txrsp(pendingTXRSP.info, 0);
    }
}

void CCHIAgent::FCAgent::fire_txevt() {
    if (this->port->txevt.fire()) {
        auto& chnEVT = this->port->txevt;

        // TODO: for verbose log
        switch (CCHIOpcodeEVT(chnEVT.opcode)) {
            case CCHIOpcodeEVT::Evict:
                break;
            case CCHIOpcodeEVT::WriteBackFull:
                break;
            default:
                assert(false);
        }

        chnEVT.valid = false;
        assert(pendingTXEVT.is_pending());
        pendingTXEVT.update();
        auto entry = this->Transactions.find(chnEVT.txnID);
        if (entry != this->Transactions.end()) {
            entry->second->FireTXEVT();
        }
    }
}

void CCHIAgent::FCAgent::fire_rxrsp() {
    if (this->port->rxrsp.fire()) {
        auto& chnRSP = this->port->rxrsp;

        // TODO: for verbose log
        switch (CCHIOpcodeRSP(chnRSP.opcode)) {
            case CCHIOpcodeRSP::CompDBIDResp:
                break;
            default:
                assert(false);
        }

        chnRSP.valid = false;
        auto entry = this->Transactions.find(chnRSP.txnID);
        if (entry != this->Transactions.end()) {
            entry->second->FireRXRSP();
        }
        if (CCHIOpcodeRSP(chnRSP.opcode) == CCHIOpcodeRSP::CompDBIDResp) {
            assert(this->DBID2TxnID.count(chnRSP.dbID) == 0);
            this->DBID2TxnID.insert(chnRSP.dbID, chnRSP.txnID);

            auto txdat = std::make_shared<CCHI::BundleChannelDAT>();
            txdat->txnID    = chnRSP.dbID;
            txdat->opcode   = uint8_t(CCHIOpcodeDAT_DOWN::CopyBackWrData);
            for (int i = 0; i < 32; ++i) {
                // TODO
                txdat->data[i] = (uint8_t)random();
            }
            pendingTXDAT.init(txdat, 4);
        }
    }
} 

void CCHIAgent::FCAgent::fire_txdat() {
    if (this->port->txdat.fire()) {
        auto& chnDAT = this->port->txdat;

        // TODO: for verbose log
        switch (CCHIOpcodeDAT_UP(chnDAT.opcode)) {
            case CCHIOpcodeDAT_UP::CompData:
                break;
            default:
                assert(false);
        }

        chnDAT.valid = false;
        assert((this->DBID2TxnID.count(chnDAT.txnID) == 1));
        auto txnID = this->DBID2TxnID[chnDAT.txnID];
        auto entry = this->Transactions.find(txnID);
        if (entry != this->Transactions.end()) {
            entry->second->FireTXDAT();
            if (entry->second->isComplete()) {
                this->Transactions.erase(txnID);
                this->localBoard[entry->second->info->addr].inflight = false;
            }
        }
    }
} 

void CCHIAgent::FCAgent::send_txevt(std::shared_ptr<CCHI::BundleChannelEVT> &txevt, uint8_t alias) {
    auto entry = this->localBoard.find(txevt->addr);
    if (entry == this->localBoard.end()) {
        assert(false);
        // localBoardEntry new_entry(XactType::EVT, CCHI::State::INV, 0);
        // this->localBoard.insert(std::make_pair(addr, new_entry));
    }
    else {
        this->localBoard[txevt->addr].state[alias]  = CCHI::State::INV;
        this->localBoard[txevt->addr].dirty[alias]  = 0;
        this->localBoard[txevt->addr].inflight      = true;
        this->localBoard[txevt->addr].inflight_xact = XactType::EVT;

        switch (CCHIOpcodeEVT(txevt->opcode)) { 
            case CCHIOpcodeEVT::Evict:
                break;
            case CCHIOpcodeEVT::WriteBackFull:
                this->Transactions.emplace(txevt->txnID, std::make_shared<Xact::XactionWriteBackFull>(txevt));
                break;
            default:
                break;
        }
    }

    this->port->txevt.valid     = true;
    this->port->txevt.addr      = txevt->addr;
    this->port->txevt.opcode    = txevt->opcode;
    this->port->txevt.txnID     = txevt->txnID;
}

void CCHIAgent::FCAgent::send_txreq(std::shared_ptr<CCHI::BundleChannelREQ> &txreq, uint8_t alias) {
    bool can_send = true;

    auto entry = this->localBoard.find(txreq->addr);
    if (entry == this->localBoard.end()) {
        localBoardEntry new_entry(XactType::REQ, CCHI::State::INV, 0, 0);
        this->localBoard.insert(std::make_pair(txreq->addr, new_entry));
    }
    else {
        if (entry->second.inflight) {
            switch (entry->second.inflight_xact) {
                case XactType::EVT: case XactType::SNP:
                    can_send = false;
                    break;
                case XactType::REQ:
                    assert(false);
                    break;
                default:
                    assert(false);
                    break;
            }
        }
        else {
            entry->second.inflight = true;
            entry->second.inflight_xact = XactType::REQ;
        }
    }

    this->port->txreq.valid     = can_send;
    this->port->txreq.txnID     = txreq->txnID;
    this->port->txreq.opcode    = txreq->opcode;
    this->port->txreq.addr      = txreq->addr;
    
}

void CCHIAgent::FCAgent::send_txdat(std::shared_ptr<CCHI::BundleChannelDAT> &txdat, uint8_t alias) {
    this->port->txdat.valid     = true;
    *this->port->txdat.data     = *txdat->data;
    this->port->txdat.be        = txdat->be;
    this->port->txdat.opcode    = txdat->opcode;
    this->port->txdat.txnID     = txdat->txnID;
}

bool CCHIAgent::FCAgent::do_WriteBackFull(paddr_t addr) {
    if (pendingTXEVT.is_pending())
        return false;

    auto txevt = std::make_shared<CCHI::BundleChannelEVT>();
    txevt->txnID    = this->idpool.getid();
    txevt->opcode   = uint8_t(CCHIOpcodeEVT::WriteBackFull);
    txevt->addr     = addr;
    pendingTXEVT.init(txevt, 1);
    return true;
}

bool CCHIAgent::FCAgent::do_ReadUnique(paddr_t addr) {
    if (pendingTXREQ.is_pending())
        return false;
    
    auto txreq = std::make_shared<CCHI::BundleChannelREQ>();
    txreq->txnID    = this->idpool.getid();
    txreq->opcode   = uint8_t(CCHIOpcodeREQ::ReadUnique);
    txreq->addr     = addr;
    pendingTXREQ.init(txreq, 1);
    return true;
}