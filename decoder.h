#pragma once

#include <stdint.h>
#include "config.h"

#include "b1/arib_std_b1.h"
#include "b1/b_cas_card.h"

typedef struct decoder {
    ARIB_STD_B1 *b1;
    B_CAS_CARD *skapa;
} decoder;

typedef struct decoder_options {
    int round;
    int strip;
} decoder_options;

/* prototypes */

decoder *b1_startup(decoder_options *opt);
int b1_shutdown(decoder *dec);
int b1_decode(decoder *dec, ARIB_STD_B1_BUFFER *sbuf, ARIB_STD_B1_BUFFER *dbuf);
int b1_finish(decoder *dec, ARIB_STD_B1_BUFFER *sbuf, ARIB_STD_B1_BUFFER *dbuf);
