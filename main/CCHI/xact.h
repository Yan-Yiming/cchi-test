#include <cstdint>
#include <assert.h>
#include "base.hpp"

namespace Xact {

    class Xaction {
    public:
        virtual void    FireTXEVT() = 0;
        virtual void    FireRXRSP() = 0;
        virtual void    FireTXDAT() = 0;
    };

    class XactionEVT : public Xaction {
    public:
        CCHI::BundleChannelEVT info;
    };

    class XactionREQ : public Xaction {
        CCHI::BundleChannelREQ info;
    };

    class XactionSNP : public Xaction {
        CCHI::BundleChannelSNP info;
    };

    class XactionWriteBackFull : public XactionEVT {
    private:
        bool    isWriteBackFullComplete;
        bool    isCompDBIDRespComplete;
        bool    isCopyBackWrDataComplete;

    public:
        bool    isComplete;

        void FireTXEVT() {
            assert(isWriteBackFullComplete == false);
            isWriteBackFullComplete = true;
        }
        void FireRXRSP() {
            assert(isCompDBIDRespComplete == false);
            isCompDBIDRespComplete = true;
        }
        void FireTXDAT() {
            assert(isCopyBackWrDataComplete == false);
            isCopyBackWrDataComplete = true;
            isComplete = true;
        }
    };
}