#pragma once

#include <cstdint>

#include "cchi_flit.hpp"


namespace CCHI {

    enum class XactionType {
        NonCacheableRead = 0,
        CacheableRead = 1,
        Evict,
        CopyBackWrite,
        Dataless,
        CMO,
        Stash,
        NonCacheableWrite,
        CacheableWrite,
        Snoop
    };


    enum class FiredRequestFlitType {
        EVT,
        SNP,
        REQ
    };

    enum class FiredResponseFlitType {
        TXRSP,
        RXRSP,
        TXDAT,
        RXDAT
    };

    class FiredFlit {
    public:
        uint64_t    time;

    public:
        FiredFlit(uint64_t time) noexcept;

    public:
        virtual bool    IsTX() const noexcept;
        virtual bool    IsRX() const noexcept;
    };

    class FiredRequestFlit : public FiredFlit {
    public:
        FiredRequestFlitType    type;

        union {
            FlitEVT     evt;
            FlitREQ     req;
            FlitSNP     snp;
        };

    public:
        bool            IsTXEVT() const noexcept;
        bool            IsRXSNP() const noexcept;
        bool            IsTXREQ() const noexcept;
    };

    class FiredResponseFlit : public FiredFlit {
    public:
        FiredResponseFlitType   type;

        union {
            FlitTXRSP   txrsp;
            FlitRXRSP   rxrsp;
            FlitTXDAT   txdat;
            FlitRXDAT   rxdat;
        };

    public:
        bool            IsTXRSP() const noexcept;
        bool            IsRXRSP() const noexcept;
        bool            IsTXDAT() const noexcept;
        bool            IsRXDAT() const noexcept;
    };
}
