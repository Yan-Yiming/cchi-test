#include <cstdint>
#include <iostream>
#include "CHISequencer.hpp"
#include "VTestTop_CCHITEST_L2.h"

#define VTestTop VTestTop_CCHITEST_L2

void PutChannelTXREQ(VTestTop* top, CCHI::FCBundle* bundle) {
    auto& ch = bundle->txreq;
    top->io_cpuPort_txreq_valid = ch.valid;
    top->io_cpuPort_txreq_bits_txnID = ch.txnID;
    top->io_cpuPort_txreq_bits_opcode = ch.opcode;
    top->io_cpuPort_txreq_bits_addr = ch.addr;
    top->io_cpuPort_txreq_bits_size = ch.size;
    top->io_cpuPort_txreq_bits_expCompStash = ch.expCompStash;
}
void GetChannelTXREQ(VTestTop* top, CCHI::FCBundle* bundle) {
    bundle->txreq.ready = top->io_cpuPort_txreq_ready;
}

void PutChannelTXEVT(VTestTop* top, CCHI::FCBundle* bundle) {
    auto& ch = bundle->txevt;
    top->io_cpuPort_txevt_valid = ch.valid;
    top->io_cpuPort_txevt_bits_txnID = ch.txnID;
    top->io_cpuPort_txevt_bits_opcode = ch.opcode;
    top->io_cpuPort_txevt_bits_addr = ch.addr;
}
void GetChannelTXEVT(VTestTop* top, CCHI::FCBundle* bundle) {
    bundle->txevt.ready = top->io_cpuPort_txevt_ready;
}

// --- TXRSP: Agent -> DUT ---
void PutChannelTXRSP(VTestTop* top, CCHI::FCBundle* bundle) {
    auto& ch = bundle->txrsp;
    top->io_cpuPort_txrsp_valid = ch.valid;
    top->io_cpuPort_txrsp_bits_txnID = ch.txnID;
    top->io_cpuPort_txrsp_bits_opcode = ch.opcode;
}
void GetChannelTXRSP(VTestTop* top, CCHI::FCBundle* bundle) {
    bundle->txrsp.ready = top->io_cpuPort_txrsp_ready;
}

// --- TXDAT: Agent -> DUT ---
void PutChannelTXDAT(VTestTop* top, CCHI::FCBundle* bundle) {
    auto& ch = bundle->txdat;
    top->io_cpuPort_txdat_valid = ch.valid;
    top->io_cpuPort_txdat_bits_txnID = ch.txnID;
    top->io_cpuPort_txdat_bits_opcode = ch.opcode;
    // 拷贝 256-bit 数据
    std::memcpy(top->io_cpuPort_txdat_bits_data, ch.data, 32);
}
void GetChannelTXDAT(VTestTop* top, CCHI::FCBundle* bundle) {
    bundle->txdat.ready = top->io_cpuPort_txdat_ready;
}

// --- RXSNP: DUT -> Agent ---
void GetChannelRXSNP(VTestTop* top, CCHI::FCBundle* bundle) {
    auto& ch = bundle->rxsnp;
    ch.valid = top->io_cpuPort_rxsnp_valid;
    ch.txnID = top->io_cpuPort_rxsnp_bits_txnID;
    ch.opcode = top->io_cpuPort_rxsnp_bits_opcode;
    ch.addr = top->io_cpuPort_rxsnp_bits_addr;
}
void PutChannelRXSNP(VTestTop* top, CCHI::FCBundle* bundle) {
    top->io_cpuPort_rxsnp_ready = bundle->rxsnp.ready;
}

// --- RXRSP: DUT -> Agent ---
void GetChannelRXRSP(VTestTop* top, CCHI::FCBundle* bundle) {
    auto& ch = bundle->rxrsp;
    ch.valid = top->io_cpuPort_rxrsp_valid;
    ch.txnID = top->io_cpuPort_rxrsp_bits_txnID;
    ch.opcode = top->io_cpuPort_rxrsp_bits_opcode;
    ch.dbID = top->io_cpuPort_rxrsp_bits_dbID;
}
void PutChannelRXRSP(VTestTop* top, CCHI::FCBundle* bundle) {
    top->io_cpuPort_rxrsp_ready = bundle->rxrsp.ready;
}

// --- RXDAT: DUT -> Agent ---
void GetChannelRXDAT(VTestTop* top, CCHI::FCBundle* bundle) {
    auto& ch = bundle->rxdat;
    ch.valid = top->io_cpuPort_rxdat_valid;
    ch.txnID = top->io_cpuPort_rxdat_bits_txnID;
    ch.opcode = top->io_cpuPort_rxdat_bits_opcode;
    ch.dbID = top->io_cpuPort_rxdat_bits_dbID;
    std::memcpy(ch.data, top->io_cpuPort_rxdat_bits_data, 32);
}
void PutChannelRXDAT(VTestTop* top, CCHI::FCBundle* bundle) {
    top->io_cpuPort_rxdat_ready = bundle->rxdat.ready;
}