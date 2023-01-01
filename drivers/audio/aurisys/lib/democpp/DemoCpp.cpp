#include <stdio.h>
#include <string.h>

#include "DemoCpp.h"


DemoCpp::DemoCpp() :
    mStatus(DEMO_STATUS_CLOSE),
    mSampleRate(0),
    mChannels(0) {
    printf("DemoCpp construt\n");
}

DemoCpp::~DemoCpp() {
    printf("DemoCpp deconstruct\n");
}

int32_t DemoCpp::getStatus() {
    return mStatus;
}

int32_t DemoCpp::open(uint32_t ch, uint32_t sr) {
    mStatus = DEMO_STATUS_OPEN;
    mChannels = ch;
    mSampleRate = sr;
    return 0;
}

int32_t DemoCpp::close() {
    mStatus = DEMO_STATUS_CLOSE;
    return 0;
}

int32_t DemoCpp::process(void * pIn, uint32_t inSize, void * pOut, uint32_t outSize) {
    if (inSize > outSize)
        return -1;

    if (mStatus == DEMO_STATUS_OPEN) {
        memcpy(pOut, pIn, inSize);
        return inSize;
    } else {
        return -1;
    }
}

int32_t DemoCpp::config(int32_t ch, int32_t sr) {
    mSampleRate = sr;
    mChannels = ch;
    return 0;
}

