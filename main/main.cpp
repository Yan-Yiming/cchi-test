#include <cstdint>
#include <iostream>
#include <verilated_fst_c.h>

static bool wave_enable     = true;
static uint64_t wave_begin  = 0;
static uint64_t wave_end    = 0;

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

int main(int argc, char **argv){
    uint64_t time = 0;

    Verilated::commandArgs(argc, argv);

    CHISequencer*   chitest;

    fst = nullptr;
    top = new VTestTop;

    while (chitest->IsAlive()) {

        chitest->Tick(time);

        GetChannelRXSNP(top, chitest);
        GetChannelRXRSP(top, chitest);
        GetChannelRXDAT(top, chitest);

        chitest->Tock();

        PutChannelTXEVT(top, chitest);
        PutChannelTXREQ(top, chitest);
        PutChannelTXRSP(top, chitest);
        PutChannelTXDAT(top, chitest);

        EvalNegedge(time, top);

        // ready
        PutChannelRXSNP(top, chitest);
        PutChannelRXRSP(top, chitest);
        PutChannelRXDAT(top, chitest);

        GetChannelTXEVT(top, chitest);
        GetChannelTXREQ(top, chitest);
        GetChannelTXRSP(top, chitest);
        GetChannelTXDAT(top, chitest);

        EvalPosedge(time, top);

        if (!(time % 10000))
            std::cout << "[chi-test] Simulation time elapsed: " << time << "ps" << std::endl;
    }

    return 0;
}