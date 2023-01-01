#include <audio_kal_hal.h>

uint64_t get_timestamp_ns(void)
{
        uint64_t time_ns = 0;

#if defined(CFG_MTK_AUDIODSP_SUPPORT)
	time_ns = timer_get_ap_timestamp();
#elif defined(CFG_SCP_AUDIO_FW_SUPPORT)
	time_ns = read_xgpt_stamp_ns();
#else
	time_ns = 0;
#endif
	return time_ns;
}


