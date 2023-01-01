#ifndef MTK_AUDIO_VOIP_C2C_CLIENT_H
#define MTK_AUDIO_VOIP_C2C_CLIENT_H

#include "audio_voip_c2c_common.h"

#include "audio_sw_mixer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 *                     public function
 * =============================================================================
 */

void audio_voip_c2c_client_init(void);
voip_c2c_status audio_voip_c2c_client_attach_sw_mixer (const uint32_t id, struct sw_mixer_attr_t *attr);
voip_c2c_status audio_voip_c2c_client_write(void *buf, uint32_t size);
voip_c2c_status audio_voip_c2c_client_detach_sw_mixer(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of MTK_AUDIO_VOIP_C2C_CLIENT_H */




