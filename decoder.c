#include <stdlib.h>
#include <stdio.h>

#include "decoder.h"

decoder *b1_startup(decoder_options *opt)
{
    decoder *dec = calloc(1, sizeof(decoder));
    int code;
    const char *err = NULL;

    dec->b1 = create_arib_std_b1();
    if (!dec->b1) {
        err = "create_arib_std_b1 failed";
        goto error;
    }

    code = dec->b1->set_multi2_round(dec->b1, opt->round);
    if (code < 0) {
        err = "set_multi2_round failed";
        goto error;
    }

    code = dec->b1->set_strip(dec->b1, opt->strip);
    if (code < 0) {
        err = "set_strip failed";
        goto error;
    }

    dec->skapa = create_b_cas_card();
    if (!dec->skapa) {
        err = "create_b_cas_card failed";
        goto error;
    }
    code = dec->skapa->init(dec->skapa);
    if (code < 0) {
        err = "skapa->init failed";
        goto error;
    }

    code = dec->b1->set_b_cas_card(dec->b1, dec->skapa);
    if (code < 0) {
        err = "set_b_cas_card failed";
        goto error;
    }

    return dec;

error:
    fprintf(stderr, "%s\n", err);
    free(dec);
    return NULL;
}

int b1_shutdown(decoder *dec)
{
    dec->b1->release(dec->b1);
    dec->skapa->release(dec->skapa);
    free(dec);

    return 0;
}

int b1_decode(decoder *dec, ARIB_STD_B1_BUFFER *sbuf, ARIB_STD_B1_BUFFER *dbuf)
{
    int code;

    code = dec->b1->put(dec->b1, sbuf);
    if (code < 0) {
        fprintf(stderr, "b1->put failed\n");
        return code;
    }

    code = dec->b1->get(dec->b1, dbuf);
    if (code < 0) {
        fprintf(stderr, "b1->get failed\n");
        return code;
    }

    return code;
}

int b1_finish(decoder *dec, ARIB_STD_B1_BUFFER *sbuf, ARIB_STD_B1_BUFFER *dbuf)
{
    int code;

    code = dec->b1->flush(dec->b1);
    if (code < 0) {
        fprintf(stderr, "b1->flush failed\n");
        return code;
    }

    code = dec->b1->get(dec->b1, dbuf);
    if (code < 0) {
        fprintf(stderr, "b1->get failed\n");
        return code;
    }

    return code;
}
