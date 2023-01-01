#ifndef MTK_AUDIO_VOIP_C2C_COMMON_H
#define MTK_AUDIO_VOIP_C2C_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 *                     Definition
 * =============================================================================
 */

#define DEBUG 0

#define PRINTF_V(fmt, ...) do { if (DEBUG) PRINTF_D(fmt, __VA_ARGS__); } while (0)

enum voip_c2c_msg {
	AUD_VOIP_C2C_ATTACH_MSG,
	AUD_VOIP_C2C_DETACH_MSG,
	AUD_VOIP_C2C_WRITE_MSG
};

enum voip_c2c_status {
	VOIP_C2C_ERROR = -1,
	VOIP_C2C_NO_ERROR = 0,
};

typedef enum voip_c2c_status voip_c2c_status;


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of MTK_AUDIO_VOIP_C2C_COMMON_H */

