#include "aurisys_lib_handler.h"

#include <string.h>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */

#include <sys/types.h>
#include <unistd.h>

#include <uthash.h> /* uthash */
#include <utlist.h> /* linked list */

#include <wrapped_audio.h>
#include <wrapped_errors.h>

#include <audio_log_hal.h>
#include <audio_assert.h>
#include <audio_memory_control.h>
#include <audio_lock.h>
#include <audio_ringbuf_pure.h>
#include <audio_sample_rate.h>

#include <audio_task.h>

#include <arsi_type.h>
#include <aurisys_config.h>

#include <audio_pool_buf_handler.h>

#include <arsi_api.h>

#include <aurisys_utility.h>

#include <aurisys_adb_command.h>

#ifdef CFG_MTK_AUDIODSP_SUPPORT
#include <feature_manager.h>
#include <dvfs_config_parser.h>
#endif
#include <audio_messenger_ipi.h>
#include <audio_controller_msg_id.h>
#include <xgpt.h>

#ifdef CFG_SLB_SUPPORT
#include <slb.h>
#endif

#ifdef AURISYS_C2C_SYNC_SUPPORT
#include <adsp_c2c_ipc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "[AURI][HDL]"


#ifdef AURISYS_DUMP_LOG_V
#undef  AUD_LOG_V
#define AUD_LOG_V(x...) AUD_LOG_D(x)
#endif


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define PROCESS_DATA_SIZE_PER_TIME (4096)
#ifdef CFG_MTK_AUDIODSP_SUPPORT
#if CFG_ADSP_CORE_ID == CFG_ADSP_A
#define LOCAL_TASK_SCENE (TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A)
#elif CFG_ADSP_CORE_ID == CFG_ADSP_B
#define LOCAL_TASK_SCENE (TASK_SCENE_AUDIO_CONTROLLER_HIFI3_B)
#else
#define LOCAL_TASK_SCENE (TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A)
#endif
#else
#define LOCAL_TASK_SCENE (TASK_SCENE_AUDIO_CONTROLLER_RV)
#endif



#define LINEAR_TO_FRAME_2_CH(linear_buf, frame_buf, data_size, type) \
	do { \
		uint32_t channel_size = (data_size >> 1); \
		type *linear = (type *)(void *)(linear_buf); \
		type *frame_ch1 = (type *)(void *)(frame_buf); \
		type *frame_ch2 = (type *)(void *)(((char *)frame_ch1) + channel_size); \
		uint32_t num_sample = channel_size / sizeof(type); \
		uint32_t i = 0; \
		uint32_t j = 0; \
		for (i = 0; i < num_sample; i++) { \
			j = i << 1; \
			frame_ch1[i] = linear[j]; \
			frame_ch2[i] = linear[j + 1]; \
		} \
	} while(0)

#define LINEAR_TO_FRAME_3_CH(linear_buf, frame_buf, data_size, type) \
	do { \
		uint32_t channel_size = (data_size / 3); \
		type *linear = (type *)(void *)(linear_buf); \
		type *frame_ch1 = (type *)(void *)(frame_buf); \
		type *frame_ch2 = (type *)(void *)(((char *)frame_ch1) + channel_size); \
		type *frame_ch3 = (type *)(void *)(((char *)frame_ch2) + channel_size); \
		uint32_t num_sample = channel_size / sizeof(type); \
		uint32_t i = 0; \
		uint32_t j = 0; \
		for (i = 0; i < num_sample; i++) { \
			j = i * 3; \
			frame_ch1[i] = linear[j]; \
			frame_ch2[i] = linear[j + 1]; \
			frame_ch3[i] = linear[j + 2]; \
		} \
	} while(0)

#define LINEAR_TO_FRAME_4_CH(linear_buf, frame_buf, data_size, type) \
	do { \
		uint32_t channel_size = (data_size >> 2); \
		type *linear = (type *)(void *)(linear_buf); \
		type *frame_ch1 = (type *)(void *)(frame_buf); \
		type *frame_ch2 = (type *)(void *)(((char *)frame_ch1) + channel_size); \
		type *frame_ch3 = (type *)(void *)(((char *)frame_ch2) + channel_size); \
		type *frame_ch4 = (type *)(void *)(((char *)frame_ch3) + channel_size); \
		uint32_t num_sample = channel_size / sizeof(type); \
		uint32_t i = 0; \
		uint32_t j = 0; \
		for (i = 0; i < num_sample; i++) { \
			j = i << 2; \
			frame_ch1[i] = linear[j]; \
			frame_ch2[i] = linear[j + 1]; \
			frame_ch3[i] = linear[j + 2]; \
			frame_ch4[i] = linear[j + 3]; \
		} \
	} while(0)

#define FRAME_TO_LINEAR_2_CH(frame_buf, linear_buf, data_size, type) \
	do { \
		uint32_t channel_size = (data_size >> 1); \
		type *linear = (type *)(void *)(linear_buf); \
		type *frame_ch1 = (type *)(void *)(frame_buf); \
		type *frame_ch2 = (type *)(void *)(((char *)frame_ch1) + channel_size); \
		uint32_t num_sample = channel_size / sizeof(type); \
		uint32_t i = 0; \
		uint32_t j = 0; \
		for (i = 0; i < num_sample; i++) { \
			j = i << 1; \
			linear[j]     = frame_ch1[i]; \
			linear[j + 1] = frame_ch2[i]; \
		} \
	} while(0)

#define FRAME_TO_LINEAR_3_CH(frame_buf, linear_buf, data_size, type) \
	do { \
		uint32_t channel_size = (data_size / 3); \
		type *linear = (type *)(void *)(linear_buf); \
		type *frame_ch1 = (type *)(void *)(frame_buf); \
		type *frame_ch2 = (type *)(void *)(((char *)frame_ch1) + channel_size); \
		type *frame_ch3 = (type *)(void *)(((char *)frame_ch2) + channel_size); \
		uint32_t num_sample = channel_size / sizeof(type); \
		uint32_t i = 0; \
		uint32_t j = 0; \
		for (i = 0; i < num_sample; i++) { \
			j = i * 3; \
			linear[j]     = frame_ch1[i]; \
			linear[j + 1] = frame_ch2[i]; \
			linear[j + 2] = frame_ch3[i]; \
		} \
	} while(0)

#define FRAME_TO_LINEAR_4_CH(frame_buf, linear_buf, data_size, type) \
	do { \
		uint32_t channel_size = (data_size >> 2); \
		type *linear = (type *)(void *)(linear_buf); \
		type *frame_ch1 = (type *)(void *)(frame_buf); \
		type *frame_ch2 = (type *)(void *)(((char *)frame_ch1) + channel_size); \
		type *frame_ch3 = (type *)(void *)(((char *)frame_ch2) + channel_size); \
		type *frame_ch4 = (type *)(void *)(((char *)frame_ch3) + channel_size); \
		uint32_t num_sample = channel_size / sizeof(type); \
		uint32_t i = 0; \
		uint32_t j = 0; \
		for (i = 0; i < num_sample; i++) { \
			j = i << 2; \
			linear[j]     = frame_ch1[i]; \
			linear[j + 1] = frame_ch2[i]; \
			linear[j + 2] = frame_ch3[i]; \
			linear[j + 3] = frame_ch4[i]; \
		} \
	} while(0)



/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

#ifdef AURISYS_RESERVE_MEM
typedef struct aurisys_lib_reserve_mem_t {
	char *name;
	uint32_t dl_reserve_size;
	uint32_t ul_reserve_size;
} aurisys_lib_reserve_mem_t;
#endif

#ifdef AURISYS_C2C_SYNC_SUPPORT
struct aurisys_lib_sync_info_t {
	uint8_t aurisys_scenario;
	uint8_t lib_id;
};

struct aurisys_c2c_sync_buf_t {
	data_buf_t share_buf_d2u;
	data_buf_t share_buf_u2d;
	data_buf_t d2u_buf;
	data_buf_t u2d_buf;
	struct aurisys_lib_sync_info_t lib_sync_info;
	bool updated;
	bool inited;
	struct alock_t *sync_lock;
};
#endif

/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static struct alock_t *g_aurisys_lib_handler_lock;

//static uint32_t mDumpFileNum = 0;

static char fake_aec_mem[640];
#ifdef AURISYS_RESERVE_MEM
static char *g_reserve_working_buf;
static struct alock_t *g_aurisys_reserve_mem_lock;
static aurisys_lib_reserve_mem_t aurisys_reserve_mem[] = {
#if MTKSPEECH_LIB_SUPPORT
	{"mtk_sp",        MTK_SPH_DL_RESERVE_SIZE,    MTK_SPH_UL_RESERVE_SIZE},
#endif
#if !FV_DUMMY_LIB
	{"fv_speech",     FV_SPH_DL_RESERVE_SIZE,     FV_SPH_UL_RESERVE_SIZE},
#endif
#if !NXPSPEECH_DUMMY_LIB
	{"nxp_speech",    NXP_SPH_DL_RESERVE_SIZE,    NXP_SPH_UL_RESERVE_SIZE},
#endif
#if !NXPRECORD_DUMMY_LIB
	{"nxp_record",    0,                          NXP_REC_UL_RESERVE_SIZE}
#endif
};
static bool ul_reserve_mem = false;
static bool dl_reserve_mem = false;
static uint32_t ul_reserve_max_size = 0;
static uint32_t dl_reserve_max_size = 0;
#endif

#ifdef AURISYS_C2C_SYNC_SUPPORT
static struct aurisys_c2c_sync_buf_t aurisys_c2c_sync_buf;
#endif

/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static void arsi_lib_printf(const char *message, ...);
static void arsi_lib_printf_dummy(const char *message, ...);

static void allocate_data_buf(audio_buf_t *audio_buf);
/* NOTE: free data buf in release_lib_config */

static void init_audio_buf_by_lib_config(
	audio_buf_t *audio_buf,
	arsi_lib_config_t *lib_config);

static void clone_lib_config(
	arsi_lib_config_t *des,
	arsi_lib_config_t *src,
	const aurisys_component_t *the_component,
	const struct aurisys_lib_manager_config_t *manager_config);
static void release_lib_config(arsi_lib_config_t *lib_config);

static void init_pool_buf(aurisys_lib_handler_t *lib_handler);
static void deinit_pool_buf(aurisys_lib_handler_t *lib_handler);


static void aurisys_destroy_lib_handler_list_xlink(
	aurisys_lib_handler_t **handler_list);

#ifdef AURISYS_RESERVE_MEM
static int alloc_lib_reserved_mem(aurisys_lib_handler_t *lib_handler,
	data_buf_t *working_buf);
static int free_lib_reserved_mem(aurisys_lib_handler_t *lib_handler);
#endif

#ifdef AURISYS_C2C_SYNC_SUPPORT
static void aurisys_c2c_init(void);
static bool aurisys_lib_need_sync(uint8_t task_scene);
#endif
/*
 * =============================================================================
 *                     utilities declaration
 * =============================================================================
 */

static uint32_t get_size_per_channel(const audio_format_t audio_format);

static uint32_t get_frame_buf_size(const audio_buf_t *audio_buf);

static void char_to_string(string_buf_t *target, char *source, uint32_t mem_sz);

static int linear_to_frame_base(char *linear_buf, char *frame_buf,
				uint32_t data_size, audio_format_t audio_format, uint8_t num_channels);
static int frame_base_to_linear(char *frame_buf, char *linear_buf,
				uint32_t data_size, audio_format_t audio_format, uint8_t num_channels);
static inline uint64_t get_current_time_us(void);
static uint64_t get_expect_process_time_us(struct audio_buf_t *p_audio_buf, uint64_t process_size);


/*
 * =============================================================================
 *                     public function implementation
 * =============================================================================
 */

void aurisys_lib_handler_c_file_init(void)
{
#ifdef AURISYS_RESERVE_MEM
	uint32_t i = 0;
#endif
	if (g_aurisys_lib_handler_lock == NULL)
		NEW_ALOCK(g_aurisys_lib_handler_lock);
#ifdef AURISYS_RESERVE_MEM
	if (g_reserve_working_buf == NULL) {
		for (i = 0 ; i < sizeof(aurisys_reserve_mem) / sizeof(aurisys_lib_reserve_mem_t); i++) {
			if (dl_reserve_max_size < aurisys_reserve_mem[i].dl_reserve_size)
				dl_reserve_max_size = aurisys_reserve_mem[i].dl_reserve_size;
			if (ul_reserve_max_size < aurisys_reserve_mem[i].ul_reserve_size)
				ul_reserve_max_size = aurisys_reserve_mem[i].ul_reserve_size;
		}
		AUDIO_ALLOC_BUFFER(g_reserve_working_buf, dl_reserve_max_size + ul_reserve_max_size);
		AUD_LOG_D("%s, g_reserve_working_buf = %p, size = %d\n", __FUNCTION__,
			  g_reserve_working_buf, dl_reserve_max_size + ul_reserve_max_size);
	}
	if (g_aurisys_reserve_mem_lock == NULL)
		NEW_ALOCK(g_aurisys_reserve_mem_lock);
#endif

#ifdef AURISYS_C2C_SYNC_SUPPORT
	aurisys_c2c_init();
#endif
}


void aurisys_lib_handler_c_file_deinit(void)
{
#if 0 /* singleton lock */
	if (g_aurisys_lib_handler_lock != NULL)
		FREE_ALOCK(g_aurisys_lib_handler_lock);
#endif
}


void aurisys_destroy_lib_handler_list(aurisys_lib_handler_t **handler_list)
{
	LOCK_ALOCK_MS(g_aurisys_lib_handler_lock, 1000);

	aurisys_destroy_lib_handler_list_xlink(handler_list);

	UNLOCK_ALOCK(g_aurisys_lib_handler_lock);
}


