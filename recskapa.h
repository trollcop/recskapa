#pragma once

#define MAX_QUEUE           8192
#define MAX_READ_SIZE       (188 * 87)

#define SKAPA_LO (11200)

typedef struct _BUFSZ {
    int size;
    uint8_t buffer[MAX_READ_SIZE];
} BUFSZ;

typedef struct _QUEUE_T
{
    unsigned int in;            // input index
    unsigned int out;           // output index
    size_t size;                // queue size
    size_t num_avail;           // available slots
    size_t num_used;            // used slots
    pthread_mutex_t mutex;
    pthread_cond_t cond_avail;  // available condition
    pthread_cond_t cond_used;   // used condition
    BUFSZ *buffer[1];           // buffers
} QUEUE_T;

/* This is used to determine if all tuning details are specified on command line, thus not needing a channel file */
typedef enum channel_mask_e
{
    CH_NONE = 0,
    CH_FREQ = 1 << 0,
    CH_POL = 1 << 1,
    CH_TONE = 1 << 2,
    CH_ALL = 0x7

} channel_mask_e;

typedef struct thread_data
{
    int tfd;    /* tuner fd */
    int wfd;    /* output file fd */
    time_t start_time;

    /* tuning data passed from main thread when not using channel file */
    int freq;
    int lo_freq;
    int polarity;
    int tone;
    bool hikari;

    int recsec;

    bool indefinite;

    QUEUE_T *queue; //invariable
    pthread_t signal_thread; //invariable
    decoder *decoder; //invariable
    decoder_options *dopt; //invariable
} thread_data;

extern const char *version;
extern char chanfile[256];

extern bool f_exit;

/* prototypes */
int tune(char *channel, thread_data *tdata, int dev_num, int dev_frontend);
int close_tuner(thread_data *tdata);
void calc_cn(void);
int parse_time(char *rectimestr, int *recsec);
