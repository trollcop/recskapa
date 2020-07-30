#ifndef _RECPT1_UTIL_H_
#define _RECPT1_UTIL_H_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

#include "config.h"
#include "decoder.h"
#include "recpt1.h"
#include "mkpath.h"

/* ipc message size */
#define MSGSZ     255

/* used in checksigna.c */
#define MAX_RETRY (2)

typedef struct msgbuf {
    long    mtype;
    char    mtext[MSGSZ];
} message_buf;

typedef struct thread_data {
    int tfd;    /* tuner fd */ //xxx variable
    int wfd;    /* output file fd */ //invariable
    int msqid; //invariable
    time_t start_time; //invariable

    int recsec; //xxx variable

    bool indefinite; //invaliable
    bool tune_persistent; //invaliable

    QUEUE_T *queue; //invariable
    pthread_t signal_thread; //invariable
    decoder *decoder; //invariable
    decoder_options *dopt; //invariable
} thread_data;

extern const char *version;
extern char chanfile[256];

extern bool f_exit;

/* prototypes */
int tune(char *channel, thread_data *tdata, int dev_num);
int close_tuner(thread_data *tdata);
void calc_cn(void);
int parse_time(char *rectimestr, int *recsec);
void do_bell(int bell);

#endif
