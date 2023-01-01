#include "audio_voip_c2c_client.h"

#include "audio_voip_c2c_common.h"

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>

#include "adsp_c2c_ipc.h"
#include "audio_assert.h"
#include "mt_printf.h"

#ifdef __cplusplus
extern "C" {
#endif



/*
 * =============================================================================
 *                     Macro
 * =============================================================================
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "[voip_c2c_client]"


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

/* C2C Server Struct */
struct voip_c2c_client_t {
	void *share_buf;
	uint32_t share_buf_size;
};

/*
 * =============================================================================
 *                     Member
 * =============================================================================
 */

struct voip_c2c_client_t voip_c2c_client;

/*
 * =============================================================================
 *                     Function
 * =============================================================================
 */

voip_c2c_status audio_voip_c2c_client_attach_sw_mixer (
	const uint32_t id,
	struct sw_mixer_attr_t *attr)
{
	PRINTF_D(LOG_TAG "%s()+, id = %d, attr buf_size = %d, fmt = %d, sr = %d, ch = %d\n", __func__,
		id, attr->buffer_size, attr->fmt_cfg.audio_format, attr->fmt_cfg.sample_rate, attr->fmt_cfg.num_channels);

	adsp_c2c_status status = adsp_c2c_ipc_send(
		ADSP_C2C_VOIP_ID,
		ADSP_C2C_MSG_NEED_ACK,
		AUD_VOIP_C2C_ATTACH_MSG,
		0,
		0,
		id,
		attr,
		sizeof(*attr));

	PRINTF_D(LOG_TAG "%s()-, adsp_c2c_status = %d\n", __func__, status);

	return (status != ADSP_C2C_ERROR ? VOIP_C2C_NO_ERROR : VOIP_C2C_ERROR);
}

voip_c2c_status audio_voip_c2c_client_detach_sw_mixer(void)
{
	PRINTF_D(LOG_TAG "%s()+\n", __func__);

	adsp_c2c_status status = adsp_c2c_ipc_send(
		ADSP_C2C_VOIP_ID,
		ADSP_C2C_MSG_NEED_ACK,
		AUD_VOIP_C2C_DETACH_MSG,
		0,
		0,
		0,
		0,
		0);

	PRINTF_D(LOG_TAG "%s()-, adsp_c2c_status = %d\n", __func__, status);

	return (status != ADSP_C2C_ERROR ? VOIP_C2C_NO_ERROR : VOIP_C2C_ERROR);
}

voip_c2c_status audio_voip_c2c_client_write(void *buf, uint32_t size)
{
	PRINTF_V(LOG_TAG "%s()+, buf = %p, size = %zu (share buf = %p, size = %d)\n", __func__, buf, size, voip_c2c_client.share_buf, size);

	if (size > voip_c2c_client.share_buf_size) {
		PRINTF_E(LOG_TAG "%s()-, C2C message is buffer overflow.  (data size = %d, buf size = %d)\n", __func__, sizeof(struct sw_mixer_attr_t), voip_c2c_client.share_buf_size);
		return VOIP_C2C_ERROR;
	}

	memcpy(voip_c2c_client.share_buf, buf, size);

	struct adsp_c2c_data_t c2c_data;
	c2c_data.addr = (uint8_t*)voip_c2c_client.share_buf;
	c2c_data.size = size;

	adsp_c2c_status status = adsp_c2c_ipc_send(
		ADSP_C2C_VOIP_ID,
		ADSP_C2C_MSG_NEED_ACK,
		AUD_VOIP_C2C_WRITE_MSG,
		0,
		0,
		0,
		&c2c_data,
		sizeof(c2c_data));

	PRINTF_V(LOG_TAG "%s()-, adsp_c2c_status = %d\n", __func__, status);

	return (status != ADSP_C2C_ERROR ? VOIP_C2C_NO_ERROR : VOIP_C2C_ERROR);
}

void audio_voip_c2c_client_init(void)
{
	PRINTF_D(LOG_TAG "%s()+", __func__);

	/* Init member */
	memset(&voip_c2c_client, 0, sizeof(voip_c2c_client));
	adsp_c2c_get_share_mem(ADSP_C2C_VOIP_ID, &voip_c2c_client.share_buf_size, &voip_c2c_client.share_buf);

	PRINTF_D(LOG_TAG "%s()-, share buf = 0x%p, buf size = %d\n", __func__, voip_c2c_client.share_buf, voip_c2c_client.share_buf_size);
}

#ifdef __cplusplus
}  /* extern "C" */
#endif