void aurisys_arsi_create_handler(aurisys_lib_handler_t *lib_handler)
{
	AurisysLibInterface *arsi_api = NULL;
	void *arsi_handler = NULL;
	const arsi_task_config_t *arsi_task_config = NULL;
	const arsi_lib_config_t *arsi_lib_config  = NULL;

	data_buf_t          *working_buf;
	data_buf_t          *slb_buf;
#ifdef AURISYS_C2C_SYNC_SUPPORT
	data_buf_t          *d2u_buf, *u2d_buf;
#endif
	data_buf_t          *param_buf;

	status_t             retval = NO_ERROR;
	bool use_slb = false;
#ifdef CFG_SLB_SUPPORT
	void *slb_addr = NULL;
#endif

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	/* incase the arsi_handler is already created */
	if (lib_handler->arsi_handler != NULL) {
		AUD_LOG_W("%s(-), lib_handler->arsi_handler != NULL", __FUNCTION__);
		UNLOCK_ALOCK(lib_handler->lock);
		return;
	}

	arsi_api = lib_handler->api;
	arsi_task_config = lib_handler->task_config;
	arsi_lib_config = &lib_handler->lib_config;
	working_buf = &lib_handler->working_buf;
	slb_buf = &lib_handler->slb_buf;
#ifdef AURISYS_C2C_SYNC_SUPPORT
	d2u_buf = &lib_handler->syc_buf_d2u;
	u2d_buf = &lib_handler->syc_buf_u2d;
#endif

	param_buf = &lib_handler->param_buf;

#ifdef CFG_SLB_SUPPORT
	/* slb support */
	if (arsi_api->arsi_is_support_slb &&
	    (arsi_task_config->task_scene == TASK_SCENE_PHONE_CALL ||
	     arsi_task_config->task_scene == TASK_SCENE_VOIP))
		use_slb = arsi_api->arsi_is_support_slb();
	if (use_slb) {
		/* get slb form system */
		slb_buf->memory_size = get_slb_buffer(&slb_addr, 500);
		if (slb_addr == NULL || slb_buf->memory_size == 0) {
			AUD_LOG_E("%s(), get_slb_buffer fail!, addr: %p, size: %d",
				  __FUNCTION__, slb_addr, slb_buf->memory_size);
			slb_buf->p_buffer = NULL;
			slb_buf->memory_size = 0;
		} else {
			slb_buf->p_buffer = slb_addr;
			AUD_LOG_D("%s(), get slb, addr: %p, size: %d",
				  __FUNCTION__, slb_buf->p_buffer, slb_buf->memory_size);
		}
	}
#endif

	/* query working buf size */
	if (use_slb && arsi_api->arsi_query_working_buf_size_with_slb != NULL)
		retval = arsi_api->arsi_query_working_buf_size_with_slb(arsi_task_config,
				arsi_lib_config, slb_buf, &working_buf->memory_size, lib_handler->debug_log_fp);
	else
		retval = arsi_api->arsi_query_working_buf_size(arsi_task_config,
				arsi_lib_config, &working_buf->memory_size, lib_handler->debug_log_fp);

	if (working_buf->memory_size == 0 || retval != NO_ERROR) {
		AUD_LOG_E("%s(), lib_name %s, %p, working_buf->memory_size %u, retval %d",
			  __FUNCTION__, lib_handler->lib_name, lib_handler, working_buf->memory_size,
			  retval);
		AUD_ASSERT(working_buf->memory_size != 0);
		AUD_ASSERT(retval == NO_ERROR);
		UNLOCK_ALOCK(lib_handler->lock);
		return;
	} else {
		AUD_LOG_D("%s(), lib_name %s, memory_size %u", __FUNCTION__, lib_handler->lib_name, working_buf->memory_size);
	}

#ifdef AURISYS_RESERVE_MEM
	if (alloc_lib_reserved_mem(lib_handler, working_buf) != 0)
#endif
	{
		AUDIO_ALLOC_BUFFER(working_buf->p_buffer, working_buf->memory_size);
	}
	/* create handler */
	if (use_slb && arsi_api->arsi_create_handler_with_slb != NULL)
		retval = arsi_api->arsi_create_handler_with_slb(arsi_task_config, arsi_lib_config, param_buf, working_buf,
								slb_buf, &arsi_handler, lib_handler->debug_log_fp);
	else
		retval = arsi_api->arsi_create_handler(arsi_task_config, arsi_lib_config,
						       param_buf, working_buf, &arsi_handler, lib_handler->debug_log_fp);

	AUD_LOG_D("%s(), lib_name %s, %p, process_type %d, memory_size %u, working_buf %p, arsi_handler %p, retval 0x%x",
		  __FUNCTION__, lib_handler->lib_name, lib_handler, lib_handler->arsi_process_type, working_buf->memory_size,
		  working_buf->p_buffer, arsi_handler, retval);
	AUD_ASSERT(retval == NO_ERROR);
	AUD_ASSERT(arsi_handler != NULL);
	lib_handler->arsi_handler = arsi_handler;

	/* debug log */
	arsi_api->arsi_set_debug_log_fp(lib_handler->debug_log_fp,
					lib_handler->arsi_handler);

#ifdef AURISYS_C2C_SYNC_SUPPORT
	/* aurisys sync buf */
	LOCK_ALOCK_MS(aurisys_c2c_sync_buf.sync_lock, 500);
	if (arsi_api->arsi_query_sync_buf &&
	    aurisys_lib_need_sync(arsi_task_config->task_scene) &&
	    aurisys_c2c_sync_buf.lib_sync_info.lib_id == 0) {
		d2u_buf->memory_size = aurisys_c2c_sync_buf.d2u_buf.memory_size;
		u2d_buf->memory_size = aurisys_c2c_sync_buf.u2d_buf.memory_size;
		arsi_api->arsi_query_sync_buf(d2u_buf, u2d_buf, lib_handler->arsi_handler);

		if ((d2u_buf->data_size > d2u_buf->memory_size) ||
		    (u2d_buf->data_size > u2d_buf->memory_size) ||
		    (d2u_buf->memory_size != aurisys_c2c_sync_buf.d2u_buf.memory_size) ||
		    (u2d_buf->memory_size != aurisys_c2c_sync_buf.u2d_buf.memory_size)) {
			AUD_LOG_E("%s(), lib: %s sync buf size error!", __FUNCTION__, lib_handler->lib_name);
			d2u_buf->p_buffer = NULL;
			u2d_buf->p_buffer = NULL;
		} else {
			aurisys_c2c_sync_buf.lib_sync_info.aurisys_scenario = arsi_task_config->task_scene;
			aurisys_c2c_sync_buf.lib_sync_info.lib_id = lib_handler->lib_id;
			AUD_LOG_D("%s(), d2u_buf: %p, u2d_buf: %p, lib_id: %d, task_scene: %d",
				  __FUNCTION__, d2u_buf->p_buffer, u2d_buf->p_buffer,
				  lib_handler->lib_id, arsi_task_config->task_scene);
		}
	}
	UNLOCK_ALOCK(aurisys_c2c_sync_buf.sync_lock);
#endif

	/* process unit */
	if (arsi_api->arsi_query_process_unit_bytes == NULL) {
		lib_handler->ul_process_size_per_time = PROCESS_DATA_SIZE_PER_TIME;
		lib_handler->dl_process_size_per_time = PROCESS_DATA_SIZE_PER_TIME;
	} else {
		retval = arsi_api->arsi_query_process_unit_bytes(
				 &lib_handler->ul_process_size_per_time,
				 &lib_handler->dl_process_size_per_time,
				 lib_handler->arsi_handler);
		AUD_LOG_D("%s(), arsi_query_process_unit_bytes, ul %u, dl %u, retval %d",
			  __FUNCTION__,
			  lib_handler->ul_process_size_per_time,
			  lib_handler->dl_process_size_per_time,
			  retval);
	}

	/* debug dump */
	if (lib_handler->lib_dump_enabled) {
		retval = arsi_api->arsi_query_max_debug_dump_buf_size(
				 &lib_handler->lib_dump_buf, lib_handler->arsi_handler);
		AUD_LOG_D("%s(), arsi_query_max_debug_dump_buf_size, retval: %d, memory_size: %u",
			  __FUNCTION__, retval, lib_handler->lib_dump_buf.memory_size);

		if (retval == NO_ERROR &&
		    lib_handler->lib_dump_buf.memory_size > 0) {
			AUDIO_ALLOC_CHAR_BUFFER(lib_handler->lib_dump_buf.p_buffer,
						lib_handler->lib_dump_buf.memory_size);
		}
	}

	UNLOCK_ALOCK(lib_handler->lock);
}


void aurisys_arsi_destroy_handler(aurisys_lib_handler_t *lib_handler)
{
	AurisysLibInterface *arsi_api = NULL;

	status_t retval = NO_ERROR;


	LOCK_ALOCK_MS(lib_handler->lock, 500);

	/* incase the arsi_handler is already destroyed */
	if (lib_handler->arsi_handler == NULL) {
		AUD_LOG_W("%s(), itor_lib_hanlder->arsi_handler == NULL", __FUNCTION__);
		UNLOCK_ALOCK(lib_handler->lock);
		return;
	}

	arsi_api = lib_handler->api;

	retval = arsi_api->arsi_destroy_handler(lib_handler->arsi_handler);
	AUD_LOG_D("%s(), lib_name %s, %p, arsi_destroy_handler, arsi_handler = %p, retval = %d",
		  __FUNCTION__, lib_handler->lib_name, lib_handler, lib_handler->arsi_handler,
		  retval);
	lib_handler->arsi_handler = NULL;

#ifdef AURISYS_RESERVE_MEM
	if (free_lib_reserved_mem(lib_handler) != 0)
#endif
	{
		AUDIO_FREE_POINTER(lib_handler->working_buf.p_buffer);
	}
	if (lib_handler->lib_dump_enabled)
		AUDIO_FREE_POINTER(lib_handler->lib_dump_buf.p_buffer);

#ifdef AURISYS_C2C_SYNC_SUPPORT
	taskENTER_CRITICAL();
	if (lib_handler->arsi_process_type == ARSI_PROCESS_TYPE_UL_ONLY) {
		memset(aurisys_c2c_sync_buf.share_buf_u2d.p_buffer,
		       0, aurisys_c2c_sync_buf.share_buf_u2d.memory_size);
		memset(aurisys_c2c_sync_buf.d2u_buf.p_buffer,
		       0, aurisys_c2c_sync_buf.d2u_buf.memory_size);
	} else if (lib_handler->arsi_process_type == ARSI_PROCESS_TYPE_DL_ONLY) {
		memset(aurisys_c2c_sync_buf.share_buf_d2u.p_buffer,
		       0, aurisys_c2c_sync_buf.share_buf_d2u.memory_size);
		memset(aurisys_c2c_sync_buf.u2d_buf.p_buffer,
		       0, aurisys_c2c_sync_buf.u2d_buf.memory_size);
	}
	taskEXIT_CRITICAL();

	LOCK_ALOCK_MS(aurisys_c2c_sync_buf.sync_lock, 500);
	/* clear lib info */
	memset(&aurisys_c2c_sync_buf.lib_sync_info, 0, sizeof(struct aurisys_lib_sync_info_t));
	UNLOCK_ALOCK(aurisys_c2c_sync_buf.sync_lock);
#endif
	UNLOCK_ALOCK(lib_handler->lock);
}


int aurisys_arsi_update_param(
	aurisys_lib_handler_t *lib_handler,
	struct data_buf_t *param_buf)
{
	if (!lib_handler || !param_buf) {
		ALOGE("lib_handler %p, param_buf %p!!",
		      lib_handler, param_buf);
		return -1;
	}
	if (!param_buf->p_buffer || !param_buf->data_size || !param_buf->memory_size) {
		ALOGE("p_buffer %p, data_size %u, memory_size %u!!",
		      param_buf->p_buffer, param_buf->data_size, param_buf->memory_size);
		return -1;
	}

	dynamic_change_buf_size(&lib_handler->param_buf.p_buffer,
				&lib_handler->param_buf.memory_size,
				param_buf->data_size);

	memcpy(lib_handler->param_buf.p_buffer,
	       param_buf->p_buffer,
	       param_buf->data_size);

	lib_handler->param_buf.data_size = param_buf->data_size;

	return 0;
}


