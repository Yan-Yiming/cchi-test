#include <cstdint>
#include <iostream>
#include "CHISequencer.hpp"

#define BUNDLE (chitest->fcagent->port)

void PutChannelTXREQ(VTestTop* top, CHISequencer* chitest) {
    auto& ch = BUNDLE->txreq;
    top->io_port_txreq_valid = ch.valid;
    top->io_port_txreq_bits_txnID = ch.txnID;
    top->io_port_txreq_bits_opcode = ch.opcode;
    top->io_port_txreq_bits_addr = ch.addr;
    top->io_port_txreq_bits_size = ch.size;
    top->io_port_txreq_bits_expCompStash = ch.expCompStash;
}
void GetChannelTXREQ(VTestTop* top, CHISequencer* chitest) {
    BUNDLE->txreq.ready = top->io_port_txreq_ready;
}

void PutChannelTXEVT(VTestTop* top, CHISequencer* chitest) {
    auto& ch = BUNDLE->txevt;
    top->io_port_txevt_valid = ch.valid;
    top->io_port_txevt_bits_txnID = ch.txnID;
    top->io_port_txevt_bits_opcode = ch.opcode;
    top->io_port_txevt_bits_addr = ch.addr;
}
void GetChannelTXEVT(VTestTop* top, CHISequencer* chitest) {
    BUNDLE->txevt.ready = top->io_port_txevt_ready;
}

// --- TXRSP: Agent -> DUT ---
void PutChannelTXRSP(VTestTop* top, CHISequencer* chitest) {
    auto& ch = BUNDLE->txrsp;
    top->io_port_txrsp_valid = ch.valid;
    top->io_port_txrsp_bits_txnID = ch.txnID;
    top->io_port_txrsp_bits_opcode = ch.opcode;
}
void GetChannelTXRSP(VTestTop* top, CHISequencer* chitest) {
    BUNDLE->txrsp.ready = top->io_port_txrsp_ready;
}

// --- TXDAT: Agent -> DUT ---
void PutChannelTXDAT(VTestTop* top, CHISequencer* chitest) {
    auto& ch = BUNDLE->txdat;
    top->io_port_txdat_valid = ch.valid;
    top->io_port_txdat_bits_txnID = ch.txnID;
    top->io_port_txdat_bits_opcode = ch.opcode;
    // 拷贝 256-bit 数据
    std::memcpy(top->io_port_txdat_bits_data, ch.data, 32);
}
void GetChannelTXDAT(VTestTop* top, CHISequencer* chitest) {
    BUNDLE->txdat.ready = top->io_port_txdat_ready;
}

// --- RXSNP: DUT -> Agent ---
void GetChannelRXSNP(VTestTop* top, CHISequencer* chitest) {
    auto& ch = BUNDLE->rxsnp;
    ch.valid = top->io_port_rxsnp_valid;
    ch.txnID = top->io_port_rxsnp_bits_txnID;
    ch.opcode = top->io_port_rxsnp_bits_opcode;
    ch.addr = top->io_port_rxsnp_bits_addr;
}
void PutChannelRXSNP(VTestTop* top, CHISequencer* chitest) {
    top->io_port_rxsnp_ready = BUNDLE->rxsnp.ready;
}

// --- RXRSP: DUT -> Agent ---
void GetChannelRXRSP(VTestTop* top, CHISequencer* chitest) {
    auto& ch = BUNDLE->rxrsp;
    ch.valid = top->io_port_rxrsp_valid;
    ch.txnID = top->io_port_rxrsp_bits_txnID;
    ch.opcode = top->io_port_rxrsp_bits_opcode;
    ch.dbID = top->io_port_rxrsp_bits_dbID;
}
void PutChannelRXRSP(VTestTop* top, CHISequencer* chitest) {
    top->io_port_rxrsp_ready = BUNDLE->rxrsp.ready;
}

// --- RXDAT: DUT -> Agent ---
void GetChannelRXDAT(VTestTop* top, CHISequencer* chitest) {
    auto& ch = BUNDLE->rxdat;
    ch.valid = top->io_port_rxdat_valid;
    ch.txnID = top->io_port_rxdat_bits_txnID;
    ch.opcode = top->io_port_rxdat_bits_opcode;
    ch.dbID = top->io_port_rxdat_bits_dbID;
    std::memcpy(ch.data, top->io_port_rxdat_bits_data, 32);
}
void PutChannelRXDAT(VTestTop* top, CHISequencer* chitest) {
    top->io_port_rxdat_ready = BUNDLE->rxdat.ready;
}