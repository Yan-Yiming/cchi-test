#include <cstdint>
#include <iostream>
#include "verilated_fst_c.h"
#include "../verilated/VTestTop_CCHITEST_L2.h"

#include "CHISequencer.hpp"
#include "port_connect.hpp"

static bool wave_enable     = true;
static uint64_t wave_begin  = 0;
static uint64_t wave_end    = 90000;

static VerilatedFstC*   fst;
static VTestTop*        top;

inline static bool IsInWaveTime(uint64_t time)
{
    if (wave_begin > wave_end)
        return false;

    if (wave_begin == 0 && wave_end == 0)
        return true;

    return time >= wave_begin && time <= wave_end;
}

inline static void EvalNegedge(uint64_t& time, VTestTop* top)
{
    top->clock = 0;
    top->eval();

    if (wave_enable && IsInWaveTime(time))
        fst->dump(time);

    time++;
}

inline static void EvalPosedge(uint64_t& time, VTestTop* top)
{
    top->clock = 1;
    top->eval();

    if (wave_enable && IsInWaveTime(time))
        fst->dump(time);

    time++;
}

inline static void reset(){
    top->reset = 1;
    for (int i = 0; i < 10; ++i){
        top->clock = 0;
        top->eval();
        top->clock = 1;
        top->eval();
    }
    top->reset = 0;
    for (int i = 0; i < 10; ++i){
        top->clock = 0;
        top->eval();
        top->clock = 1;
        top->eval();
    }
}

int main(int argc, char **argv){
    uint64_t time = 0;

    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    CHISequencer*   chitest = new CHISequencer();
    CCHI::FCBundle* bundle = new CCHI::FCBundle();
    
    chitest->fcagent->bindPort(bundle);

    fst = nullptr;
    top = new VTestTop;

    if (wave_enable) {
        Verilated::traceEverOn(true);
        fst = new VerilatedFstC;
        top->trace(fst, 99);
        fst->open("a.fst");
    }

    reset();

    while (chitest->IsAlive()) {
        bundle->rxsnp.ready = true;
        bundle->rxrsp.ready = true;
        bundle->rxdat.ready = true;

        chitest->Tick(time);

        GetChannelRXSNP(top, bundle);
        GetChannelRXRSP(top, bundle);
        GetChannelRXDAT(top, bundle);

        chitest->Tock();

        PutChannelTXEVT(top, bundle);
        PutChannelTXREQ(top, bundle);
        PutChannelTXRSP(top, bundle);
        PutChannelTXDAT(top, bundle);

        EvalNegedge(time, top);

        // ready
        PutChannelRXSNP(top, bundle);
        PutChannelRXRSP(top, bundle);
        PutChannelRXDAT(top, bundle);

        GetChannelTXEVT(top, bundle);
        GetChannelTXREQ(top, bundle);
        GetChannelTXRSP(top, bundle);
        GetChannelTXDAT(top, bundle);

        EvalPosedge(time, top);

        if (!(time % 10000))
            std::cout << "[chi-test] Simulation time elapsed: " << time << "ps" << std::endl;
        if (time >= 1000)
            break;
    }

    if (wave_enable)
        fst->close();

    return 0;
}