uint32_t aurisys_arsi_process_ul_only(aurisys_lib_handler_t *lib_handler)
{
	uint32_t process_count = 0;

	AurisysLibInterface *arsi_api = NULL;

	audio_pool_buf_t *pool_buf_in = NULL;
	audio_pool_buf_t *pool_buf_out = NULL;
	audio_ringbuf_t  *rb_smoother = NULL;

	uint32_t raw_in_size = 0;
	uint32_t process_size_per_time_in = 0;
	uint32_t process_size_per_time_out = 0;
	uint32_t expect_out_size = 0;
	uint32_t pad_size = 0;

	audio_ringbuf_t *rb_in = NULL;
	audio_ringbuf_t *rb_out = NULL;
	audio_ringbuf_t *rb_aec = NULL;

	audio_buf_t *p_ul_buf_in = NULL;
	audio_buf_t *p_ul_buf_out = NULL;
	audio_buf_t *p_ul_ref_bufs = NULL;
	audio_buf_t *p_ul_buf_aec = NULL;

	data_buf_t *lib_dump_buf = NULL;

	uint32_t frame_buf_size_ul_in = 0;
	uint32_t frame_buf_size_ul_out = 0;
	uint32_t frame_buf_size_ul_aec = 0;
	uint32_t max_frame_buf_size = 0;

	uint32_t pool_in_data_count = 0;
	uint64_t process_time_period_us = 0;
	uint64_t start_time_us = 0;
	uint64_t end_time_us = 0;

	status_t retval = NO_ERROR;

	audio_buf_t fake_aec_buf;

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	arsi_api = lib_handler->api;

	rb_in = &lib_handler->ul_pool_in.ringbuf;
	rb_out = &lib_handler->ul_pool_out.ringbuf;
	AUD_LOG_V("%s(+), rb_in  data_count %u, free_count %u", __FUNCTION__,
		  audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
	AUD_LOG_VV("%s(+), rb_out data_count %u, free_count %u", __FUNCTION__,
		   audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));

	if (lib_handler->aec_pool_in != NULL) {
		rb_aec = &lib_handler->aec_pool_in->ringbuf;
		AUD_LOG_V("%s(+), rb_aec data_count %u, free_count %u", __FUNCTION__,
			  audio_ringbuf_count(rb_aec), audio_ringbuf_free_space(rb_aec));
	}

	pool_buf_in = &lib_handler->ul_pool_in;
	pool_buf_out = &lib_handler->ul_pool_out;
	rb_smoother = &lib_handler->ul_smoother;

	raw_in_size = audio_ringbuf_count(&pool_buf_in->ringbuf) -
		      lib_handler->ul_left_raw_in_size;
	process_size_per_time_in = lib_handler->ul_process_size_per_time;
	expect_out_size = (raw_in_size * pool_buf_out->buf->num_channels) /
			  pool_buf_in->buf->num_channels;
	dynamic_change_pool_buf_size(pool_buf_out, expect_out_size);

	if ((lib_handler->lib_config.p_ul_buf_in == NULL) ||
	    (lib_handler->lib_config.p_ul_buf_out == NULL)) {
		AUD_ASSERT(lib_handler->lib_config.p_ul_buf_in != NULL);
		AUD_ASSERT(lib_handler->lib_config.p_ul_buf_out != NULL);
		UNLOCK_ALOCK(lib_handler->lock);
		return 0;
	}

	p_ul_buf_in   = lib_handler->lib_config.p_ul_buf_in;
	p_ul_buf_out  = lib_handler->lib_config.p_ul_buf_out;
	p_ul_ref_bufs = lib_handler->lib_config.p_ul_ref_bufs;
	p_ul_buf_aec  = lib_handler->p_ul_ref_buf_aec;

	/* fake AEC for VoIP w/o AEC */
	if (lib_handler->task_config->task_scene == TASK_SCENE_VOIP &&
	    p_ul_ref_bufs == NULL) {
		p_ul_ref_bufs = &fake_aec_buf;
		p_ul_ref_bufs->data_buf.memory_size = 640;
		p_ul_ref_bufs->data_buf.data_size = 640;
		p_ul_ref_bufs->data_buf.p_buffer = fake_aec_mem;

		p_ul_ref_bufs->data_buf_type = DATA_BUF_ECHO_REF;
		p_ul_ref_bufs->num_channels = 1;
		p_ul_ref_bufs->channel_mask = AUDIO_CHANNEL_IN_MONO;
		p_ul_ref_bufs->sample_rate_buffer = 16000;
		p_ul_ref_bufs->sample_rate_content = 16000;
		p_ul_ref_bufs->audio_format = AUDIO_FORMAT_PCM_16_BIT;
	}

	if (lib_handler->lib_dump_enabled &&
	    lib_handler->lib_dump_buf.p_buffer != NULL)
		lib_dump_buf = &lib_handler->lib_dump_buf;

#ifdef AURISYS_C2C_SYNC_SUPPORT
	if (lib_handler->syc_buf_d2u.p_buffer != NULL &&
	    lib_handler->syc_buf_d2u.data_size <= lib_handler->syc_buf_d2u.memory_size) {
		//taskENTER_CRITICAL();
		memcpy(lib_handler->syc_buf_d2u.p_buffer,
		       aurisys_c2c_sync_buf.share_buf_d2u.p_buffer,
		       lib_handler->syc_buf_d2u.data_size);
		//taskEXIT_CRITICAL();
	}
#endif

	AUDIO_CHECK(lib_handler);
	AUDIO_CHECK(rb_in->base);
	AUDIO_CHECK(rb_out->base);
	AUDIO_CHECK(p_ul_buf_in->data_buf.p_buffer);
	AUDIO_CHECK(p_ul_buf_out->data_buf.p_buffer);
	AUDIO_CHECK(lib_handler->linear_buf_ul);
	AUDIO_CHECK(lib_handler->linear_buf_ul_aec);


	if (lib_handler->lib_config.frame_size_ms != 0) { /* frame base */
		frame_buf_size_ul_in = get_frame_buf_size(p_ul_buf_in);
		frame_buf_size_ul_out = get_frame_buf_size(p_ul_buf_out);
		process_time_period_us = get_expect_process_time_us(p_ul_buf_in, frame_buf_size_ul_in);

		if ((p_ul_buf_in->data_buf.p_buffer == NULL) ||
		    (p_ul_buf_out->data_buf.p_buffer == NULL)) {
			AUD_ASSERT(p_ul_buf_in->data_buf.p_buffer != NULL);
			AUD_ASSERT(p_ul_buf_out->data_buf.p_buffer != NULL);
			UNLOCK_ALOCK(lib_handler->lock);
			return 0;
		}

		if (p_ul_buf_aec != NULL) {
			frame_buf_size_ul_aec = get_frame_buf_size(p_ul_buf_aec);
			AUD_ASSERT(p_ul_buf_aec->data_buf.memory_size == frame_buf_size_ul_aec);
			if (rb_aec != NULL)
				AUD_ASSERT(audio_ringbuf_count(rb_aec) >= frame_buf_size_ul_aec);
		}

		/* temp buf for interleave <-> frame base */
		if (lib_handler->linear_buf_ul == NULL) {
			max_frame_buf_size = (frame_buf_size_ul_in > frame_buf_size_ul_out)
					     ? frame_buf_size_ul_in
					     : frame_buf_size_ul_out;
			if (max_frame_buf_size < frame_buf_size_ul_aec)
				max_frame_buf_size = frame_buf_size_ul_aec;
			AUDIO_ALLOC_BUFFER(lib_handler->linear_buf_ul, max_frame_buf_size);
			if (lib_handler->linear_buf_ul == NULL) {
				UNLOCK_ALOCK(lib_handler->lock);
				return 0;
			}
		}
		if (lib_handler->linear_buf_ul_aec == NULL) {
			AUDIO_ALLOC_BUFFER(lib_handler->linear_buf_ul_aec, frame_buf_size_ul_aec);
			if (lib_handler->linear_buf_ul_aec == NULL) {
				UNLOCK_ALOCK(lib_handler->lock);
				return 0;
			}
		}

		dynamic_change_ring_buf_size(rb_smoother,
					     expect_out_size + frame_buf_size_ul_out);

		pool_in_data_count = audio_ringbuf_count(rb_in);
		while (pool_in_data_count >= frame_buf_size_ul_in) { /* once per frame */
			if (rb_aec != NULL) { /* once per frame */
				if (audio_ringbuf_count(rb_aec) < frame_buf_size_ul_aec)
					break;
			}

			pool_in_data_count -= frame_buf_size_ul_in;

			if (lib_handler->task_config->task_scene == TASK_SCENE_PHONE_CALL)
				audio_ringbuf_copy_to_linear(p_ul_buf_in->data_buf.p_buffer, rb_in,
							     frame_buf_size_ul_in);

			else {
				/* get data from pool */
				audio_ringbuf_copy_to_linear(lib_handler->linear_buf_ul, rb_in,
							     frame_buf_size_ul_in);
				/* frame base transfer */
				linear_to_frame_base(
					lib_handler->linear_buf_ul,
					p_ul_buf_in->data_buf.p_buffer,
					frame_buf_size_ul_in,
					p_ul_buf_in->audio_format,
					p_ul_buf_in->num_channels);
			}
			p_ul_buf_in->data_buf.data_size = frame_buf_size_ul_in;

			if (rb_aec != NULL && p_ul_buf_aec != NULL) {
				if (p_ul_buf_aec->num_channels == 1)
					audio_ringbuf_copy_to_linear(p_ul_buf_aec->data_buf.p_buffer, rb_aec,
								     frame_buf_size_ul_aec);

				else {
					if (lib_handler->task_config->task_scene == TASK_SCENE_PHONE_CALL)
						audio_ringbuf_copy_to_linear(p_ul_buf_aec->data_buf.p_buffer, rb_aec,
									     frame_buf_size_ul_aec);

					else {
						audio_ringbuf_copy_to_linear(lib_handler->linear_buf_ul_aec, rb_aec,
									     frame_buf_size_ul_aec);
						linear_to_frame_base(
							lib_handler->linear_buf_ul_aec,
							p_ul_buf_aec->data_buf.p_buffer,
							frame_buf_size_ul_aec,
							p_ul_buf_aec->audio_format,
							p_ul_buf_aec->num_channels);
					}
				}
				p_ul_buf_aec->data_buf.data_size = frame_buf_size_ul_aec;
			}


			/* process */
			AUD_LOG_VV("%s(+), lib in %u, out %u", __FUNCTION__,
				   p_ul_buf_in->data_buf.data_size,
				   p_ul_buf_out->data_buf.data_size);
			AUD_ASSERT(p_ul_buf_out->data_buf.data_size == 0);
#if 0
			memcpy(p_ul_buf_out->data_buf.p_buffer, p_ul_buf_in->data_buf.p_buffer,
			       frame_buf_size_ul_out);
			p_ul_buf_out->data_buf.data_size = frame_buf_size_ul_out;
			p_ul_buf_in->data_buf.data_size = 0;
#else
			start_time_us = get_current_time_us();
			retval = arsi_api->arsi_process_ul_buf(
					 p_ul_buf_in,
					 p_ul_buf_out,
					 p_ul_ref_bufs,
					 lib_dump_buf,
					 lib_handler->arsi_handler);
			end_time_us = get_current_time_us();
			if ((end_time_us - start_time_us) > process_time_period_us)
				AUD_LOG_W("%s(), lib: %s, UL process time %0.3f exceed eximate time %0.3f\n!",
					  __FUNCTION__, lib_handler->lib_name,
					  (float)(end_time_us - start_time_us) / 1000,
					  (float)process_time_period_us / 1000);
#endif
			if (retval != NO_ERROR) {
				AUD_LOG_W("lib_handler %p, arsi_handler %p, arsi_process_ul_buf retval = %d",
					  lib_handler, lib_handler->arsi_handler, retval);
			}
			AUD_LOG_VV("%s(-), lib in %u, out %u", __FUNCTION__,
				   p_ul_buf_in->data_buf.data_size,
				   p_ul_buf_out->data_buf.data_size);

			AUD_ASSERT(p_ul_buf_in->data_buf.data_size == 0);
			if (p_ul_buf_out->data_buf.data_size != frame_buf_size_ul_out) {
				AUD_LOG_E("%s(), p_ul_buf_out->data_buf.data_size %u != %u",
					  __FUNCTION__, p_ul_buf_out->data_buf.data_size, frame_buf_size_ul_out);
				AUD_ASSERT(p_ul_buf_out->data_buf.data_size == frame_buf_size_ul_out);
			}

			if ((lib_handler->lib_dump_enabled) && (lib_dump_buf != NULL) &&
			    (lib_dump_buf->data_size > 0)) {
				struct ipi_msg_t ipi_msg;
				audio_send_ipi_msg(&ipi_msg,
						   LOCAL_TASK_SCENE,
						   AUDIO_IPI_LAYER_TO_HAL,
						   AUDIO_IPI_DMA,
						   AUDIO_IPI_MSG_BYPASS_ACK,
						   AUD_CTL_MSG_D2A_AURISYS_DUMP,
						   lib_dump_buf->data_size,
						   ((lib_handler->dsp_task_scene << 16) + lib_handler->lib_order),
						   lib_dump_buf->p_buffer);
			}

			if (lib_handler->task_config->task_scene == TASK_SCENE_PHONE_CALL) {
				audio_ringbuf_copy_from_linear_impl(
					rb_smoother,
					p_ul_buf_out->data_buf.p_buffer,
					frame_buf_size_ul_out);
				p_ul_buf_out->data_buf.data_size = 0;
			} else {
				/* linear base transfer */
				frame_base_to_linear(
					p_ul_buf_out->data_buf.p_buffer,
					lib_handler->linear_buf_ul,
					p_ul_buf_out->data_buf.data_size,
					p_ul_buf_out->audio_format,
					p_ul_buf_out->num_channels);
				p_ul_buf_out->data_buf.data_size = 0;

				audio_ringbuf_copy_from_linear_impl(
					rb_smoother,
					lib_handler->linear_buf_ul,
					frame_buf_size_ul_out);
			}
			process_count += frame_buf_size_ul_out;
		}
	} else {
		dynamic_change_ring_buf_size(rb_smoother,
					     expect_out_size + process_size_per_time_in);

		pool_in_data_count = audio_ringbuf_count(rb_in);
		if (process_size_per_time_in == 0)
			process_size_per_time_in = pool_in_data_count;
		process_size_per_time_out =
			(process_size_per_time_in * pool_buf_out->buf->num_channels) /
			pool_buf_in->buf->num_channels;
		process_time_period_us = get_expect_process_time_us(p_ul_buf_in, process_size_per_time_in);
		dynamic_change_buf_size(&p_ul_buf_in->data_buf.p_buffer,
					&p_ul_buf_in->data_buf.memory_size,
					process_size_per_time_in);
		dynamic_change_buf_size(&p_ul_buf_out->data_buf.p_buffer,
					&p_ul_buf_out->data_buf.memory_size,
					process_size_per_time_out);
		while (pool_in_data_count >= process_size_per_time_in) {
			/* get data from pool to lib in */
			audio_ringbuf_copy_to_linear(p_ul_buf_in->data_buf.p_buffer, rb_in,
						     process_size_per_time_in);
			pool_in_data_count -= process_size_per_time_in;
			p_ul_buf_in->data_buf.data_size = process_size_per_time_in;


			/* process */
			AUD_LOG_VV("%s(+), lib in %u, out %u", __FUNCTION__,
				   p_ul_buf_in->data_buf.data_size,
				   p_ul_buf_out->data_buf.data_size);
			AUD_ASSERT(p_ul_buf_out->data_buf.data_size == 0);
#if 0
			memcpy(p_ul_buf_out->data_buf.p_buffer, p_ul_buf_in->data_buf.p_buffer,
			       process_size_per_time_out);
			p_ul_buf_out->data_buf.data_size = process_size_per_time_out;
			p_ul_buf_in->data_buf.data_size = 0;
#else
			start_time_us = get_current_time_us();
			retval = arsi_api->arsi_process_ul_buf(
					 p_ul_buf_in,
					 p_ul_buf_out,
					 p_ul_ref_bufs,
					 lib_dump_buf,
					 lib_handler->arsi_handler);
			end_time_us = get_current_time_us();
			if ((end_time_us - start_time_us) > process_time_period_us)
				AUD_LOG_W("%s(), lib: %s, UL process time %0.3f exceed eximate time %0.3f\n!",
					  __FUNCTION__, lib_handler->lib_name,
					  (float)(end_time_us - start_time_us) / 1000,
					  (float)process_time_period_us / 1000);
#endif
			if (retval != NO_ERROR) {
				AUD_LOG_W("lib_handler %p, arsi_handler %p, arsi_process_ul_buf retval = %d",
					  lib_handler, lib_handler->arsi_handler, retval);
			}
			AUD_LOG_VV("%s(-), lib in %u, out %u", __FUNCTION__,
				   p_ul_buf_in->data_buf.data_size,
				   p_ul_buf_out->data_buf.data_size);

			AUD_ASSERT(p_ul_buf_in->data_buf.data_size == 0);
			AUD_ASSERT(p_ul_buf_out->data_buf.data_size != 0);

			if ((lib_handler->lib_dump_enabled) && (lib_dump_buf != NULL) &&
			    (lib_dump_buf->data_size > 0)) {
				struct ipi_msg_t ipi_msg;
				audio_send_ipi_msg(&ipi_msg,
						   LOCAL_TASK_SCENE,
						   AUDIO_IPI_LAYER_TO_HAL,
						   AUDIO_IPI_DMA,
						   AUDIO_IPI_MSG_BYPASS_ACK,
						   AUD_CTL_MSG_D2A_AURISYS_DUMP,
						   lib_dump_buf->data_size,
						   ((lib_handler->dsp_task_scene << 16) + lib_handler->lib_order),
						   lib_dump_buf->p_buffer);
			}

			audio_ringbuf_copy_from_linear_impl(
				rb_smoother,
				p_ul_buf_out->data_buf.p_buffer,
				p_ul_buf_out->data_buf.data_size);
			process_count += p_ul_buf_out->data_buf.data_size;
			p_ul_buf_out->data_buf.data_size = 0;
		}
	}

	if (lib_handler->pad_flag == false && process_count < expect_out_size) {
		if (lib_handler->lib_config.frame_size_ms != 0)   /* frame base */
			pad_size = frame_buf_size_ul_out;
		else {
			AUD_ASSERT(lib_handler->ul_process_size_per_time != 0);
			pad_size = process_size_per_time_in;
		}
		audio_ringbuf_compensate_value_impl(rb_smoother, 0, pad_size);
	}
	lib_handler->pad_flag = true;

	audio_ringbuf_copy_from_ringbuf_impl(rb_out, rb_smoother, expect_out_size);
	lib_handler->ul_left_raw_in_size = audio_ringbuf_count(&pool_buf_in->ringbuf);

#ifdef AURISYS_C2C_SYNC_SUPPORT
	if ((lib_handler->syc_buf_u2d.p_buffer != NULL) &&
	    (lib_handler->syc_buf_u2d.data_size <=
	     aurisys_c2c_sync_buf.share_buf_u2d.memory_size)) {
		struct adsp_c2c_data_t c2c_data;

		memcpy(aurisys_c2c_sync_buf.share_buf_u2d.p_buffer,
		       lib_handler->syc_buf_u2d.p_buffer,
		       lib_handler->syc_buf_u2d.data_size);
		c2c_data.size = lib_handler->syc_buf_u2d.data_size;
		c2c_data.addr = aurisys_c2c_sync_buf.share_buf_u2d.p_buffer;

		adsp_c2c_ipc_send(
			ADSP_C2C_AURISYS_SYNC_ID,
			ADSP_C2C_MSG_NEED_ACK,
			0,
			lib_handler->task_config->task_scene,
			0,
			lib_handler->lib_id,
			&c2c_data,
			sizeof(c2c_data));
		/* memcpy(aurisys_c2c_sync_buf.d2u_buf.p_buffer,
		       aurisys_c2c_sync_buf.share_buf_d2u.p_buffer,
		       aurisys_c2c_sync_buf.share_buf_d2u.memory_size); */
	}
#endif

	AUDIO_CHECK(lib_handler);
	AUDIO_CHECK(rb_in->base);
	AUDIO_CHECK(rb_out->base);
	AUDIO_CHECK(p_ul_buf_in->data_buf.p_buffer);
	AUDIO_CHECK(p_ul_buf_out->data_buf.p_buffer);
	AUDIO_CHECK(lib_handler->linear_buf_ul);
	AUDIO_CHECK(lib_handler->linear_buf_ul_aec);

	AUD_LOG_VV("%s(-), rb_in  data_count %u, free_count %u", __FUNCTION__,
		   audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
	AUD_LOG_V("%s(-), rb_out data_count %u, free_count %u", __FUNCTION__,
		  audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));
	if (rb_aec != NULL) {
		AUD_LOG_V("%s(-), rb_aec data_count %u, free_count %u", __FUNCTION__,
			  audio_ringbuf_count(rb_aec), audio_ringbuf_free_space(rb_aec));
	}
	UNLOCK_ALOCK(lib_handler->lock);
	return expect_out_size;
}


