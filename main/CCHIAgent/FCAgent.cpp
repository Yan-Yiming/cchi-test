#include "BaseAgent.h"

void CCHIAgent::FCAgent::handle_channel() {
    fire_txevt();
    fire_txreq();
    fire_rxsnp();
    fire_txrsp();
    fire_txdat();
    fire_rxrsp();
    fire_rxdat();
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
        auto entry = this->evtTransactions.find(chnEVT.txnID);
        if (entry != this->evtTransactions.end())
            entry.F
    }
}

void CCHIAgent::FCAgent::do_writebackfull(paddr_t addr) {

}