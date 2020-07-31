#pragma once

#define MAX_QUEUE           8192
#define MAX_READ_SIZE       (188 * 87)

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

typedef struct thread_data
{
    int tfd;    /* tuner fd */
    int wfd;    /* output file fd */
    time_t start_time;

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
int tune(char *channel, thread_data *tdata, int dev_num);
int close_tuner(thread_data *tdata);
void calc_cn(void);
int parse_time(char *rectimestr, int *recsec);