uint32_t aurisys_arsi_process_dl_only(aurisys_lib_handler_t *lib_handler)
{
	uint32_t process_count = 0;

	AurisysLibInterface *arsi_api = NULL;

	audio_pool_buf_t *pool_buf_in = NULL;
	audio_pool_buf_t *pool_buf_out = NULL;
	audio_ringbuf_t  *rb_smoother = NULL;

	uint32_t raw_in_size = 0;
	uint32_t process_size_per_time_in = 0;
	uint32_t process_size_per_time_out = 0;
	uint32_t expect_out_size = 0;
	uint32_t pad_size = 0;

	audio_ringbuf_t *rb_in = NULL;
	audio_ringbuf_t *rb_out = NULL;
	audio_ringbuf_t *rb_iv = NULL;

	audio_buf_t *p_dl_buf_in = NULL;
	audio_buf_t *p_dl_buf_out = NULL;
	audio_buf_t *p_dl_ref_bufs = NULL;
	audio_buf_t *p_dl_ref_iv = NULL;

	data_buf_t *lib_dump_buf = NULL;

	uint32_t frame_buf_size_dl_in = 0;
	uint32_t frame_buf_size_dl_out = 0;
	uint32_t max_frame_buf_size = 0;

	uint32_t pool_in_data_count = 0;
	uint64_t process_time_period_us = 0;
	uint64_t start_time_us = 0;
	uint64_t end_time_us = 0;

	status_t retval = NO_ERROR;


	LOCK_ALOCK_MS(lib_handler->lock, 500);

	arsi_api = lib_handler->api;

	rb_in = &lib_handler->dl_pool_in.ringbuf;
	rb_out = &lib_handler->dl_pool_out.ringbuf;

	if (lib_handler->iv_pool_in != NULL)
		rb_iv = &lib_handler->iv_pool_in->ringbuf;

	AUD_LOG_V("%s(+), rb_in  data_count %u, free_count %u", __FUNCTION__,
		  audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
	AUD_LOG_VV("%s(+), rb_out data_count %u, free_count %u", __FUNCTION__,
		   audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));

	pool_buf_in = &lib_handler->dl_pool_in;
	pool_buf_out = &lib_handler->dl_pool_out;
	rb_smoother = &lib_handler->dl_smoother;

	raw_in_size = audio_ringbuf_count(&pool_buf_in->ringbuf) -
		      lib_handler->dl_left_raw_in_size;
	process_size_per_time_in = lib_handler->dl_process_size_per_time;
	expect_out_size = (raw_in_size * pool_buf_out->buf->num_channels) /
			  pool_buf_in->buf->num_channels;
	dynamic_change_pool_buf_size(pool_buf_out, expect_out_size);

	if ((lib_handler->lib_config.p_dl_buf_in == NULL) ||
	    (lib_handler->lib_config.p_dl_buf_out == NULL)) {
		AUD_ASSERT(lib_handler->lib_config.p_dl_buf_in != NULL);
		AUD_ASSERT(lib_handler->lib_config.p_dl_buf_out != NULL);
		UNLOCK_ALOCK(lib_handler->lock);
		return 0;
	}

	p_dl_buf_in  = lib_handler->lib_config.p_dl_buf_in;
	p_dl_buf_out = lib_handler->lib_config.p_dl_buf_out;
	p_dl_ref_bufs = lib_handler->lib_config.p_dl_ref_bufs;
	p_dl_ref_iv = lib_handler->p_dl_ref_buf_iv;

	if (lib_handler->lib_dump_enabled &&
	    lib_handler->lib_dump_buf.p_buffer != NULL)
		lib_dump_buf = &lib_handler->lib_dump_buf;

#ifdef AURISYS_C2C_SYNC_SUPPORT
	taskENTER_CRITICAL();
	if ((lib_handler->syc_buf_u2d.p_buffer != NULL) &&
	    (aurisys_c2c_sync_buf.updated == true) &&
	    (lib_handler->syc_buf_u2d.data_size <=
	     lib_handler->syc_buf_u2d.memory_size)) {
		memcpy(lib_handler->syc_buf_u2d.p_buffer,
		       aurisys_c2c_sync_buf.u2d_buf.p_buffer,
		       lib_handler->syc_buf_u2d.data_size);
		aurisys_c2c_sync_buf.updated = false;
	}
	taskEXIT_CRITICAL();
#endif

	AUDIO_CHECK(lib_handler);
	AUDIO_CHECK(rb_in->base);
	AUDIO_CHECK(rb_out->base);
	AUDIO_CHECK(p_dl_buf_in->data_buf.p_buffer);
	AUDIO_CHECK(p_dl_buf_out->data_buf.p_buffer);
	AUDIO_CHECK(lib_handler->linear_buf_dl);


	if (lib_handler->lib_config.frame_size_ms != 0) { /* frame base */
		frame_buf_size_dl_in = get_frame_buf_size(p_dl_buf_in);
		frame_buf_size_dl_out = get_frame_buf_size(p_dl_buf_out);
		process_time_period_us = get_expect_process_time_us(p_dl_buf_in, frame_buf_size_dl_in);

		if ((p_dl_buf_in->data_buf.p_buffer == NULL) ||
		    (p_dl_buf_out->data_buf.p_buffer == NULL)) {
			AUD_ASSERT(p_dl_buf_in->data_buf.p_buffer != NULL);
			AUD_ASSERT(p_dl_buf_out->data_buf.p_buffer != NULL);
			UNLOCK_ALOCK(lib_handler->lock);
			return 0;
		}

		AUD_ASSERT(p_dl_buf_in->data_buf.memory_size == frame_buf_size_dl_in);
		AUD_ASSERT(p_dl_buf_out->data_buf.memory_size == frame_buf_size_dl_out);

		/* temp buf for interleave <-> frame base */
		if (lib_handler->linear_buf_dl == NULL) {
			max_frame_buf_size = (frame_buf_size_dl_in > frame_buf_size_dl_out)
					     ? frame_buf_size_dl_in
					     : frame_buf_size_dl_out;
			AUDIO_ALLOC_BUFFER(lib_handler->linear_buf_dl, max_frame_buf_size);
			if (lib_handler->linear_buf_dl == NULL) {
				UNLOCK_ALOCK(lib_handler->lock);
				return 0;
			}
		}

		dynamic_change_ring_buf_size(rb_smoother,
					     expect_out_size + frame_buf_size_dl_out);

		pool_in_data_count = audio_ringbuf_count(rb_in);
		while (pool_in_data_count >= frame_buf_size_dl_in) { /* once per frame */
			pool_in_data_count -= frame_buf_size_dl_in;

			if (lib_handler->task_config->task_scene == TASK_SCENE_PHONE_CALL)
				audio_ringbuf_copy_to_linear(p_dl_buf_in->data_buf.p_buffer, rb_in,
							     frame_buf_size_dl_in);

			else {
				/* get data from pool */
				audio_ringbuf_copy_to_linear(lib_handler->linear_buf_dl, rb_in,
							     frame_buf_size_dl_in);

				/* frame base transfer */
				linear_to_frame_base(
					lib_handler->linear_buf_dl,
					p_dl_buf_in->data_buf.p_buffer,
					frame_buf_size_dl_in,
					p_dl_buf_in->audio_format,
					p_dl_buf_in->num_channels);
			}
			p_dl_buf_in->data_buf.data_size = frame_buf_size_dl_in;
			/* get iv data form pool to lib in */
			if (p_dl_ref_bufs != NULL && p_dl_ref_iv != NULL) {
				audio_ringbuf_copy_to_linear(p_dl_ref_iv->data_buf.p_buffer, rb_iv,
							     frame_buf_size_dl_in);
				p_dl_ref_iv->data_buf.data_size = frame_buf_size_dl_in;
			}

			/* process */
			AUD_LOG_VV("%s(+), lib in %u, out %u", __FUNCTION__,
				   p_dl_buf_in->data_buf.data_size,
				   p_dl_buf_out->data_buf.data_size);
			AUD_ASSERT(p_dl_buf_out->data_buf.data_size == 0);
#if 0
			memcpy(p_dl_buf_out->data_buf.p_buffer, p_dl_buf_in->data_buf.p_buffer,
			       frame_buf_size_dl_out);
			p_dl_buf_out->data_buf.data_size = frame_buf_size_dl_out;
			p_dl_buf_in->data_buf.data_size = 0;
#else
			start_time_us = get_current_time_us();
			retval = arsi_api->arsi_process_dl_buf(
					 p_dl_buf_in,
					 p_dl_buf_out,
					 p_dl_ref_bufs,
					 lib_dump_buf,
					 lib_handler->arsi_handler);
			end_time_us = get_current_time_us();
			if ((end_time_us - start_time_us) > process_time_period_us)
				AUD_LOG_W("%s(), lib: %s, DL process time %0.3f exceed eximate time %0.3f\n!",
					  __FUNCTION__, lib_handler->lib_name,
					  (float)(end_time_us - start_time_us) / 1000,
					  (float)process_time_period_us / 1000);
#endif
			if (retval != NO_ERROR) {
				AUD_LOG_W("lib_handler %p, arsi_handler %p, arsi_process_dl_buf retval = %d",
					  lib_handler, lib_handler->arsi_handler, retval);
			}
			AUD_LOG_VV("%s(-), lib in %u, out %u", __FUNCTION__,
				   p_dl_buf_in->data_buf.data_size,
				   p_dl_buf_out->data_buf.data_size);

			AUD_ASSERT(p_dl_buf_in->data_buf.data_size == 0);
			if (p_dl_buf_out->data_buf.data_size != frame_buf_size_dl_out) {
				AUD_LOG_E("%s(), p_dl_buf_out->data_buf.data_size %u != %u",
					  __FUNCTION__, p_dl_buf_out->data_buf.data_size, frame_buf_size_dl_out);
				AUD_ASSERT(p_dl_buf_out->data_buf.data_size == frame_buf_size_dl_out);
			}

			if ((lib_handler->lib_dump_enabled) && (lib_dump_buf != NULL) &&
			    (lib_dump_buf->data_size > 0)) {
				struct ipi_msg_t ipi_msg;
				audio_send_ipi_msg(&ipi_msg,
						   LOCAL_TASK_SCENE,
						   AUDIO_IPI_LAYER_TO_HAL,
						   AUDIO_IPI_DMA,
						   AUDIO_IPI_MSG_BYPASS_ACK,
						   AUD_CTL_MSG_D2A_AURISYS_DUMP,
						   lib_dump_buf->data_size,
						   ((lib_handler->dsp_task_scene << 16) + lib_handler->lib_order),
						   lib_dump_buf->p_buffer);
			}

			if (lib_handler->task_config->task_scene == TASK_SCENE_PHONE_CALL) {
				audio_ringbuf_copy_from_linear_impl(
					rb_smoother,
					p_dl_buf_out->data_buf.p_buffer,
					frame_buf_size_dl_out);
				p_dl_buf_out->data_buf.data_size = 0;
			} else {
				/* linear base transfer */
				frame_base_to_linear(
					p_dl_buf_out->data_buf.p_buffer,
					lib_handler->linear_buf_dl,
					p_dl_buf_out->data_buf.data_size,
					p_dl_buf_out->audio_format,
					p_dl_buf_out->num_channels);
				p_dl_buf_out->data_buf.data_size = 0;

				audio_ringbuf_copy_from_linear_impl(
					rb_smoother,
					lib_handler->linear_buf_dl,
					frame_buf_size_dl_out);
			}
			process_count += frame_buf_size_dl_out;
		}
	} else {
		dynamic_change_ring_buf_size(rb_smoother,
					     expect_out_size + process_size_per_time_in);

		pool_in_data_count = audio_ringbuf_count(rb_in);
		if (process_size_per_time_in == 0)
			process_size_per_time_in = pool_in_data_count;
		process_size_per_time_out =
			(process_size_per_time_in * pool_buf_out->buf->num_channels) /
			pool_buf_in->buf->num_channels;
		process_time_period_us = get_expect_process_time_us(p_dl_buf_in, process_size_per_time_in);
		dynamic_change_buf_size(&p_dl_buf_in->data_buf.p_buffer,
					&p_dl_buf_in->data_buf.memory_size,
					process_size_per_time_in);
		dynamic_change_buf_size(&p_dl_buf_out->data_buf.p_buffer,
					&p_dl_buf_out->data_buf.memory_size,
					process_size_per_time_out);
		if (p_dl_ref_bufs != NULL && p_dl_ref_iv != NULL) {
			dynamic_change_buf_size(&p_dl_ref_iv->data_buf.p_buffer,
						&p_dl_ref_iv->data_buf.memory_size,
						process_size_per_time_in);
		}
		while (pool_in_data_count >= process_size_per_time_in) {
			/* get data from pool to lib in */
			audio_ringbuf_copy_to_linear(p_dl_buf_in->data_buf.p_buffer, rb_in,
						     process_size_per_time_in);
			pool_in_data_count -= process_size_per_time_in;
			p_dl_buf_in->data_buf.data_size = process_size_per_time_in;

			/* get iv data form pool to lib in */
			if (p_dl_ref_bufs != NULL && p_dl_ref_iv != NULL) {
				audio_ringbuf_copy_to_linear(p_dl_ref_iv->data_buf.p_buffer, rb_iv,
							     process_size_per_time_in);
				p_dl_ref_iv->data_buf.data_size = process_size_per_time_in;
			}
			/* process */
			AUD_LOG_VV("%s(+), lib in %u, out %u", __FUNCTION__,
				   p_dl_buf_in->data_buf.data_size,
				   p_dl_buf_out->data_buf.data_size);
			AUD_ASSERT(p_dl_buf_out->data_buf.data_size == 0);
#if 0
			memcpy(p_dl_buf_out->data_buf.p_buffer, p_dl_buf_in->data_buf.p_buffer,
			       process_size_per_time_out);
			p_dl_buf_out->data_buf.data_size = process_size_per_time_out;
			p_dl_buf_in->data_buf.data_size = 0;
#else
			start_time_us = get_current_time_us();
			retval = arsi_api->arsi_process_dl_buf(
					 p_dl_buf_in,
					 p_dl_buf_out,
					 p_dl_ref_bufs,
					 lib_dump_buf,
					 lib_handler->arsi_handler);
			end_time_us = get_current_time_us();
			if ((end_time_us - start_time_us) > process_time_period_us)
				AUD_LOG_W("%s(), lib: %s, DL process time %0.3f exceed eximate time %0.3f\n!",
					  __FUNCTION__, lib_handler->lib_name,
					  (float)(end_time_us - start_time_us) / 1000,
					  (float)process_time_period_us / 1000);
#endif
			if (retval != NO_ERROR) {
				AUD_LOG_W("lib_handler %p, arsi_handler %p, arsi_process_dl_buf retval = %d",
					  lib_handler, lib_handler->arsi_handler, retval);
			}
			AUD_LOG_VV("%s(-), lib in %u, out %u", __FUNCTION__,
				   p_dl_buf_in->data_buf.data_size,
				   p_dl_buf_out->data_buf.data_size);

			AUD_ASSERT(p_dl_buf_in->data_buf.data_size == 0);
			AUD_ASSERT(p_dl_buf_out->data_buf.data_size != 0);

			if ((lib_handler->lib_dump_enabled) && (lib_dump_buf != NULL) &&
			    (lib_dump_buf->data_size > 0)) {
				struct ipi_msg_t ipi_msg;
				audio_send_ipi_msg(&ipi_msg,
						   LOCAL_TASK_SCENE,
						   AUDIO_IPI_LAYER_TO_HAL,
						   AUDIO_IPI_DMA,
						   AUDIO_IPI_MSG_BYPASS_ACK,
						   AUD_CTL_MSG_D2A_AURISYS_DUMP,
						   lib_dump_buf->data_size,
						   ((lib_handler->dsp_task_scene << 16) + lib_handler->lib_order),
						   lib_dump_buf->p_buffer);
			}

			audio_ringbuf_copy_from_linear_impl(
				rb_smoother,
				p_dl_buf_out->data_buf.p_buffer,
				p_dl_buf_out->data_buf.data_size);
			process_count += p_dl_buf_out->data_buf.data_size;
			p_dl_buf_out->data_buf.data_size = 0;
		}
	}

	if (lib_handler->pad_flag == false && process_count < expect_out_size) {
		if (lib_handler->lib_config.frame_size_ms != 0)   /* frame base */
			pad_size = frame_buf_size_dl_out;
		else {
			AUD_ASSERT(lib_handler->dl_process_size_per_time != 0);
			pad_size = process_size_per_time_in;
		}
		audio_ringbuf_compensate_value_impl(rb_smoother, 0, pad_size);
	}
	lib_handler->pad_flag = true;

	audio_ringbuf_copy_from_ringbuf_impl(rb_out, rb_smoother, expect_out_size);
	lib_handler->dl_left_raw_in_size = audio_ringbuf_count(&pool_buf_in->ringbuf);

#ifdef AURISYS_C2C_SYNC_SUPPORT
	if ((lib_handler->syc_buf_d2u.p_buffer != NULL) &&
	    (lib_handler->syc_buf_d2u.data_size <=
	     aurisys_c2c_sync_buf.d2u_buf.memory_size)) {
		taskENTER_CRITICAL();
		memcpy(aurisys_c2c_sync_buf.d2u_buf.p_buffer,
		       lib_handler->syc_buf_d2u.p_buffer,
		       lib_handler->syc_buf_d2u.data_size);
		taskEXIT_CRITICAL();
	}
	AUDIO_CHECK(aurisys_c2c_sync_buf.d2u_buf.p_buffer);
#endif

	AUDIO_CHECK(lib_handler);
	AUDIO_CHECK(rb_in->base);
	AUDIO_CHECK(rb_out->base);
	AUDIO_CHECK(p_dl_buf_in->data_buf.p_buffer);
	AUDIO_CHECK(p_dl_buf_out->data_buf.p_buffer);
	AUDIO_CHECK(lib_handler->linear_buf_dl);


	AUD_LOG_VV("%s(-), rb_in  data_count %u, free_count %u", __FUNCTION__,
		   audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
	AUD_LOG_V("%s(-), rb_out data_count %u, free_count %u", __FUNCTION__,
		  audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));
	UNLOCK_ALOCK(lib_handler->lock);
	return expect_out_size;
}


