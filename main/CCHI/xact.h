#include <cstdint>
#include <assert.h>
#include "cchi_base.hpp"

enum class XactType {
    EVT = 0,
    SNP = 1,
    REQ = 2
};

namespace Xact {

    class Xaction {
    public:
        virtual void    FireTXEVT() = 0;
        virtual void    FireTXREQ() = 0;
        virtual void    FireTXRSP() = 0;
        virtual void    FireTXDAT() = 0;
        virtual void    FireRXRSP() = 0;
        virtual void    FireRXDAT() = 0;
        virtual bool    isComplete() {
            return isComplete;
        }
    };

    class XactionEVT : public Xaction {
    public:
        CCHI::BundleChannelEVT info;
    };

    class XactionREQ : public Xaction {
    public:
        CCHI::BundleChannelREQ info;
    };

    class XactionSNP : public Xaction {
        public:
        CCHI::BundleChannelSNP info;
    };

    class XactionWriteBackFull : public XactionEVT {
    private:
        bool    isWriteBackFullComplete;
        bool    isCompDBIDRespComplete;
        bool    isCopyBackWrDataComplete;

    public:
        bool    isComplete;

        inline XactionWriteBackFull(std::shared_ptr<CCHI::BundleChannelEVT> &info) {
            this->info = *info;
            this->isWriteBackFullComplete   = false;
            this->isCompDBIDRespComplete    = false;
            this->isCopyBackWrDataComplete  = false;
            this->isComplete                = false;
        }

        void FireTXEVT() override {
            assert(isWriteBackFullComplete == false);
            isWriteBackFullComplete = true;
        }
        void FireRXRSP() override {
            assert(isCompDBIDRespComplete == false);
            isCompDBIDRespComplete = true;
        }
        void FireTXDAT() override {
            assert(isCopyBackWrDataComplete == false);
            isCopyBackWrDataComplete = true;
            isComplete = true;
        }
    };

    class XactionReadUnique : public XactionREQ {
    private:
        bool    isReadUniqueComplete;
        bool    isCompDataComplete;
        bool    isCompAckComplete;
    public:
        bool    isComplete;
    
        inline XactionReadUnique(std::shared_ptr<CCHI::BundleChannelREQ> &info) {
            this->info = *info;
            this->isReadUniqueComplete  = false;
            this->isCompDataComplete    = false;
            this->isCompAckComplete     = false;
            this->isComplete            = false;
        }

        void FireTXREQ() override {
            assert(isReadUniqueComplete == false);
            isReadUniqueComplete = true;
        }
        void FireRXDAT(int ) override {
            assert(isCompDataComplete == false);
            isCompDataComplete = true;
        }
        void FireTXRSP() override {
            assert(isCompAckComplete == false);
            isCompAckComplete = true;
            isComplete = true;
        }
    };
}