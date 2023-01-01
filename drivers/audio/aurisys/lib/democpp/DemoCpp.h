#ifndef DEMO_CPP_H
#define DEMO_CPP_H

#include <stdint.h>

using namespace std;

enum demo_status {
    DEMO_STATUS_CLOSE = 0,
    DEMO_STATUS_OPEN = 1,
};

/**
 * class for demonstrating C++ lib usage in ADSP
 **/
class DemoCpp {
public:
    DemoCpp();
    virtual ~DemoCpp();

    int32_t open(uint32_t ch, uint32_t sr);
    int32_t close();

    int32_t process(void * pIn, uint32_t inSize, void * pOut, uint32_t outSize);
    int32_t config(int32_t ch, int32_t sr);

    /**
     * return the class status
     */
    int32_t getStatus();
private:
    int32_t mStatus;
    int32_t mSampleRate;
    int32_t mChannels;
};




#endif /* end of DEMO_CPP_H */