uint32_t aurisys_arsi_process_ul_and_dl(aurisys_lib_handler_t *lib_handler)
{
	if (!lib_handler) {
		AUD_LOG_E("%s(), NULL! return", __FUNCTION__);
		return -1;
	}

	return 0;
}


int aurisys_arsi_set_ul_digital_gain(
	aurisys_lib_handler_t *lib_handler,
	const int16_t ul_analog_gain_ref_only,
	const int16_t ul_digital_gain)
{
	status_t retval = NO_ERROR;

	if (lib_handler == NULL)
		return -1;
	if (lib_handler->api == NULL)
		return -1;
	if (lib_handler->api->arsi_set_ul_digital_gain == NULL)
		return -1;

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	retval = lib_handler->api->arsi_set_ul_digital_gain(
			 ul_analog_gain_ref_only,
			 ul_digital_gain,
			 lib_handler->arsi_handler);

	AUD_LOG_D("lib_name %s, %p, set ul_analog_gain_ref_only %d, ul_digital_gain %d, retval %d",
		  lib_handler->lib_name, lib_handler, ul_analog_gain_ref_only, ul_digital_gain,
		  retval);

	UNLOCK_ALOCK(lib_handler->lock);
	return (retval == NO_ERROR) ? 0 : -1;
}


int aurisys_arsi_set_dl_digital_gain(
	aurisys_lib_handler_t *lib_handler,
	const int16_t dl_analog_gain_ref_only,
	const int16_t dl_digital_gain)
{
	status_t retval = NO_ERROR;

	if (lib_handler == NULL)
		return -1;
	if (lib_handler->api == NULL)
		return -1;
	if (lib_handler->api->arsi_set_dl_digital_gain == NULL)
		return -1;

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	retval = lib_handler->api->arsi_set_dl_digital_gain(
			 dl_analog_gain_ref_only,
			 dl_digital_gain,
			 lib_handler->arsi_handler);

	AUD_LOG_D("lib_name %s, %p, set dl_analog_gain_ref_only %d, dl_digital_gain %d, retval %d",
		  lib_handler->lib_name, lib_handler, dl_analog_gain_ref_only, dl_digital_gain,
		  retval);

	UNLOCK_ALOCK(lib_handler->lock);
	return (retval == NO_ERROR) ? 0 : -1;
}


int aurisys_arsi_set_ul_enhance(
	aurisys_lib_handler_t *lib_handler,
	const uint8_t b_enhance_on)
{
	status_t retval = NO_ERROR;

	if (lib_handler == NULL)
		return -1;
	if (lib_handler->api == NULL)
		return -1;
	if (lib_handler->api->arsi_set_ul_enhance == NULL)
		return -1;

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	retval = lib_handler->api->arsi_set_ul_enhance(
			 b_enhance_on,
			 lib_handler->arsi_handler);

	AUD_LOG_D("lib_name %s, %p, set ul b_enhance_on %d, retval %d",
		  lib_handler->lib_name, lib_handler, b_enhance_on, retval);

	UNLOCK_ALOCK(lib_handler->lock);
	return (retval == NO_ERROR) ? 0 : -1;
}


int aurisys_arsi_set_dl_enhance(
	aurisys_lib_handler_t *lib_handler,
	const uint8_t b_enhance_on)
{
	status_t retval = NO_ERROR;

	if (lib_handler == NULL)
		return -1;
	if (lib_handler->api == NULL)
		return -1;
	if (lib_handler->api->arsi_set_dl_enhance == NULL)
		return -1;

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	retval = lib_handler->api->arsi_set_dl_enhance(
			 b_enhance_on,
			 lib_handler->arsi_handler);

	AUD_LOG_D("lib_name %s, %p, set dl b_enhance_on %d, retval %d",
		  lib_handler->lib_name, lib_handler, b_enhance_on, retval);

	UNLOCK_ALOCK(lib_handler->lock);
	return (retval == NO_ERROR) ? 0 : -1;
}


int aurisys_arsi_set_ul_mute(
	aurisys_lib_handler_t *lib_handler,
	const uint8_t b_mute_on)
{
	status_t retval = NO_ERROR;

	if (lib_handler == NULL)
		return -1;
	if (lib_handler->api == NULL)
		return -1;
	if (lib_handler->api->arsi_set_ul_mute == NULL)
		return -1;

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	retval = lib_handler->api->arsi_set_ul_mute(
			 b_mute_on,
			 lib_handler->arsi_handler);

	AUD_LOG_D("lib_name %s, %p, set ul b_mute_on %d, retval %d",
		  lib_handler->lib_name, lib_handler, b_mute_on, retval);

	UNLOCK_ALOCK(lib_handler->lock);
	return (retval == NO_ERROR) ? 0 : -1;
}


int aurisys_arsi_set_dl_mute(
	aurisys_lib_handler_t *lib_handler,
	const uint8_t b_mute_on)
{
	status_t retval = NO_ERROR;

	if (lib_handler == NULL)
		return -1;
	if (lib_handler->api == NULL)
		return -1;
	if (lib_handler->api->arsi_set_dl_mute == NULL)
		return -1;

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	retval = lib_handler->api->arsi_set_dl_mute(
			 b_mute_on,
			 lib_handler->arsi_handler);

	AUD_LOG_D("lib_name %s, %p, set dl b_mute_on %d, retval %d",
		  lib_handler->lib_name, lib_handler, b_mute_on, retval);

	UNLOCK_ALOCK(lib_handler->lock);
	return (retval == NO_ERROR) ? 0 : -1;
}


int aurisys_arsi_apply_param(aurisys_lib_handler_t *lib_handler)
{
	status_t retval = NO_ERROR;

	if (!lib_handler ||
	    !lib_handler->arsi_handler ||
	    !lib_handler->api ||
	    !lib_handler->api->arsi_update_param) {
		AUD_LOG_E("%s(), NULL!!", __FUNCTION__);
		return -ENODEV;
	}

	if (lib_handler->param_buf.p_buffer == NULL ||
	    lib_handler->param_buf.data_size == 0 ||
	    lib_handler->param_buf.memory_size == 0)
		return NO_ERROR;

	LOCK_ALOCK_MS(lib_handler->lock, 500);
	retval = lib_handler->api->arsi_update_param(
			 lib_handler->task_config,
			 &lib_handler->lib_config,
			 &lib_handler->param_buf,
			 lib_handler->arsi_handler);
	if (retval != NO_ERROR)
		AUD_LOG_E("%s(), %p, arsi_update_param fail, ret = %d", __FUNCTION__,
			  lib_handler, retval);

	else
		AUD_LOG_D("%s(), %p, arsi_update_param done", __FUNCTION__, lib_handler);

	UNLOCK_ALOCK(lib_handler->lock);
	return (int)retval;
}


int aurisys_arsi_run_adb_cmd(aurisys_lib_handler_t *lib_handler,
			     aurisys_adb_command_t *adb_cmd)
{
	status_t retval = NO_ERROR;

	AurisysLibInterface *arsi_api = NULL;
	void *arsi_handler = NULL;

	bool run_adb_done = false;
#ifdef CFG_MTK_AUDIODSP_SUPPORT
	bool update_dvfs = false;
	struct dvfs_swip_config_t dvfs_config;
	uint32_t swip_id;
	uint32_t feature_id;
#endif
	string_buf_t key_value_pair;

	if (!lib_handler || !adb_cmd) {
		AUD_LOG_D("%s(), lib_handler %p adb_cmd %p NULL!!",
			  __FUNCTION__, lib_handler, adb_cmd);
		return -1;
	}

	LOCK_ALOCK_MS(lib_handler->lock, 500);

	arsi_api = lib_handler->api;
	arsi_handler = lib_handler->arsi_handler;
	if (!arsi_api) {
		AUD_LOG_D("%s(), arsi_api %p NULL!!", __FUNCTION__, arsi_api);
		UNLOCK_ALOCK(lib_handler->lock);
		return -1;
	}


	switch (adb_cmd->adb_cmd_type) {
	case ADB_CMD_ENABLE_LOG:
		if (adb_cmd->direction == AURISYS_SET_PARAM) {
			AUD_LOG_D("lib_name %s, %p, ADB_CMD_ENABLE_LOG: %d",
				  lib_handler->lib_name, lib_handler, adb_cmd->enable_log);
			lib_handler->debug_log_fp = (adb_cmd->enable_log)
						    ? arsi_lib_printf
						    : arsi_lib_printf_dummy;
			if (arsi_handler && arsi_api->arsi_set_debug_log_fp) {
				retval = arsi_api->arsi_set_debug_log_fp(
						 lib_handler->debug_log_fp,
						 arsi_handler);
				if (retval == 0)
					run_adb_done = true;
			}
		}
		break;
	case ADB_CMD_APPLY_PARAM:
		if (adb_cmd->direction == AURISYS_SET_PARAM) {
			AUD_LOG_D("lib_name %s, %p, ADB_CMD_APPLY_PARAM: %u",
				  lib_handler->lib_name, lib_handler, adb_cmd->enhancement_mode);
			if (lib_handler->param_buf.p_buffer == NULL ||
			    lib_handler->param_buf.data_size == 0 ||
			    lib_handler->param_buf.memory_size == 0) {
				AUD_LOG_W("%s(-) %p, no param data!!", __FUNCTION__, lib_handler);
				break;
			}

			/* apply */
			if (arsi_handler) {
				if (arsi_api->arsi_update_param) {
					retval = arsi_api->arsi_update_param(
							 lib_handler->task_config,
							 &lib_handler->lib_config,
							 &lib_handler->param_buf,
							 arsi_handler);
					if (retval == 0)
						run_adb_done = true;
				}
			}
		}
		break;
	case ADB_CMD_ADDR_VALUE:
		if (adb_cmd->direction == AURISYS_SET_PARAM) {
			if (arsi_handler && arsi_api->arsi_set_addr_value) {
				retval = arsi_api->arsi_set_addr_value(
						 adb_cmd->addr_value_pair.addr,
						 adb_cmd->addr_value_pair.value,
						 arsi_handler);
				if (retval == 0)
					run_adb_done = true;
				AUD_LOG_D("lib_name %s, %p, ADB_CMD_ADDR_VALUE: set *0x%x = 0x%x, retval %d",
					  lib_handler->lib_name,
					  lib_handler,
					  adb_cmd->addr_value_pair.addr,
					  adb_cmd->addr_value_pair.value,
					  retval);
			}
		} else if (adb_cmd->direction == AURISYS_GET_PARAM) {
			if (arsi_handler && arsi_api->arsi_get_addr_value) {
				retval = arsi_api->arsi_get_addr_value(
						 adb_cmd->addr_value_pair.addr,
						 &adb_cmd->addr_value_pair.value,
						 arsi_handler);
				if (retval == 0)
					run_adb_done = true;
				AUD_LOG_D("lib_name %s, %p, ADB_CMD_ADDR_VALUE: get *0x%x is 0x%x, retval %d",
					  lib_handler->lib_name,
					  lib_handler,
					  adb_cmd->addr_value_pair.addr,
					  adb_cmd->addr_value_pair.value,
					  retval);
			}
		}
		break;
	case ADB_CMD_KEY_VALUE:
		if (adb_cmd->direction == AURISYS_SET_PARAM) {
			if (arsi_handler && arsi_api->arsi_set_key_value_pair) {
				char_to_string(&key_value_pair, adb_cmd->key_value, MAX_ADB_CMD_KEY_VALUE_SIZE);
				retval = arsi_api->arsi_set_key_value_pair(
						 &key_value_pair,
						 arsi_handler);
				if (retval == 0) {
					run_adb_done = true;
#ifdef CFG_MTK_AUDIODSP_SUPPORT
					if (!strcmp("mtk_bessound", lib_handler->lib_name))
						update_dvfs = true;
#endif
				}
				AUD_LOG_D("lib_name %s, %p, ADB_CMD_KEY_VALUE: set %u, %u, %s, retval %d",
					  lib_handler->lib_name, lib_handler,
					  key_value_pair.memory_size,
					  key_value_pair.string_size,
					  key_value_pair.p_string,
					  retval);
			}
		} else if (adb_cmd->direction == AURISYS_GET_PARAM) {
			if (arsi_handler && arsi_api->arsi_get_key_value_pair) {
				char_to_string(&key_value_pair, adb_cmd->key_value, MAX_ADB_CMD_KEY_VALUE_SIZE);
				retval = arsi_api->arsi_get_key_value_pair(
						 &key_value_pair,
						 arsi_handler);
				if (retval == 0)
					run_adb_done = true;
				AUD_LOG_D("lib_name %s, %p, ADB_CMD_KEY_VALUE: get %u, %u, %s, retval %d",
					  lib_handler->lib_name, lib_handler,
					  key_value_pair.memory_size,
					  key_value_pair.string_size,
					  key_value_pair.p_string,
					  retval);
			}
		}
		break;
	case ADB_CMD_ENABLE_LIB_DUMP:
		if (adb_cmd->direction == AURISYS_SET_PARAM) {
			if (arsi_handler &&
			    arsi_api->arsi_query_max_debug_dump_buf_size &&
			    lib_handler->lib_dump_buf.p_buffer == NULL) {
				retval = arsi_api->arsi_query_max_debug_dump_buf_size(
							&lib_handler->lib_dump_buf, arsi_handler);
				AUD_LOG_D("%s(), arsi_query_max_debug_dump_buf_size, retval: %d, memory_size: %u",
					  __FUNCTION__, retval, lib_handler->lib_dump_buf.memory_size);
				if (retval == NO_ERROR &&
				    lib_handler->lib_dump_buf.memory_size > 0) {
					AUDIO_ALLOC_CHAR_BUFFER(lib_handler->lib_dump_buf.p_buffer,
								lib_handler->lib_dump_buf.memory_size);
					lib_handler->lib_dump_enabled = true;
				}
			}
			run_adb_done = true;
		}
		break;
	default:
		AUD_LOG_W("lib_name %s, %p, not support adb_cmd_type %d!!",
			  lib_handler->lib_name, lib_handler, adb_cmd->adb_cmd_type);
		retval = BAD_VALUE;
		break;
	}
#ifdef CFG_MTK_AUDIODSP_SUPPORT
	if (update_dvfs) {
		map_aurisys_config_to_dvfs_config(lib_handler, &dvfs_config);
		swip_id = get_swip_id(lib_handler->lib_name,
				      lib_handler->task_config->task_scene);
		feature_id = get_feature_id(lib_handler->task_config->task_scene);
		dvfs_delete_swip(feature_id, swip_id, &dvfs_config);
		dvfs_add_swip(feature_id, swip_id, &dvfs_config);
	}
#endif
	UNLOCK_ALOCK(lib_handler->lock);

	return (retval == NO_ERROR && run_adb_done == true) ? 0 : -1;
}


