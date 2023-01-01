/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*/
#ifndef AUDIO_TASK_UTILITY_H
#define AUDIO_TASK_UTILITY_H

#include <stdio.h>
#include <audio_type.h>
#include <time.h>
#include <stdint.h>
#include <audio_ringbuf.h>
#include <audio_task_dump.h>
#include <audio_task_event.h>
#include <audio_task_checker.h>
#include <audio_task_time.h>
#include <audio_task_state.h>

#ifdef CFG_MTK_AUDIODSP_SUPPORT
#include <hal_cache.h>
#include <audio_sw_mixer.h>
#endif

#define GET_TIME_INTERVAL(stop_time_ns, start_time_ns) \
	(((stop_time_ns) > (start_time_ns)) \
	 ? (((stop_time_ns) - (start_time_ns))*77) \
	 : (((0xFFFFFFFFFFFFFFFF - (start_time_ns)) + (stop_time_ns) + 1))*77)

#define AUDIO_VOLUME_MAX (0xffff)

/* task audio buffer define  */
#define GET_HW_BUF_RATE(HW_BUF) (HW_BUF.aud_buffer.buffer_attr.rate)
#define PGET_HW_BUF_RATE(PHW_BUF) (PHW_BUF->aud_buffer.buffer_attr.rate)
#define GET_HW_BUF_CHANNEL(HW_BUF) (HW_BUF.aud_buffer.buffer_attr.channel)
#define PGET_HW_BUF_CHANNEL(PHW_BUF) (PHW_BUF->aud_buffer.buffer_attr.channel)
#define GET_HW_BUF_FORMAT(HW_BUF) (HW_BUF.aud_buffer.buffer_attr.format)
#define PGET_HW_BUF_FORMAT(PHW_BUF) (PHW_BUF->aud_buffer.buffer_attr.format)
#define GET_HW_BUF_PERIOD(HW_BUF) (HW_BUF.aud_buffer.period_size)
#define PGET_HW_BUF_PERIOD(PHW_BUF) (PHW_BUF->aud_buffer.period_size)
#define GET_HW_BUF_COUNT(HW_BUF) (HW_BUF.aud_buffer.period_count)
#define PGET_HW_BUF_COUNT(PHW_BUF) (PHW_BUF->aud_buffer.period_count)

#define LINEAR_MCH_TO_NCH(linear_buf, data_size, type, source_ch, target_ch) \
	({ \
		uint32_t __channel_size = data_size / source_ch; \
		uint32_t __num_sample = __channel_size / sizeof(type); \
		uint32_t __data_size_nch = __channel_size * target_ch; \
		type    *__linear_mch = (type *)(linear_buf); \
		type    *__linear_nch = (type *)(linear_buf); \
		uint32_t __idx_sample = 0; \
		for (__idx_sample = 0; __idx_sample < __num_sample; __idx_sample++) { \
			memmove(__linear_nch, __linear_mch, target_ch * sizeof(type)); \
			__linear_nch += target_ch; \
			__linear_mch += source_ch; \
		} \
		__data_size_nch; \
	})

enum {
	AURI_INPUTBUF      = 0x1,
	AURI_OUTPUTBUF     = 0x2,
	AURI_REFBUF        = 0x4,
	AURI_PROCESSINGBUF = 0x8,
	AURI_LINEAROUT     = 0x10,
	AURI_POOLBUF_DLOUT = 0x20,
	AURI_POOLBUF_ULOUT = 0x40,
};

enum {
	/* scene for mixer */
	SWMIXER_AUDPLAYBACK       = 0,
	SWMIXER_MUSIC_PLAYBACK    = 1,
	MIXER_BUFFER_SCENE_SIZE,
};

enum {
	/* buffer status */
	BUF_STATE_UNDERFLOW = -2,
	BUF_STATE_OVERFLOW = -1,
	BUF_STATE_NOT_ENOUGH = 0,
	BUF_STATE_ENOUGH = 1,
};

struct buf_attr;
struct RingBuf;
struct AudioTask;
struct Audio_Task_Msg_t;
struct audio_hw_buffer;
struct stream_attribute_dsp;
struct audio_task_common;
struct ipi_msg_t;
struct task_dump_inst;
struct audio_ringbuf_t;
struct sw_mixer_attr_t;

uint32_t doDownMix(void *linear_buf,
		   uint32_t data_size,
		   uint32_t audio_format,
		   uint32_t source_ch,
		   uint32_t target_ch);

#ifdef CFG_MTK_AUDIODSP_SUPPORT
/* semaphore with critical section */
int audio_semaphore_get_3way(int flag, unsigned int retry_count, unsigned int waitms);
int audio_semaphore_release_3way(int flag);
#endif

/* utility function */
int aud_task_check_state(struct AudioTask *task, unsigned int state);
unsigned int frame_to_bytes(unsigned int framecount, unsigned int channels,
			    unsigned int format);
unsigned int getframesize(unsigned int channel, unsigned int format);
unsigned int getsamplesize(unsigned int format);


/* get audio hw buffer API */
int get_audiobuf_from_msg(struct ipi_msg_t *ipi_msg,
			  struct audio_hw_buffer *task_audio_buf);
unsigned int get_audio_write_size(struct audio_hw_buffer *audio_hwbuf);
unsigned int get_audio_memif_size(struct audio_hw_buffer *audio_hwbuf);


