#include "audio_voip_c2c_server.h"

#include "audio_voip_c2c_common.h"

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "semphr.h"

#include "audio_assert.h"
#include "audio_sw_mixer.h"
#include "audio_task.h"
#include "adsp_c2c_ipc.h"
#include "mt_printf.h"

#include <audio_ringbuf.h>
#include <audio_task_utility.h>

#include <aurisys_controller.h>
#include <wrapped_audio.h>

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
#define LOG_TAG "[voip_c2c_server]"

#define LOCAL_TASK_NAME "VoIP_C2C_DL"
#define LOCAL_TASK_STACK_SIZE 2048
#define LOCAL_TASK_PRIORITY 2


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

/* C2C Server Struct */
struct voip_c2c_server_t {
	TaskHandle_t voip_c2c_task_handle;
	struct adsp_c2c_ipc_msg_t c2c_msg;
	void *mixer_source;
};

/*
 * =============================================================================
 *                     Member
 * =============================================================================
 */

struct voip_c2c_server_t voip_c2c_server;


/*
 * =============================================================================
 *                     Function
 * =============================================================================
 */

void audio_voip_c2c_server_c2c_handler(struct adsp_c2c_ipc_msg_t *c2c_msg)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	voip_c2c_server.c2c_msg = *c2c_msg;
	vTaskNotifyGiveFromISR(voip_c2c_server.voip_c2c_task_handle,
			       &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void audio_voip_c2c_server_loop(void *pvParameters)
{
	struct voip_c2c_server_t *server = (struct voip_c2c_server_t *)pvParameters;

	PRINTF_D(LOG_TAG "%s()", __func__);

	adsp_c2c_registration(ADSP_C2C_VOIP_ID, audio_voip_c2c_server_c2c_handler);

	while (1) {
		PRINTF_V(LOG_TAG "%s(), waiting c2c message...", __func__);

		if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != pdTRUE) {
			PRINTF_W(LOG_TAG "%s(), error\n", __func__);
			continue;
		}

		PRINTF_V(LOG_TAG
			 "%s()+, Got c2c message! (msg_id = %d, msg payload = %p, size = %zu)", __func__,
			 server->c2c_msg.msg_id, server->c2c_msg.payload, server->c2c_msg.payload_len);

		/* Process C2C message */
		struct adsp_c2c_ipc_msg_t *c2c_msg = &server->c2c_msg;

		switch (c2c_msg->msg_id) {
		case AUD_VOIP_C2C_ATTACH_MSG:
			/* Check size */
			if (c2c_msg->payload_len != (uint8_t)sizeof(struct sw_mixer_attr_t)) {
				PRINTF_E(LOG_TAG "%s(), wrong message size (%d != %d)\n", __func__,
					 c2c_msg->payload_len, sizeof(struct sw_mixer_attr_t));
				configASSERT(0);
				break;
			}

			/* SWMixer attach */
			if (server->mixer_source != NULL) {
				PRINTF_E(LOG_TAG "%s(), mixer_source already attached! (0x%p)\n", __func__,
					 server->mixer_source);
				configASSERT(0);
				break;
			}

			struct sw_mixer_attr_t *attr = (struct sw_mixer_attr_t *)c2c_msg->payload;
			struct buf_attr out_attr;

			if (!get_aurisys_on() || attr->fmt_cfg.num_channels == 0 || attr->fmt_cfg.sample_rate == 0) { // TODO: get attr directly form audio HAL
				get_swmixer_out_attr(&out_attr);
				attr->fmt_cfg.audio_format = AUDIO_FORMAT_PCM_32_BIT;
				attr->fmt_cfg.num_channels = out_attr.channel;
				attr->fmt_cfg.sample_rate = out_attr.rate;
			}

			server->mixer_source =
				sw_mixer_source_attach((const uint32_t)c2c_msg->param, attr);

			PRINTF_D(LOG_TAG
				 "%s(), sw_mixer_source_attach %s. (mixer id = %d, mixer_source = 0x%p, buf_size = %d, fmt = %d, sr = %d, ch = %d)",
				 __func__, server->mixer_source != NULL ? "success" : "fail",
				 (const uint32_t)c2c_msg->param, server->mixer_source,
				 attr->buffer_size, attr->fmt_cfg.audio_format, attr->fmt_cfg.sample_rate,
				 attr->fmt_cfg.num_channels);

			if (server->mixer_source == NULL)
				configASSERT(0);
			break;
		case AUD_VOIP_C2C_DETACH_MSG:
			/* SWMixer detach */
			if (server->mixer_source) {
				sw_mixer_source_detach(server->mixer_source);
				server->mixer_source = NULL;
			} else
				PRINTF_W(LOG_TAG "%s(), mixer_source is NULL, cannot detach\n", __func__);
			break;
		case AUD_VOIP_C2C_WRITE_MSG:
			/* SWMixer write */
			if (server->mixer_source) {
				struct adsp_c2c_data_t *c2c_data = (struct adsp_c2c_data_t *)c2c_msg->payload;
				sw_mixer_source_write(server->mixer_source, c2c_data->addr, c2c_data->size);
			} else
				PRINTF_W(LOG_TAG "%s(), mixer_source is NULL, cannot write\n", __func__);
			break;
		default:
			PRINTF_E(LOG_TAG "%s(), c2c_msg id(%d) err\n", __func__, c2c_msg->msg_id);
			configASSERT(0);
			break;
		}

		PRINTF_V(LOG_TAG "%s()-, Complete C2C msg processing. (msg_id = %d)", __func__,
			 c2c_msg->msg_id);

		/* Ack C2C */
		if (c2c_msg->ack_type == ADSP_C2C_MSG_NEED_ACK) {
			adsp_c2c_ipc_send(
				c2c_msg->adsp_c2c_id,
				ADSP_C2C_MSG_ACK_BACK,
				0,
				0,
				0,
				0,
				NULL,
				0);
		}
	}
}

void audio_voip_c2c_server_init(void)
{
	BaseType_t xReturn = pdFAIL;

	PRINTF_D(LOG_TAG "%s()", __func__);

	/* Init member */
	memset(&voip_c2c_server, 0, sizeof(voip_c2c_server));

	/* Create c2c server task */
	xReturn = xTaskCreate(
			  audio_voip_c2c_server_loop,
			  LOCAL_TASK_NAME,
			  LOCAL_TASK_STACK_SIZE,
			  (void *)&voip_c2c_server,
			  LOCAL_TASK_PRIORITY,
			  &voip_c2c_server.voip_c2c_task_handle);

	AUD_ASSERT(xReturn == pdPASS);
}

#ifdef __cplusplus
}  /* extern "C" */
#endif