int aurisys_arsi_set_buf(
	aurisys_lib_handler_t *lib_handler,
	const struct data_buf_t *data_buf,
	const uint32_t id)
{
	AurisysLibInterface *arsi_api = NULL;
	void *arsi_handler = NULL;
	status_t retval = NO_ERROR;

	if (!lib_handler || !data_buf) {
		ALOGE("%s(), lib_handler %p, data_buf %p!!",
		      __FUNCTION__, lib_handler, data_buf);
		return -ENODEV;
	}
	if (!data_buf->p_buffer || !data_buf->data_size || !data_buf->memory_size) {
		ALOGE("%s(), p_buffer %p, data_size %u, memory_size %u!!",
		      __FUNCTION__, data_buf->p_buffer, data_buf->data_size, data_buf->memory_size);
		return -ENODEV;
	}

	arsi_api = lib_handler->api;
	arsi_handler = lib_handler->arsi_handler;

	if (!arsi_api) {
		AUD_LOG_D("%s(), arsi_api %p NULL!!", __FUNCTION__, arsi_api);
		return -ENODEV;
	}

	LOCK_ALOCK_MS(lib_handler->lock, 500);
	if (arsi_api->arsi_set_buf && arsi_handler)
		retval = arsi_api->arsi_set_buf(id, data_buf, arsi_handler);
	UNLOCK_ALOCK(lib_handler->lock);

	return (int)retval;
}


int aurisys_arsi_get_buf(
	aurisys_lib_handler_t *lib_handler,
	struct data_buf_t *data_buf,
	const uint32_t id)
{
	AurisysLibInterface *arsi_api = NULL;
	void *arsi_handler = NULL;
	status_t retval = NO_ERROR;

	if (!lib_handler || !data_buf) {
		ALOGE("%s(), lib_handler %p, data_buf %p!!",
		      __FUNCTION__, lib_handler, data_buf);
		return -ENODEV;
	}
	if (!data_buf->p_buffer || !data_buf->memory_size) {
		ALOGE("%s(), p_buffer %p, memory_size %u!!",
		      __FUNCTION__, data_buf->p_buffer, data_buf->memory_size);
		return -ENODEV;
	}

	arsi_api = lib_handler->api;
	arsi_handler = lib_handler->arsi_handler;

	if (!arsi_api) {
		AUD_LOG_D("%s(), arsi_api %p NULL!!", __FUNCTION__, arsi_api);
		return -ENODEV;
	}

	LOCK_ALOCK_MS(lib_handler->lock, 500);
	if (arsi_api->arsi_get_buf && arsi_handler)
		retval = arsi_api->arsi_get_buf(id, data_buf, arsi_handler);
	UNLOCK_ALOCK(lib_handler->lock);

	return (int)retval;
}


/*
 * =============================================================================
 *                     private function implementation
 * =============================================================================
 */

static void arsi_lib_printf(const char *message, ...)
{
	int ret = 0;
	static char printf_msg[256];

	va_list args;
	va_start(args, message);

	ret = vsnprintf(printf_msg, sizeof(printf_msg), message, args);
	if (ret >= 0)
		AUD_LOG_D("[LIB] %s", printf_msg);

	va_end(args);
}


static void arsi_lib_printf_dummy(const char *message, ...)
{
	if (message == NULL) {
		AUD_LOG_E("%s(), NULL!! return", __FUNCTION__);
		return;
	}

	return;
}


static void allocate_data_buf(audio_buf_t *audio_buf)
{
	data_buf_t *data_buf = &audio_buf->data_buf;

	if (audio_buf->frame_size_ms != 0) {
		data_buf->memory_size = get_frame_buf_size(audio_buf);
		AUDIO_ALLOC_BUFFER(data_buf->p_buffer, data_buf->memory_size);
	} else
		data_buf->memory_size = 0;

	data_buf->data_size = 0;
}


static void init_audio_buf_by_lib_config(
	audio_buf_t *audio_buf,
	arsi_lib_config_t *lib_config)
{
	if (audio_buf == NULL || lib_config == NULL) {
		AUD_LOG_E("%s(), NULL!! return", __FUNCTION__);
		return;
	}

	/* same as lib_config */
	audio_buf->b_interleave = lib_config->b_interleave;
	audio_buf->frame_size_ms = lib_config->frame_size_ms;
	audio_buf->sample_rate_buffer = lib_config->sample_rate;
	audio_buf->sample_rate_content = lib_config->sample_rate; /* TODO */
	audio_buf->audio_format = lib_config->audio_format;

	/* alloc data_buf */
	allocate_data_buf(audio_buf);

	AUD_LOG_V("audio_buf data_buf_type %d", audio_buf->data_buf_type);
	AUD_LOG_V("audio_buf b_interleave %d", audio_buf->b_interleave);
	AUD_LOG_V("audio_buf frame_size_ms %d", audio_buf->frame_size_ms);
	AUD_LOG_V("audio_buf num_channels %d", audio_buf->num_channels);
	AUD_LOG_V("audio_buf channel_mask 0x%x", audio_buf->channel_mask); /* TODO */
	AUD_LOG_V("audio_buf sample_rate_buffer %u", audio_buf->sample_rate_buffer);
	AUD_LOG_V("audio_buf sample_rate_content %u", audio_buf->sample_rate_content);
	AUD_LOG_V("audio_buf audio_format 0x%x", audio_buf->audio_format);
	AUD_LOG_V("audio_buf memory_size %u", audio_buf->data_buf.memory_size);
	AUD_LOG_V("audio_buf p_buffer %p", audio_buf->data_buf.p_buffer);
}


static void clone_lib_config(
	arsi_lib_config_t *des,
	arsi_lib_config_t *src,
	const aurisys_component_t *the_component,
	const struct aurisys_lib_manager_config_t *manager_config)
{
	int i = 0;

	des->sample_rate = audio_sample_rate_get_match_rate(
				   the_component->sample_rate_mask,
				   manager_config->sample_rate);
	des->audio_format = get_dedicated_format_from_mask(
				    the_component->support_format_mask,
				    manager_config->audio_format);
	des->frame_size_ms = get_dedicated_frame_ms_from_mask(
				     the_component->support_frame_ms_mask,
				     manager_config->frame_size_ms);

	des->b_interleave = src->b_interleave;
	des->num_ul_ref_buf_array = 0;
	des->num_dl_ref_buf_array = 0;

	if (manager_config->arsi_process_type == ARSI_PROCESS_TYPE_UL_ONLY ||
	    manager_config->arsi_process_type == ARSI_PROCESS_TYPE_UL_AND_DL) {
		des->num_ul_ref_buf_array = src->num_ul_ref_buf_array;
		/* ul in */
		if (src->p_ul_buf_in != NULL) {
			AUDIO_ALLOC_STRUCT(audio_buf_t, des->p_ul_buf_in);
			if (des->p_ul_buf_in == NULL)
				goto AURISYS_CLONE_LIB_CONFG_FAIL;

			/* data_buf_type & num_channels */
			memcpy(des->p_ul_buf_in, src->p_ul_buf_in, sizeof(audio_buf_t));
			des->p_ul_buf_in->num_channels =
				get_dedicated_channel_number_from_mask(
					the_component->support_channel_number_mask[des->p_ul_buf_in->data_buf_type],
					manager_config->num_channels_ul);

			des->p_ul_buf_in->channel_mask = get_input_ch_mask(
					des->p_ul_buf_in->num_channels);
			init_audio_buf_by_lib_config(des->p_ul_buf_in, des);
		}

		/* ul out */
		if (src->p_ul_buf_out != NULL) {
			AUDIO_ALLOC_STRUCT(audio_buf_t, des->p_ul_buf_out);
			if (des->p_ul_buf_out == NULL)
				goto AURISYS_CLONE_LIB_CONFG_FAIL;

			/* data_buf_type & num_channels */
			memcpy(des->p_ul_buf_out, src->p_ul_buf_out, sizeof(audio_buf_t));
			des->p_ul_buf_out->num_channels =
				get_dedicated_channel_number_from_mask(
					the_component->support_channel_number_mask[des->p_ul_buf_out->data_buf_type],
					manager_config->num_channels_ul);

			des->p_ul_buf_out->channel_mask = get_input_ch_mask(
					des->p_ul_buf_out->num_channels);
			init_audio_buf_by_lib_config(des->p_ul_buf_out, des);
		}

		/* ul refs */
		if (src->num_ul_ref_buf_array != 0) {
			AUDIO_ALLOC_STRUCT_ARRAY(audio_buf_t, src->num_ul_ref_buf_array,
						 des->p_ul_ref_bufs);
			if (des->p_ul_ref_bufs == NULL)
				goto AURISYS_CLONE_LIB_CONFG_FAIL;

			for (i = 0; i < src->num_ul_ref_buf_array; i++) {
				/* data_buf_type & num_channels */
				memcpy(&des->p_ul_ref_bufs[i], &src->p_ul_ref_bufs[i], sizeof(audio_buf_t));

				des->p_ul_ref_bufs[i].channel_mask = get_input_ch_mask(
						des->p_ul_ref_bufs[i].num_channels);
				init_audio_buf_by_lib_config(&des->p_ul_ref_bufs[i], des);
			}
		}
	}

	if (manager_config->arsi_process_type == ARSI_PROCESS_TYPE_DL_ONLY ||
	    manager_config->arsi_process_type == ARSI_PROCESS_TYPE_UL_AND_DL) {
		des->num_dl_ref_buf_array = src->num_dl_ref_buf_array;
		/* dl in */
		if (src->p_dl_buf_in != NULL) {
			AUDIO_ALLOC_STRUCT(audio_buf_t, des->p_dl_buf_in);
			if (des->p_dl_buf_in == NULL)
				goto AURISYS_CLONE_LIB_CONFG_FAIL;

			/* data_buf_type & num_channels */
			memcpy(des->p_dl_buf_in, src->p_dl_buf_in, sizeof(audio_buf_t));
			des->p_dl_buf_in->num_channels =
				get_dedicated_channel_number_from_mask(
					the_component->support_channel_number_mask[des->p_dl_buf_in->data_buf_type],
					manager_config->num_channels_dl);

			des->p_dl_buf_in->channel_mask = get_output_ch_mask(
					des->p_dl_buf_in->num_channels);
			init_audio_buf_by_lib_config(des->p_dl_buf_in, des);
		}

		/* dl out */
		if (src->p_dl_buf_out != NULL) {
			AUDIO_ALLOC_STRUCT(audio_buf_t, des->p_dl_buf_out);
			if (des->p_dl_buf_out == NULL)
				goto AURISYS_CLONE_LIB_CONFG_FAIL;

			/* data_buf_type & num_channels */
			memcpy(des->p_dl_buf_out, src->p_dl_buf_out, sizeof(audio_buf_t));
			des->p_dl_buf_out->num_channels =
				get_dedicated_channel_number_from_mask(
					the_component->support_channel_number_mask[des->p_dl_buf_out->data_buf_type],
					manager_config->num_channels_dl);

			des->p_dl_buf_out->channel_mask = get_output_ch_mask(
					des->p_dl_buf_out->num_channels);
			init_audio_buf_by_lib_config(des->p_dl_buf_out, des);
		}

		/* dl refs */
		if (src->num_dl_ref_buf_array != 0) {
			AUDIO_ALLOC_STRUCT_ARRAY(audio_buf_t, src->num_dl_ref_buf_array,
						 des->p_dl_ref_bufs);
			if (des->p_dl_ref_bufs == NULL)
				goto AURISYS_CLONE_LIB_CONFG_FAIL;

			for (i = 0; i < src->num_dl_ref_buf_array; i++) {
				/* data_buf_type & num_channels */
				memcpy(&des->p_dl_ref_bufs[i], &src->p_dl_ref_bufs[i], sizeof(audio_buf_t));

				des->p_dl_ref_bufs[i].channel_mask = get_output_ch_mask(
						des->p_dl_ref_bufs[i].num_channels);
				init_audio_buf_by_lib_config(&des->p_dl_ref_bufs[i], des);
				if (des->p_dl_ref_bufs[i].data_buf_type == DATA_BUF_IV_BUFFER)
					des->p_dl_ref_bufs[i].audio_format = manager_config->iv_format;
			}
		}
	}
	dump_lib_config(des);
	return;

AURISYS_CLONE_LIB_CONFG_FAIL:
	AUD_LOG_E("%s() fail", __FUNCTION__);
	AUDIO_FREE_POINTER(des->p_ul_buf_in);
	AUDIO_FREE_POINTER(des->p_ul_buf_out);
	AUDIO_FREE_POINTER(des->p_ul_ref_bufs);
	AUDIO_FREE_POINTER(des->p_dl_buf_in);
	AUDIO_FREE_POINTER(des->p_dl_buf_out);
	AUDIO_FREE_POINTER(des->p_dl_ref_bufs);
}