/* volume process */
/*
 * volume range: 0x0 ~ 0xffff
 * it will base on channel and format to ramp volume from volume src ==> volume tar
 */
int volume_process(void *pInBuffer, unsigned int size, unsigned int channel, unsigned int format,
		   unsigned short volume_src, unsigned short volume_tar);
/* debug */
void DumpTaskMsg(char *appendstring, struct ipi_msg_t *ipi_msg);

/* enter point of irq handler*/
int adsp_audio_irq_entry(void);

/* condition enter write loop */
int enter_write_cond(struct AudioTask *task);
int enter_read_cond(int memif,
		    struct AudioTask *task,
		    struct audio_hw_buffer *audio_hwbuf,
		    struct audio_hw_buffer *audio_afebuf,
		    struct RingBuf *pRingBuf,
		    unsigned int processing_in_size,
		    unsigned int processing_out_size);

/* condition enter first write */
int first_write_cond(int handle,
		     struct AudioTask *task,
		     struct audio_hw_buffer *audio_hwbuf,
		     struct audio_hw_buffer *audio_afebuf,
		     struct RingBuf *pRingBuf,
		     unsigned int written_size);

/* *send message to task */
unsigned int get_queue_idx(struct AudioTask *this,
			   unsigned int max_queue_size);
unsigned int get_data_queue_idx(struct AudioTask *this,
			   unsigned int max_queue_size);


int send_task_message(struct AudioTask *this, struct ipi_msg_t *ipi_msg,
		      unsigned int max_queue_size);
int send_task_message_fromisr(struct AudioTask *this, struct ipi_msg_t *ipi_msg,
			      unsigned int max_queue_size);

bool is_no_restart(struct audio_buffer *aud_buffer);

/* only do memcy from input to output buffer */
int task_do_processing(char *inputbuffer, int inputbuffersize,
		       char *outputbuffer, int *outputbuffersize,
		       char *referencebuffer, int referencebuffersize);

/* audio and auriry fmt translate*/
uint32_t trans_sndrv_fmt_to_audio_fmt(const unsigned int sndrv_pcm_format);
uint32_t trans_audio_fmt_to_sndrv_fmt(const unsigned int audio_fmt);

/* aurisys utility */
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
unsigned int aurisys_align(struct audio_ringbuf_t *ringbuf,
			   uint32_t expect_out_size);
void audio_dsp_aurisys_dl_process(char *data_buf, uint32_t processing_size, void *param);
int audio_dsp_aurisys_process_dl_to_sink(void *buf, uint32_t size, void *param);
#endif
void cal_output_buffer_size(struct stream_attribute_dsp *attr_in,
			    struct stream_attribute_dsp *attr_out,
			    uint32_t size_in, uint32_t *size_out);
void cal_buffer_size(struct stream_attribute_dsp *attr, int *size);
void cal_hw_buffer_size(struct stream_attribute_dsp *attr,
			struct audio_hw_buffer *buf_attr, int *size);
bool is_4channel_use(struct stream_attribute_dsp *attr);

int clean_task_attr(struct audio_hw_buffer *buf_attr);
void init_swmixer_out_attr(struct buf_attr buffer_attr, int size);
void get_swmixer_out_attr(struct buf_attr *attr);

/*
 * provide swmixer cb as output path
 * now only aud_taskplayback/music return true
 */
bool sw_mixer_write_cb(int scene);
bool sw_mixer_need_attach_source(int scene);
bool sw_mixer_need_attach_target(int scene);
bool is_task_attach_sw_mixer(struct audio_task_common *task_common, int scene);

/* write this task for swxmier attach*/
uint32_t get_swmixer_prior(int scene);
uint32_t get_swmixer_source_id(int scene);
uint32_t get_swmixer_target_id(int scene);

void user_attach_sw_mixer_source(struct audio_task_common *task_common,
				 struct sw_mixer_attr_t *attr);
int user_write_sw_mixer_source(struct audio_task_common *task_common,
				void *buf, uint32_t size);
void user_detach_sw_mixer_source(struct audio_task_common *task_common);

/* write data to AFE hw, or other sink source */
int audio_dsp_write_to_sink(void *buf, uint32_t size, void *param, unsigned int *consume_time);
/* get data from source, source can be hardware or other source. */
int get_audio_capture_data(char *buf, int size, struct AudioTask *task);

#ifdef CFG_MTK_AUDIODSP_SUPPORT
/* adsp core share memory */
bool get_share_mem_core(unsigned int core_id);
int init_share_mem_core(char *memory, unsigned int size, unsigned int core_id);
int set_core_irq(unsigned int core_id, unsigned char task_scene);
int clear_core_irq(unsigned int core_id, unsigned char task_scene);
#endif
/* pulse detect */
void detect_pulse(bool enable, const char* taskname, const int pulse_level, const void *ptr, const unsigned int bytes,
		 const unsigned int channels,const unsigned int format, const unsigned int rate,
		 const unsigned int shift_bits);

/* capture pipe */
void init_capture_pipe_attr(struct audio_hw_buffer *hw_buffer);
void get_capture_pipe_attr(struct audio_hw_buffer *hw_buffer);

#endif // end of AUDIO_TASK_UTILITY_H
