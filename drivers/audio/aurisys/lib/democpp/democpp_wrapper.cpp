#include <stdio.h>
#include <memory>

#include <arsi_api.h> // implement MTK AURISYS API
#include <wrapped_audio.h>


#include "DemoCpp.h"

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

void democpp_arsi_assign_lib_fp(AurisysLibInterface *lib);

#ifdef __cplusplus
}
#endif

static debug_log_fp_t print_log;

//static DemoCpp* gInstance = new DemoCpp();
static DemoCpp* gInstance = NULL;


DemoCpp* getDemoCpp() {
    if (gInstance == NULL) {
        gInstance = new DemoCpp();
    }

    return gInstance;
}


status_t democpp_arsi_query_working_buf_size(
	const arsi_task_config_t *p_arsi_task_config,
	const arsi_lib_config_t  *p_arsi_lib_config,
	uint32_t                 *p_working_buf_size,
	const debug_log_fp_t      debug_log_fp)
{
	if (p_arsi_task_config == NULL ||
	    p_arsi_lib_config == NULL ||
	    p_working_buf_size == NULL ||
	    debug_log_fp == NULL)
		return BAD_VALUE;
        // buffer size to avoid assert
	*p_working_buf_size = sizeof(int);

	return NO_ERROR;
}

status_t democpp_arsi_create_handler(
	const arsi_task_config_t *p_arsi_task_config,
	const arsi_lib_config_t  *p_arsi_lib_config,
	const data_buf_t         *p_param_buf,
	data_buf_t               *p_working_buf,
	void                    **pp_handler,
	const debug_log_fp_t      debug_log_fp)
{
	if (p_arsi_task_config == NULL ||
	    p_arsi_lib_config == NULL ||
	    p_param_buf == NULL ||
	    p_working_buf == NULL ||
	    pp_handler == NULL ||
	    debug_log_fp == NULL)
		return BAD_VALUE;

	unsigned int channel;
	unsigned int samplerate;

	/*set attribute*/
	channel = p_arsi_task_config->output_device_info.num_channels;
	samplerate = p_arsi_task_config->output_device_info.sample_rate;

	if (getDemoCpp()->open(channel, samplerate) != 0) {
		debug_log_fp("[DemoCpp] Fail to open\n");
		return NO_INIT;
	}
	*pp_handler = getDemoCpp();

	return NO_ERROR;
}


status_t democpp_arsi_process_ul_buf(
	audio_buf_t *p_ul_buf_in,
	audio_buf_t *p_ul_buf_out,
	audio_buf_t *p_ul_ref_bufs,
	data_buf_t  *p_debug_dump_buf,
	void        *p_handler)
{

	if (p_ul_buf_in == NULL ||
	    p_ul_buf_out == NULL ||
	    p_handler == NULL)
		return NO_INIT;

	return NO_ERROR;
}


status_t democpp_arsi_process_dl_buf(
	audio_buf_t *p_dl_buf_in,
	audio_buf_t *p_dl_buf_out,
	audio_buf_t *p_dl_ref_bufs,
	data_buf_t  *p_debug_dump_buf,
	void        *p_handler)
{

	if (p_dl_buf_in == NULL ||
	    p_dl_buf_out == NULL ||
	    p_handler == NULL)
		return NO_INIT;

        getDemoCpp()->process(p_dl_buf_in->data_buf.p_buffer, p_dl_buf_in->data_buf.data_size,
                               p_dl_buf_out->data_buf.p_buffer, p_dl_buf_out->data_buf.memory_size);
	p_dl_buf_out->data_buf.data_size = p_dl_buf_in->data_buf.data_size;
	p_dl_buf_in->data_buf.data_size = 0;

	print_log("- DL democpp: in->data_buf.p_buffer = %p in->data_buf.data_size = %d, out->data_buf.p_buffer = %p, out->data_buf.data_size = %d\n",
		  p_dl_buf_in->data_buf.p_buffer,
		  p_dl_buf_in->data_buf.data_size,
		  p_dl_buf_out->data_buf.p_buffer,
		  p_dl_buf_out->data_buf.data_size
		 );
	return NO_ERROR;
}


status_t democpp_arsi_reset_handler(
	const arsi_task_config_t *p_arsi_task_config,
	const arsi_lib_config_t  *p_arsi_lib_config,
	const data_buf_t         *p_param_buf,
	void                     *p_handler)
{

	if (p_arsi_task_config == NULL ||
	    p_arsi_lib_config == NULL ||
	    p_param_buf == NULL ||
	    p_handler == NULL)
		return NO_INIT;

	unsigned int channel;
	unsigned int samplerate;
	if (getDemoCpp()->getStatus() != DEMO_STATUS_OPEN)
		return NO_INIT;

	/*set attribute*/
	channel = p_arsi_task_config->output_device_info.num_channels;
	samplerate = p_arsi_task_config->output_device_info.sample_rate;

	getDemoCpp()->config(channel, samplerate);
	print_log("%s(), done\n", __func__);

	return NO_ERROR;
}


status_t democpp_arsi_destroy_handler(void *p_handler)
{
        getDemoCpp()->close();
        delete gInstance;
        gInstance = NULL;
	print_log("%s(), p_handler = %p\n", __func__, p_handler);

	return NO_ERROR;
}

status_t democpp_arsi_set_debug_log_fp(const debug_log_fp_t debug_log,
					 void *p_handler)
{
	if (p_handler == NULL)
		return NO_INIT;

	print_log = debug_log;

	return NO_ERROR;
}

/*
status_t democpp_arsi_get_lib_version(string_buf_t *version_buf) {

    return NO_ERROR;
}
*/

status_t democpp_query_process_unit_bytes(uint32_t *p_uplink_bytes,
					    uint32_t *p_downlink_bytes,
					    void     *p_handler)
{

	if (p_uplink_bytes == NULL ||
	    p_downlink_bytes == NULL ||
	    p_handler == NULL)
		return BAD_VALUE;

	*p_uplink_bytes = 0; // no uplink process
	*p_downlink_bytes = 0; // no need specific process size
	return NO_ERROR;
}

/**
* Only implement DL processing and the necessary functions now
**/
void democpp_arsi_assign_lib_fp(AurisysLibInterface *lib)
{
	lib->arsi_get_lib_version = NULL;//democpp_arsi_get_lib_version;
	lib->arsi_query_working_buf_size = democpp_arsi_query_working_buf_size;
	lib->arsi_create_handler = democpp_arsi_create_handler;
	lib->arsi_process_dl_buf = democpp_arsi_process_dl_buf;
	lib->arsi_reset_handler = democpp_arsi_reset_handler;
	lib->arsi_destroy_handler = democpp_arsi_destroy_handler;
	lib->arsi_set_debug_log_fp = democpp_arsi_set_debug_log_fp;
	lib->arsi_query_process_unit_bytes = democpp_query_process_unit_bytes;
}