static void release_lib_config(arsi_lib_config_t *lib_config)
{
	int i = 0;

	AUD_LOG_VV("sample_rate %d", lib_config->sample_rate);
	AUD_LOG_VV("audio_format %d", lib_config->audio_format);
	AUD_LOG_VV("frame_size_ms %d", lib_config->frame_size_ms);
	AUD_LOG_VV("b_interleave %d", lib_config->b_interleave);
	AUD_LOG_VV("num_ul_ref_buf_array %d", lib_config->num_ul_ref_buf_array);
	AUD_LOG_VV("num_dl_ref_buf_array %d", lib_config->num_dl_ref_buf_array);


	/* TODO: move there config to a function */

	/* ul in */
	if (lib_config->p_ul_buf_in != NULL) {
		AUD_LOG_V("UL buf_in %p", lib_config->p_ul_buf_in->data_buf.p_buffer);
		AUDIO_FREE_POINTER(lib_config->p_ul_buf_in->data_buf.p_buffer);
		AUDIO_FREE_POINTER(lib_config->p_ul_buf_in);
	}

	/* ul out */
	if (lib_config->p_ul_buf_out != NULL) {
		AUD_LOG_V("UL buf_out %p", lib_config->p_ul_buf_out->data_buf.p_buffer);
		AUDIO_FREE_POINTER(lib_config->p_ul_buf_out->data_buf.p_buffer);
		AUDIO_FREE_POINTER(lib_config->p_ul_buf_out);
	}

	/* ul refs */
	if (lib_config->num_ul_ref_buf_array != 0) {
		for (i = 0; i < lib_config->num_ul_ref_buf_array; i++) {
			AUD_LOG_V("UL buf_ref[%d] %p", i,
				  lib_config->p_ul_ref_bufs[i].data_buf.p_buffer);
			AUDIO_FREE_POINTER(lib_config->p_ul_ref_bufs[i].data_buf.p_buffer);
		}
		AUDIO_FREE_POINTER(lib_config->p_ul_ref_bufs);
	}

	/* dl in */
	if (lib_config->p_dl_buf_in != NULL) {
		AUD_LOG_V("DL buf_in %p", lib_config->p_dl_buf_in->data_buf.p_buffer);
		AUDIO_FREE_POINTER(lib_config->p_dl_buf_in->data_buf.p_buffer);
		AUDIO_FREE_POINTER(lib_config->p_dl_buf_in);
	}

	/* dl out */
	if (lib_config->p_dl_buf_out != NULL) {
		AUD_LOG_V("DL buf_out %p", lib_config->p_dl_buf_out->data_buf.p_buffer);
		AUDIO_FREE_POINTER(lib_config->p_dl_buf_out->data_buf.p_buffer);
		AUDIO_FREE_POINTER(lib_config->p_dl_buf_out);
	}

	/* dl refs */
	if (lib_config->num_dl_ref_buf_array != 0) {
		for (i = 0; i < lib_config->num_dl_ref_buf_array; i++) {
			AUD_LOG_V("DL buf_ref[%d] %p", i,
				  lib_config->p_dl_ref_bufs[i].data_buf.p_buffer);
			AUDIO_FREE_POINTER(lib_config->p_dl_ref_bufs[i].data_buf.p_buffer);
		}
		AUDIO_FREE_POINTER(lib_config->p_dl_ref_bufs);
	}
}


static void init_pool_buf(aurisys_lib_handler_t *lib_handler)
{
	audio_buf_t *source = NULL;
	audio_buf_t *target = NULL;

	audio_buf_t *ref = NULL;
	int i = 0;

	/* ul pool */
	if (lib_handler->lib_config.p_ul_buf_in != NULL &&
	    lib_handler->lib_config.p_ul_buf_out != NULL) {

		create_pool_buf(&lib_handler->ul_pool_in,
				lib_handler->lib_config.p_ul_buf_in,
				0);
		create_pool_buf(&lib_handler->ul_pool_out,
				lib_handler->lib_config.p_ul_buf_out,
				0);

		source = lib_handler->ul_pool_in.buf;
		target = lib_handler->ul_pool_out.buf;
		AUD_LOG_V("UL in/out sample_rate: %d, %d, num_channels: %d, %d, audio_format: 0x%x, 0x%x",
			  source->sample_rate_buffer, target->sample_rate_buffer,
			  source->num_channels, target->num_channels,
			  source->audio_format, target->audio_format);

		/* aec */
		if (lib_handler->lib_config.p_ul_ref_bufs != NULL) {
			for (i = 0; i < lib_handler->lib_config.num_ul_ref_buf_array; i++) {
				ref = &lib_handler->lib_config.p_ul_ref_bufs[i];
				if (ref->data_buf_type == DATA_BUF_ECHO_REF) {
					AUDIO_ALLOC_STRUCT(audio_pool_buf_t, lib_handler->aec_pool_in);
					if (lib_handler->aec_pool_in != NULL) {
						create_pool_buf(lib_handler->aec_pool_in,
								ref, 0);
						lib_handler->p_ul_ref_buf_aec = ref;
					} else
						AUD_LOG_E("%s(), creat ul_ref_buf fail!", __FUNCTION__);
					break;
				}
			}
		}
	}

	/* dl pool */
	if (lib_handler->lib_config.p_dl_buf_in != NULL &&
	    lib_handler->lib_config.p_dl_buf_out != NULL) {
		create_pool_buf(&lib_handler->dl_pool_in,
				lib_handler->lib_config.p_dl_buf_in,
				0);
		create_pool_buf(&lib_handler->dl_pool_out,
				lib_handler->lib_config.p_dl_buf_out,
				0);

		source = lib_handler->dl_pool_in.buf;
		target = lib_handler->dl_pool_out.buf;
		AUD_LOG_V("DL in/out sample_rate: %d, %d, num_channels: %d, %d, audio_format: 0x%x, 0x%x",
			  source->sample_rate_buffer, target->sample_rate_buffer,
			  source->num_channels, target->num_channels,
			  source->audio_format, target->audio_format);
		/* iv */
		if (lib_handler->lib_config.p_dl_ref_bufs != NULL) {
			for (i = 0; i < lib_handler->lib_config.num_dl_ref_buf_array; i++) {
				ref = &lib_handler->lib_config.p_dl_ref_bufs[i];
				if (ref->data_buf_type == DATA_BUF_IV_BUFFER) {
					AUDIO_ALLOC_STRUCT(audio_pool_buf_t, lib_handler->iv_pool_in);
					if (lib_handler->iv_pool_in != NULL) {
						create_pool_buf(lib_handler->iv_pool_in,
								ref,
								0);
						lib_handler->p_dl_ref_buf_iv = ref;
					} else
						AUD_LOG_E("%s(), creat dl_ref_buf fail!", __FUNCTION__);
					break;
				}
			}
		}
	}
}


static void deinit_pool_buf(aurisys_lib_handler_t *lib_handler)
{
	/* ul pool */
	if (lib_handler->lib_config.p_ul_buf_in != NULL &&
	    lib_handler->lib_config.p_ul_buf_out != NULL) {
		destroy_pool_buf(&lib_handler->ul_pool_in);
		destroy_pool_buf(&lib_handler->ul_pool_out);
	}

	/* aec */
	if (lib_handler->aec_pool_in != NULL) {
		destroy_pool_buf(lib_handler->aec_pool_in);
		AUDIO_FREE_POINTER(lib_handler->aec_pool_in);
	}

	/* dl pool */
	if (lib_handler->lib_config.p_dl_buf_in != NULL &&
	    lib_handler->lib_config.p_dl_buf_out != NULL) {
		destroy_pool_buf(&lib_handler->dl_pool_in);
		destroy_pool_buf(&lib_handler->dl_pool_out);
	}

	/* iv */
	if (lib_handler->iv_pool_in != NULL) {
		destroy_pool_buf(lib_handler->iv_pool_in);
		AUDIO_FREE_POINTER(lib_handler->iv_pool_in);
	}
}


void aurisys_create_lib_handler_list_xlink(
	struct aurisys_library_name_t *name_list,
	struct aurisys_library_config_t *library_config_list,
	struct aurisys_lib_handler_t **lib_handler_list,
	uint32_t *num_library_hanlder,
	const struct aurisys_lib_manager_config_t *manager_config)
{
	aurisys_library_config_t *the_library_config = NULL;
	aurisys_component_t *the_component = NULL;
	aurisys_lib_handler_t *new_lib_handler = NULL;

	aurisys_library_name_t *itor_library_name = NULL;
	aurisys_library_name_t *tmp_library_name = NULL;
	uint32_t lib_order = 0;

	//char mDumpFileName[128];


	HASH_ITER(hh, name_list, itor_library_name, tmp_library_name) {
		HASH_FIND_STR(library_config_list,
			      itor_library_name->name,
			      the_library_config);
		if (the_library_config == NULL) {
			AUD_ASSERT(the_library_config != NULL);
			return;
		}

		HASH_FIND_INT(
			the_library_config->component_hh,
			&manager_config->aurisys_scenario,
			the_component);
		if (the_component == NULL) {
			AUD_ASSERT(the_component != NULL);
			return;
		}

		AUDIO_ALLOC_STRUCT(aurisys_lib_handler_t, new_lib_handler);

		*num_library_hanlder = (*num_library_hanlder) + 1;

		new_lib_handler->self = new_lib_handler;
		new_lib_handler->lib_name = itor_library_name->name;
		new_lib_handler->task_config = &manager_config->task_config;
		new_lib_handler->dsp_task_scene = manager_config->dsp_task_scene;
		new_lib_handler->arsi_process_type = manager_config->arsi_process_type;
		new_lib_handler->lib_order = lib_order;

		NEW_ALOCK(new_lib_handler->lock);
		LOCK_ALOCK_MS(new_lib_handler->lock, 1000);

		clone_lib_config(
			&new_lib_handler->lib_config,
			&the_component->lib_config,
			the_component,
			manager_config);
		init_pool_buf(new_lib_handler);


		new_lib_handler->enable_log = &the_component->enable_log;
		new_lib_handler->enable_raw_dump = &the_component->enable_raw_dump;
		new_lib_handler->enable_lib_dump = &the_component->enable_lib_dump;
		new_lib_handler->enhancement_mode = &the_component->enhancement_mode;

		new_lib_handler->debug_log_fp = (the_component->enable_log)
						? arsi_lib_printf
						: arsi_lib_printf_dummy;

		new_lib_handler->api = the_library_config->api;
		new_lib_handler->param_path = the_library_config->param_path;
		new_lib_handler->lib_dump_path = the_library_config->lib_dump_path;
		new_lib_handler->lib_id = the_library_config->lib_id;


		if (*new_lib_handler->enable_lib_dump) {
			new_lib_handler->lib_dump_enabled = true;
			/*if (!strcmp(new_lib_handler->lib_dump_path, "AUTO")) {
			    sprintf(mDumpFileName, "%s/%s.%d.%d.%d.lib_dump.bin", audio_dump, itor_library_name->name, mDumpFileNum, getpid(), gettid());
			} else {
			    sprintf(mDumpFileName, "%s", new_lib_handler->lib_dump_path);
			}*/
		}

		/*if (new_lib_handler->raw_dump_enabled || new_lib_handler->lib_dump_enabled) {
		    mDumpFileNum++;
		    mDumpFileNum %= MAX_DUMP_NUM;
		}*/



		/* hash for manager */
		HASH_ADD_KEYPTR(hh_all, *lib_handler_list,
				new_lib_handler->lib_name, strlen(new_lib_handler->lib_name),
				new_lib_handler);

		/* hash for adb cmd */
		HASH_ADD_KEYPTR(hh_component, the_component->lib_handler_list_for_adb_cmd,
				new_lib_handler->self, sizeof(new_lib_handler->self),
				new_lib_handler);
		/*
		 * keep the componet hh s.t. we can remove lib_handler in component hh
		 * when we destroy lib_handler
		 */
		new_lib_handler->head = &the_component->lib_handler_list_for_adb_cmd;


		UNLOCK_ALOCK(new_lib_handler->lock);
		new_lib_handler = NULL;
		lib_order++;
	}
}


void aurisys_destroy_lib_handler_list_xlink(aurisys_lib_handler_t
		**handler_list)
{
	aurisys_lib_handler_t *itor_lib_hanlder = NULL;
	aurisys_lib_handler_t *tmp_lib_hanlder = NULL;


	if (handler_list == NULL || *handler_list == NULL)
		return;

	HASH_ITER(hh_all, *handler_list, itor_lib_hanlder, tmp_lib_hanlder) {
		LOCK_ALOCK_MS(itor_lib_hanlder->lock, 1000);
		AUD_LOG_V("itor_lib_hanlder %p", itor_lib_hanlder);

		HASH_DELETE(hh_all, *handler_list, itor_lib_hanlder);
		HASH_DELETE(hh_component, *itor_lib_hanlder->head, itor_lib_hanlder);

#if 0
		if (itor_lib_hanlder->lib_dump_enabled) {
			if (itor_lib_hanlder->lib_dump != NULL &&
			    itor_lib_hanlder->lib_dump->mFilep != NULL)
				itor_lib_hanlder->lib_dump->AudioCloseDumpPCMFile(itor_lib_hanlder->lib_dump);
			AUDIO_FREE_POINTER(itor_lib_hanlder->lib_dump);
			AUDIO_FREE_POINTER(itor_lib_hanlder->lib_dump_buf.p_buffer);
		}

		if (itor_lib_hanlder->raw_dump_enabled) {
			/* UL in */
			if (itor_lib_hanlder->pcm_dump_ul_in != NULL &&
			    itor_lib_hanlder->pcm_dump_ul_in->mFilep != NULL)
				itor_lib_hanlder->pcm_dump_ul_in->AudioCloseDumpPCMFile(
					itor_lib_hanlder->pcm_dump_ul_in);
			AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_ul_in);
			/* UL out */
			if (itor_lib_hanlder->pcm_dump_ul_out != NULL &&
			    itor_lib_hanlder->pcm_dump_ul_out->mFilep != NULL)
				itor_lib_hanlder->pcm_dump_ul_out->AudioCloseDumpPCMFile(
					itor_lib_hanlder->pcm_dump_ul_out);
			AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_ul_out);
			/* AEC */
			if (itor_lib_hanlder->pcm_dump_aec != NULL &&
			    itor_lib_hanlder->pcm_dump_aec->mFilep != NULL)
				itor_lib_hanlder->pcm_dump_aec->AudioCloseDumpPCMFile(
					itor_lib_hanlder->pcm_dump_aec);
			AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_aec);
			/* DL in */
			if (itor_lib_hanlder->pcm_dump_dl_in != NULL &&
			    itor_lib_hanlder->pcm_dump_dl_in->mFilep != NULL)
				itor_lib_hanlder->pcm_dump_dl_in->AudioCloseDumpPCMFile(
					itor_lib_hanlder->pcm_dump_dl_in);
			AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_dl_in);
			/* DL out */
			if (itor_lib_hanlder->pcm_dump_dl_out != NULL &&
			    itor_lib_hanlder->pcm_dump_dl_out->mFilep != NULL)
				itor_lib_hanlder->pcm_dump_dl_out->AudioCloseDumpPCMFile(
					itor_lib_hanlder->pcm_dump_dl_out);
			AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_dl_out);
		}
