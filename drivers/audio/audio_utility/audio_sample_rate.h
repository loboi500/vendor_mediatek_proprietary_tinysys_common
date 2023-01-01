#ifndef AUDIO_SAMPLE_RATE_H
#define AUDIO_SAMPLE_RATE_H

#include <stdint.h>

enum {
	AUDIO_SAMPLE_RATE_BIT_8000,
	AUDIO_SAMPLE_RATE_BIT_11025,
	AUDIO_SAMPLE_RATE_BIT_12000,
	AUDIO_SAMPLE_RATE_BIT_16000,
	AUDIO_SAMPLE_RATE_BIT_22050,
	AUDIO_SAMPLE_RATE_BIT_24000,
	AUDIO_SAMPLE_RATE_BIT_32000,
	AUDIO_SAMPLE_RATE_BIT_44100,
	AUDIO_SAMPLE_RATE_BIT_48000,
	AUDIO_SAMPLE_RATE_BIT_64000,
	AUDIO_SAMPLE_RATE_BIT_88200,
	AUDIO_SAMPLE_RATE_BIT_96000,
	AUDIO_SAMPLE_RATE_BIT_128000,
	AUDIO_SAMPLE_RATE_BIT_176400,
	AUDIO_SAMPLE_RATE_BIT_192000,

	AUDIO_SAMPLE_RATE_BIT_INVALID
};


enum {
	AUDIO_SAMPLE_RATE_MASK_8000     = (1 << AUDIO_SAMPLE_RATE_BIT_8000),
	AUDIO_SAMPLE_RATE_MASK_11025    = (1 << AUDIO_SAMPLE_RATE_BIT_11025),
	AUDIO_SAMPLE_RATE_MASK_12000    = (1 << AUDIO_SAMPLE_RATE_BIT_12000),
	AUDIO_SAMPLE_RATE_MASK_16000    = (1 << AUDIO_SAMPLE_RATE_BIT_16000),
	AUDIO_SAMPLE_RATE_MASK_22050    = (1 << AUDIO_SAMPLE_RATE_BIT_22050),
	AUDIO_SAMPLE_RATE_MASK_24000    = (1 << AUDIO_SAMPLE_RATE_BIT_24000),
	AUDIO_SAMPLE_RATE_MASK_32000    = (1 << AUDIO_SAMPLE_RATE_BIT_32000),
	AUDIO_SAMPLE_RATE_MASK_44100    = (1 << AUDIO_SAMPLE_RATE_BIT_44100),
	AUDIO_SAMPLE_RATE_MASK_48000    = (1 << AUDIO_SAMPLE_RATE_BIT_48000),
	AUDIO_SAMPLE_RATE_MASK_64000    = (1 << AUDIO_SAMPLE_RATE_BIT_64000),
	AUDIO_SAMPLE_RATE_MASK_88200    = (1 << AUDIO_SAMPLE_RATE_BIT_88200),
	AUDIO_SAMPLE_RATE_MASK_96000    = (1 << AUDIO_SAMPLE_RATE_BIT_96000),
	AUDIO_SAMPLE_RATE_MASK_128000   = (1 << AUDIO_SAMPLE_RATE_BIT_128000),
	AUDIO_SAMPLE_RATE_MASK_176400   = (1 << AUDIO_SAMPLE_RATE_BIT_176400),
	AUDIO_SAMPLE_RATE_MASK_192000   = (1 << AUDIO_SAMPLE_RATE_BIT_192000),

	AUDIO_SAMPLE_RATE_MASK_INVALID  = (1 << AUDIO_SAMPLE_RATE_BIT_INVALID)
};

typedef uint32_t audio_sample_rate_mask_t;



audio_sample_rate_mask_t audio_sample_rate_string_to_masks(const char *string);

audio_sample_rate_mask_t audio_sample_rate_num_to_mask(const uint32_t rate);

uint32_t audio_sample_rate_mask_to_num(const audio_sample_rate_mask_t mask);

uint32_t audio_sample_rate_get_max_rate(const audio_sample_rate_mask_t masks);

uint32_t audio_sample_rate_get_match_rate(
	const audio_sample_rate_mask_t masks,
	const uint32_t rate);


#endif /* end of AUDIO_SAMPLE_RATE_H */