#endif
		AUDIO_FREE_POINTER(itor_lib_hanlder->linear_buf_ul);
		AUDIO_FREE_POINTER(itor_lib_hanlder->linear_buf_ul_aec);
		AUDIO_FREE_POINTER(itor_lib_hanlder->linear_buf_dl);

		deinit_pool_buf(itor_lib_hanlder);

		AUDIO_FREE_POINTER(itor_lib_hanlder->ul_smoother.base);
		AUDIO_FREE_POINTER(itor_lib_hanlder->dl_smoother.base);

		release_lib_config(&itor_lib_hanlder->lib_config);

		AUDIO_FREE_POINTER(itor_lib_hanlder->param_buf.p_buffer);

		UNLOCK_ALOCK(itor_lib_hanlder->lock);
		FREE_ALOCK(itor_lib_hanlder->lock);

		AUDIO_FREE_POINTER(itor_lib_hanlder);
	}

	*handler_list = NULL;
}



/*
 * =============================================================================
 *                     utilities implementation
 * =============================================================================
 */

static uint32_t get_size_per_channel(const audio_format_t audio_format)
{
	return (uint32_t)AUDIO_BYTES_PER_SAMPLE(audio_format); /* audio.h */
}


static uint32_t get_frame_buf_size(const audio_buf_t *audio_buf)
{
	uint32_t frame_buf_size = 0;

	if (audio_buf->frame_size_ms == 0) {
		AUD_LOG_W("frame_size_ms == 0, return");
		return 0;
	}

	frame_buf_size = (get_size_per_channel(audio_buf->audio_format) *
			  audio_buf->num_channels *
			  audio_buf->sample_rate_buffer *
			  audio_buf->frame_size_ms) / 1000;

	AUD_LOG_VV("%s() frame_size_ms %d", __FUNCTION__, audio_buf->frame_size_ms);
	AUD_LOG_VV("%s() num_channels %d", __FUNCTION__, audio_buf->num_channels);
	AUD_LOG_VV("%s() sample_rate_buffer %u", __FUNCTION__,
		   audio_buf->sample_rate_buffer);
	AUD_LOG_VV("%s() audio_format 0x%x", __FUNCTION__, audio_buf->audio_format);
	AUD_LOG_VV("%s() frame_buf_size %u", __FUNCTION__, frame_buf_size);
	AUD_ASSERT(frame_buf_size > 0);
	return frame_buf_size;
}


static void char_to_string(string_buf_t *target, char *source, uint32_t mem_sz)
{
	target->memory_size = mem_sz;
	target->string_size = strlen(source);
	target->p_string = source;

	AUD_LOG_VV("memory_size = %u", target->memory_size);
	AUD_LOG_VV("string_size = %u", target->string_size);
	AUD_LOG_VV("p_string = %s",    target->p_string);
}


static int linear_to_frame_base(char *linear_buf, char *frame_buf,
				uint32_t data_size, audio_format_t audio_format, uint8_t num_channels)
{
	if (audio_format != AUDIO_FORMAT_PCM_16_BIT &&
	    audio_format != AUDIO_FORMAT_PCM_32_BIT &&
	    audio_format != AUDIO_FORMAT_PCM_8_24_BIT) {
		AUD_LOG_E("%s(), not support audio_format 0x%x", __FUNCTION__, audio_format);
		return 0;
	}

	if (num_channels != 1 &&
	    num_channels != 2 &&
	    num_channels != 3 &&
	    num_channels != 4) {
		AUD_LOG_E("%s(), not support num_channels %d", __FUNCTION__, num_channels);
		return 0;
	}

	if ((linear_buf == NULL) || (frame_buf == NULL)) {
		AUD_LOG_E("%s(), buffer null! linear_buf: %p, frame_buf: %p",
			  __FUNCTION__, linear_buf, frame_buf);
		return 0;
	}

	if (num_channels == 1) {
		/* linear base and frame base is the same for mono */
		memcpy(frame_buf, linear_buf, data_size);
		return data_size;
	}


	if (num_channels == 2) {
		if (audio_format == AUDIO_FORMAT_PCM_16_BIT)
			LINEAR_TO_FRAME_2_CH(linear_buf, frame_buf, data_size, int16_t);
		else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
			 audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
			LINEAR_TO_FRAME_2_CH(linear_buf, frame_buf, data_size, int32_t);
	} else if (num_channels == 3) {
		if (audio_format == AUDIO_FORMAT_PCM_16_BIT)
			LINEAR_TO_FRAME_3_CH(linear_buf, frame_buf, data_size, int16_t);
		else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
			 audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
			LINEAR_TO_FRAME_3_CH(linear_buf, frame_buf, data_size, int32_t);
	} else if (num_channels == 4) {
		if (audio_format == AUDIO_FORMAT_PCM_16_BIT)
			LINEAR_TO_FRAME_4_CH(linear_buf, frame_buf, data_size, int16_t);
		else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
			 audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
			LINEAR_TO_FRAME_4_CH(linear_buf, frame_buf, data_size, int32_t);
	}

	return data_size;
}


static int frame_base_to_linear(char *frame_buf, char *linear_buf,
				uint32_t data_size, audio_format_t audio_format, uint8_t num_channels)
{
	if (audio_format != AUDIO_FORMAT_PCM_16_BIT &&
	    audio_format != AUDIO_FORMAT_PCM_32_BIT &&
	    audio_format != AUDIO_FORMAT_PCM_8_24_BIT) {
		AUD_LOG_E("%s(), not support audio_format 0x%x", __FUNCTION__, audio_format);
		return 0;
	}

	if (num_channels != 1 &&
	    num_channels != 2 &&
	    num_channels != 3 &&
	    num_channels != 4) {
		AUD_LOG_E("%s(), not support num_channels %d", __FUNCTION__, num_channels);
		return 0;
	}


	if (num_channels == 1) {
		/* linear base and frame base is the same for mono */
		memcpy(linear_buf, frame_buf, data_size);
		return data_size;
	}


	if (num_channels == 2) {
		if (audio_format == AUDIO_FORMAT_PCM_16_BIT)
			FRAME_TO_LINEAR_2_CH(frame_buf, linear_buf, data_size, int16_t);
		else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
			 audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
			FRAME_TO_LINEAR_2_CH(frame_buf, linear_buf, data_size, int32_t);
	} else if (num_channels == 3) {
		if (audio_format == AUDIO_FORMAT_PCM_16_BIT)
			FRAME_TO_LINEAR_3_CH(frame_buf, linear_buf, data_size, int16_t);
		else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
			 audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
			FRAME_TO_LINEAR_3_CH(frame_buf, linear_buf, data_size, int32_t);
	} else if (num_channels == 4) {
		if (audio_format == AUDIO_FORMAT_PCM_16_BIT)
			FRAME_TO_LINEAR_4_CH(frame_buf, linear_buf, data_size, int16_t);
		else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
			 audio_format == AUDIO_FORMAT_PCM_8_24_BIT)
			FRAME_TO_LINEAR_4_CH(frame_buf, linear_buf, data_size, int32_t);
	}

	return data_size;
}

#ifdef AURISYS_RESERVE_MEM
static bool need_lib_reserved_mem(aurisys_lib_handler_t *lib_handler, uint8_t *id) {
	uint8_t i = 0;
	bool use_reserved_mem = false;
	for (i = 0 ; i < sizeof(aurisys_reserve_mem) / sizeof(aurisys_lib_reserve_mem_t); i++) {
		if (!strcmp(aurisys_reserve_mem[i].name, lib_handler->lib_name)) {
			use_reserved_mem = true;
			break;
		}
	}
	*id = i;
	return use_reserved_mem;
}

static int alloc_lib_reserved_mem(aurisys_lib_handler_t *lib_handler, data_buf_t *working_buf) {
	uint8_t id = 0;
	if (need_lib_reserved_mem(lib_handler, &id)) {
		LOCK_ALOCK_MS(g_aurisys_reserve_mem_lock, 1000);
		if (lib_handler->arsi_process_type == ARSI_PROCESS_TYPE_DL_ONLY) { // for dl process
			AUD_ASSERT(working_buf->memory_size <= aurisys_reserve_mem[id].dl_reserve_size);
			AUD_ASSERT(dl_reserve_mem == false);
			working_buf->p_buffer = g_reserve_working_buf;
			dl_reserve_mem = true;
		} else if (lib_handler->arsi_process_type == ARSI_PROCESS_TYPE_UL_ONLY) { // for ul process
			AUD_ASSERT(working_buf->memory_size <= aurisys_reserve_mem[id].ul_reserve_size);
			AUD_ASSERT(ul_reserve_mem == false);
			working_buf->p_buffer = g_reserve_working_buf + dl_reserve_max_size;
			ul_reserve_mem = true;
		} else { // for ul + dl process
			AUD_ASSERT(working_buf->memory_size <=
				   (aurisys_reserve_mem[id].dl_reserve_size + aurisys_reserve_mem[id].ul_reserve_size));
			AUD_ASSERT(ul_reserve_mem == false);
			AUD_ASSERT(dl_reserve_mem == false);
			working_buf->p_buffer = g_reserve_working_buf;
			ul_reserve_mem = true;
			dl_reserve_mem = true;
		}
		memset(working_buf->p_buffer, 0, working_buf->memory_size);
		UNLOCK_ALOCK(g_aurisys_reserve_mem_lock);
		return 0;
	}
	return -1;
}

static int free_lib_reserved_mem(aurisys_lib_handler_t *lib_handler) {
	uint8_t id = 0;
	if (need_lib_reserved_mem(lib_handler, &id)) {
		LOCK_ALOCK_MS(g_aurisys_reserve_mem_lock, 1000);
		AUD_LOG_D("%s(), keep %s working buffer reserved, not free\n", __FUNCTION__, lib_handler->lib_name);
		if (lib_handler->arsi_process_type == ARSI_PROCESS_TYPE_DL_ONLY) {
			AUD_ASSERT(dl_reserve_mem == true);
			dl_reserve_mem = false;
		} else if (lib_handler->arsi_process_type == ARSI_PROCESS_TYPE_UL_ONLY) {
			AUD_ASSERT(ul_reserve_mem == true);
			ul_reserve_mem = false;
		} else {
			AUD_ASSERT(ul_reserve_mem == true);
			AUD_ASSERT(dl_reserve_mem == true);
			dl_reserve_mem = false;
			ul_reserve_mem = false;
		}
		UNLOCK_ALOCK(g_aurisys_reserve_mem_lock);
		return 0;
	}
	return -1;
}
#endif

static inline uint64_t get_current_time_us(void)
{
	return read_xgpt_stamp_ns() / 1000;
}

static uint64_t get_expect_process_time_us(struct audio_buf_t *p_audio_buf, uint64_t process_size)
{
	uint64_t frame_size = 0;
	if (!p_audio_buf) {
		AUD_LOG_E("%s(), p_audio_buf = NULL!\n", __FUNCTION__);
		return 0xffffffffffffffffULL;
	}

	if (p_audio_buf->frame_size_ms > 0)
		return ((uint64_t)p_audio_buf->frame_size_ms) * 1000;
	else {
		frame_size = (uint64_t)p_audio_buf->num_channels *
			     (uint64_t)get_size_per_channel(p_audio_buf->audio_format) *
			     (uint64_t)p_audio_buf->sample_rate_buffer;
		if (frame_size == 0) {
			AUD_LOG_E("%s(), frame_size = 0!\n", __FUNCTION__);
			return 0xffffffffffffffffULL;
		}
		return process_size * 1000000 / frame_size;
	}
}


#ifdef AURISYS_C2C_SYNC_SUPPORT
static bool aurisys_lib_need_sync(uint8_t task_scene)
{
#ifdef CFG_PHONE_CALL_CORE_SEPARATE
	if (task_scene == TASK_SCENE_PHONE_CALL)
		return true;
#endif
#ifdef VOIP_SEPARATE_CORE
	if (task_scene == TASK_SCENE_VOIP)
		return true;
#endif
	AUD_LOG_V("%s(), task_scene %d not support\n", __FUNCTION__, task_scene);
	return false;
}

void audio_aurisys_c2c_handler(struct adsp_c2c_ipc_msg_t *c2c_msg)
{
	struct adsp_c2c_data_t *c2c_data = (struct adsp_c2c_data_t *)c2c_msg->payload;
	uint8_t sync_lib_id = 0, sync_lib_scene = 0;

	// get lib sync info
	sync_lib_id = c2c_msg->param;
	sync_lib_scene = c2c_msg->task_scene;

	if (sync_lib_id != aurisys_c2c_sync_buf.lib_sync_info.lib_id ||
	    sync_lib_scene != aurisys_c2c_sync_buf.lib_sync_info.aurisys_scenario)
	    goto c2c_ack_back;

	// core 0 get UL to DL sync buff
	if (c2c_data->size <= aurisys_c2c_sync_buf.u2d_buf.memory_size) {
		memcpy(aurisys_c2c_sync_buf.u2d_buf.p_buffer, c2c_data->addr, c2c_data->size);
		AUDIO_CHECK(aurisys_c2c_sync_buf.u2d_buf.p_buffer);
	}
	// core 0 set DL to UL sync buff
	if (aurisys_c2c_sync_buf.d2u_buf.memory_size <=
	    aurisys_c2c_sync_buf.share_buf_d2u.memory_size)
		memcpy(aurisys_c2c_sync_buf.share_buf_d2u.p_buffer,
		       aurisys_c2c_sync_buf.d2u_buf.p_buffer,
		       aurisys_c2c_sync_buf.d2u_buf.memory_size);
	aurisys_c2c_sync_buf.updated = true;

c2c_ack_back:
	if (c2c_msg->ack_type == ADSP_C2C_MSG_NEED_ACK)
		adsp_c2c_ipc_send(
			ADSP_C2C_AURISYS_SYNC_ID,
			ADSP_C2C_MSG_ACK_BACK,
			0, 0, 0, 0, NULL, 0);
}


static void aurisys_c2c_init()
{
	uint32_t buf_size = 0;
	void *share_buf = NULL;

	if (aurisys_c2c_sync_buf.inited == true) {
		AUD_LOG_D("%s(), already init, return\n", __FUNCTION__);
		return;
	}

	adsp_c2c_registration(ADSP_C2C_AURISYS_SYNC_ID, audio_aurisys_c2c_handler);
	adsp_c2c_get_share_mem(ADSP_C2C_AURISYS_SYNC_ID, &buf_size, &share_buf);

	if (!buf_size || !share_buf) {
		AUD_LOG_E("%s(), get C2C buffer fail, size: %d, buf: %p!\n",
			  __FUNCTION__, buf_size, share_buf);
		return;
	}

	// c2c share buf
	aurisys_c2c_sync_buf.share_buf_d2u.p_buffer = share_buf;
	aurisys_c2c_sync_buf.share_buf_d2u.memory_size = (buf_size / 2);
	aurisys_c2c_sync_buf.share_buf_u2d.p_buffer = share_buf + (buf_size / 2);
	aurisys_c2c_sync_buf.share_buf_u2d.memory_size = (buf_size / 2);

	// local buf
	AUDIO_ALLOC_BUFFER(aurisys_c2c_sync_buf.d2u_buf.p_buffer, (buf_size / 2));
	aurisys_c2c_sync_buf.d2u_buf.memory_size = (buf_size / 2);
	AUDIO_ALLOC_BUFFER(aurisys_c2c_sync_buf.u2d_buf.p_buffer, (buf_size / 2));
	aurisys_c2c_sync_buf.u2d_buf.memory_size = (buf_size / 2);

	NEW_ALOCK(aurisys_c2c_sync_buf.sync_lock);
	aurisys_c2c_sync_buf.inited = true;
}
#endif


#ifdef __cplusplus
}  /* extern "C" */
#endif

