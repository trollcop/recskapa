/* -*- tab-width: 4; indent-tabs-mode: nil -*- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>

#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "config.h"
#include "decoder.h"
#include "recskapa.h"
#include "mkpath.h"

QUEUE_T *create_queue(size_t size)
{
    QUEUE_T *p_queue;
    int memsize = sizeof(QUEUE_T) + size * sizeof(BUFSZ *);

    p_queue = (QUEUE_T *)calloc(memsize, sizeof(char));

    if (p_queue != NULL) {
        p_queue->size = size;
        p_queue->num_avail = size;
        p_queue->num_used = 0;
        pthread_mutex_init(&p_queue->mutex, NULL);
        pthread_cond_init(&p_queue->cond_avail, NULL);
        pthread_cond_init(&p_queue->cond_used, NULL);
    }

    return p_queue;
}

void destroy_queue(QUEUE_T *p_queue)
{
    if (!p_queue)
        return;

    pthread_mutex_destroy(&p_queue->mutex);
    pthread_cond_destroy(&p_queue->cond_avail);
    pthread_cond_destroy(&p_queue->cond_used);
    free(p_queue);
}

/* enqueue data. this function will block if queue is full. */
void enqueue(QUEUE_T *p_queue, BUFSZ *data)
{
    struct timeval now;
    struct timespec spec;
    int retry_count = 0;

    pthread_mutex_lock(&p_queue->mutex);
    /* entered critical section */

    /* wait while queue is full */
    while (p_queue->num_avail == 0) {

        gettimeofday(&now, NULL);
        spec.tv_sec = now.tv_sec + 1;
        spec.tv_nsec = now.tv_usec * 1000;

        pthread_cond_timedwait(&p_queue->cond_avail, &p_queue->mutex, &spec);
        retry_count++;
        if (retry_count > 60)
            f_exit = true;
        if (f_exit) {
            pthread_mutex_unlock(&p_queue->mutex);
            return;
        }
    }

    p_queue->buffer[p_queue->in] = data;

    /* move position marker for input to next position */
    p_queue->in++;
    p_queue->in %= p_queue->size;

    /* update counters */
    p_queue->num_avail--;
    p_queue->num_used++;

    /* leaving critical section */
    pthread_mutex_unlock(&p_queue->mutex);
    pthread_cond_signal(&p_queue->cond_used);
}

/* dequeue data. this function will block if queue is empty. */
BUFSZ *dequeue(QUEUE_T *p_queue)
{
    struct timeval now;
    struct timespec spec;
    BUFSZ *buffer;
    int retry_count = 0;

    pthread_mutex_lock(&p_queue->mutex);
    /* entered the critical section*/

    /* wait while queue is empty */
    while (p_queue->num_used == 0) {

        gettimeofday(&now, NULL);
        spec.tv_sec = now.tv_sec + 1;
        spec.tv_nsec = now.tv_usec * 1000;

        pthread_cond_timedwait(&p_queue->cond_used, &p_queue->mutex, &spec);
        retry_count++;
        if (retry_count > 60)
            f_exit = true;
        if (f_exit) {
            pthread_mutex_unlock(&p_queue->mutex);
            return NULL;
        }
    }

    /* take buffer address */
    buffer = p_queue->buffer[p_queue->out];

    /* move position marker for output to next position */
    p_queue->out++;
    p_queue->out %= p_queue->size;

    /* update counters */
    p_queue->num_avail++;
    p_queue->num_used--;

    /* leaving the critical section */
    pthread_mutex_unlock(&p_queue->mutex);
    pthread_cond_signal(&p_queue->cond_avail);

    return buffer;
}

/* this function will be reader thread */
void *reader_func(void *p)
{
    thread_data *tdata = (thread_data *)p;
    QUEUE_T *p_queue = tdata->queue;
    decoder *dec = tdata->decoder;
    int wfd = tdata->wfd;
    bool use_b1 = dec ? true : false;
    bool fileless = false;
    pthread_t signal_thread = tdata->signal_thread;
    BUFSZ *qbuf;
    ARIB_STD_B1_BUFFER sbuf, dbuf, buf;
    int code;

    buf.size = 0;
    buf.data = NULL;

    if (wfd == -1)
        fileless = true;

    while (1) {
        ssize_t wc = 0;
        int file_err = 0;
        qbuf = dequeue(p_queue);

        /* no entry in the queue */
        if (qbuf == NULL)
            break;

        sbuf.data = qbuf->buffer;
        sbuf.size = qbuf->size;

        buf = sbuf; /* default */

        if (use_b1) {
            code = b1_decode(dec, &sbuf, &dbuf);
            if (code < 0) {
                fprintf(stderr, "b1_decode failed (code=%d). fall back to encrypted recording.\n", code);
                use_b1 = false;
            }
            else
                buf = dbuf;
        }

        if (!fileless) {
            /* write data to output file */
            wc = write(wfd, buf.data, buf.size);
            if (wc < 0) {
                perror("write");
                file_err = 1;
                pthread_kill(signal_thread, errno == EPIPE ? SIGPIPE : SIGUSR2);
                break;
            }
        }

        free(qbuf);
        qbuf = NULL;

        /* normal exit */
        if ((f_exit && !p_queue->num_used) || file_err) {

            buf = sbuf; /* default */

            if (use_b1) {
                code = b1_finish(dec, &sbuf, &dbuf);
                if (code < 0)
                    fprintf(stderr, "b1_finish failed\n");
                else
                    buf = dbuf;
            }

            if (!fileless && !file_err) {
                wc = write(wfd, buf.data, buf.size);
                if (wc < 0) {
                    perror("write");
                    file_err = 1;
                    pthread_kill(signal_thread, errno == EPIPE ? SIGPIPE : SIGUSR2);
                }
            }

            break;
        }
    }

    time_t cur_time;
    time(&cur_time);
    fprintf(stderr, "Recorded %dsec\n", (int)(cur_time - tdata->start_time));

    return NULL;
}

void show_usage(char *cmd)
{
    fprintf(stderr, "Usage: \n%s [--b1 [--round N] [--strip]] [--adapter index] [--freq 12688] [--satellite JCSAT3A|JCSAT4B] [--pol H|V] [channel] rectime destfile\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Remarks:\n");
    fprintf(stderr, "if freq, satellite and pol are specified, channel is optional\n");
    fprintf(stderr, "if rectime  is '-', records indefinitely.\n");
    fprintf(stderr, "if destfile is '-', stdout is used for output.\n");
}

void show_options(void)
{
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "--b1:                Decrypt using SKAPA card\n");
    fprintf(stderr, "  --round N:         Specify round number\n");
    fprintf(stderr, "  --strip:           Strip null stream\n");
    fprintf(stderr, "--adapter N:         Use DVB device /dev/dvb/adapterN\n");
    fprintf(stderr, "--satellite sat:     Specify Satellite to use (JCSAT3A, JCSAT4B)\n");
    fprintf(stderr, "--pol polarity:      Specify polarity for tune (H or V)\n");
    fprintf(stderr, "--freq frequency:    Specify frequency for tune (ex. 12688)\n");
    fprintf(stderr, "--help:              Show this help\n");
    fprintf(stderr, "--version:           Show version\n");
}

void cleanup(thread_data *tdata)
{
    f_exit = true;

    pthread_cond_signal(&tdata->queue->cond_avail);
    pthread_cond_signal(&tdata->queue->cond_used);
}

/* will be signal handler thread */
void *process_signals(void *t)
{
    sigset_t waitset;
    int sig;
    thread_data *tdata = (thread_data *)t;

    sigemptyset(&waitset);
    sigaddset(&waitset, SIGPIPE);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGTERM);
    sigaddset(&waitset, SIGUSR1);
    sigaddset(&waitset, SIGUSR2);

    sigwait(&waitset, &sig);

    switch(sig) {
    case SIGPIPE:
        fprintf(stderr, "\nSIGPIPE received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGINT:
        fprintf(stderr, "\nSIGINT received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGTERM:
        fprintf(stderr, "\nSIGTERM received. cleaning up...\n");
        cleanup(tdata);
        break;
    case SIGUSR1: /* normal exit*/
        cleanup(tdata);
        break;
    case SIGUSR2: /* error */
        fprintf(stderr, "Detected an error. cleaning up...\n");
        cleanup(tdata);
        break;
    }

    return NULL; /* dummy */
}

void init_signal_handlers(pthread_t *signal_thread, thread_data *tdata)
{
    sigset_t blockset;

    sigemptyset(&blockset);
    sigaddset(&blockset, SIGPIPE);
    sigaddset(&blockset, SIGINT);
    sigaddset(&blockset, SIGTERM);
    sigaddset(&blockset, SIGUSR1);
    sigaddset(&blockset, SIGUSR2);

    if (pthread_sigmask(SIG_BLOCK, &blockset, NULL))
        fprintf(stderr, "pthread_sigmask() failed.\n");

    pthread_create(signal_thread, NULL, process_signals, tdata);
}

static int parse_polarity(const char pol)
{
    switch (toupper(pol)) {
        case 'H':
            return 0;
            break;

        case 'V':
            return 1;
            break;
    }

    /* Error */
    return -1;
}

static int parse_satellite(const char *sat)
{
    if (strcasecmp(sat, "JCSAT4B") == 0)
        return 1; /* Tone on 124E */
    else if (strcasecmp(sat, "JCSAT3A") == 0)
        return 0; /* No tone on 128E */

    /* Error */
    return -1;
}

int main(int argc, char **argv)
{
    time_t cur_time;
    pthread_t signal_thread;
    pthread_t reader_thread;
    QUEUE_T *p_queue = create_queue(MAX_QUEUE);
    BUFSZ *bufptr;
    decoder *decoder = NULL;
    channel_mask_e mask = CH_NONE;
    static thread_data tdata;
    decoder_options dopt = {
        4,  /* round */
        0,  /* strip */
    };
    tdata.dopt = &dopt;
    tdata.tfd = -1;
    char *pch = NULL;
    /* If not enough tuning data is specified, we need "channel name", otherwise, direct tune */
    int arg_count = 3;

    int result;
    int option_index;
    struct option long_options[] = {
        { "adapter",   1, NULL, 'a'},
        { "b1",        0, NULL, 'b'},
        { "round",     1, NULL, 'r'},
        { "strip",     0, NULL, 's'},
        { "satellite", 1, NULL, 'l'},
        { "freq",      1, NULL, 'f'},
        { "pol",       1, NULL, 'p'},
        { "help",      0, NULL, 'h'},
        { "version",   0, NULL, 'v'},
        {0, 0, NULL, 0} /* terminate */
    };

    bool use_b1 = false;
    bool use_stdout = false;
    int dev_num = 0;

    strncpy(chanfile, "/etc/skapa.conf", sizeof(chanfile) - 1);

    while ((result = getopt_long(argc, argv, "a:bc:r:shvl:f:p:", long_options, &option_index)) != -1) {
        switch (result) {
        case 'a':
            dev_num = atoi(optarg);
            fprintf(stderr, "using device: /dev/dvb/adapter%d\n", dev_num);
            break;
        case 'b':
            use_b1 = true;
            fprintf(stderr, "using B1...\n");
            break;
        case 'c':
            strncpy(chanfile, optarg, sizeof(chanfile) - 1);
            break;
        case 'f':
            /* Get frequency */
            tdata.freq = strtoul(optarg, NULL, 0);
            mask |= CH_FREQ;
            break;
        case 'p':
            /* Get polarity */
            tdata.polarity = parse_polarity(optarg[0]);
            if (tdata.polarity < 0) {
                fprintf(stderr, "Invalid argument (%s) for --pol, must be either H or V\n", optarg);
                exit(-1);
            }
            mask |= CH_POL;
            break;
        case 's':
            dopt.strip = true;
            fprintf(stderr, "enable TS NULL strip\n");
            break;
        case 'h':
            fprintf(stderr, "\n");
            show_usage(argv[0]);
            fprintf(stderr, "\n");
            show_options();
            fprintf(stderr, "\n");
            exit(0);
            break;
        case 'v':
            fprintf(stderr, "%s %s\n", argv[0], version);
            fprintf(stderr, "recorder command for DVB tuner.\n");
            exit(0);
            break;
        case 'l':
            /* Get tone (actually satellite name) */
            tdata.tone = parse_satellite(optarg);
            if (tdata.tone < 0) {
                fprintf(stderr, "Invalid argument (%s) for --satellite, must be either 'JCSAT4B' or 'JCSAT3A'\n", optarg);
                exit(-2);
            }
            mask |= CH_TONE;
            break;
        case 'r':
            dopt.round = atoi(optarg);
            fprintf(stderr, "set round %d\n", dopt.round);
            break;
        }
    }

    /* Check if we have enough stuff to tune directly */
    if (mask == CH_ALL) {
        fprintf(stderr, "All tuning info provided, not using channel name\n");
        arg_count = 2;
    }

    if (argc - optind < arg_count) {
        fprintf(stderr, "Some required parameters are missing!\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    fprintf(stderr, "pid = %d\n", getpid());

    /* If we're here, the next argument is channel name, or else hacky? decrease optind */
    if (arg_count == 3)
        pch = argv[optind];
    else
        optind--;

    /* tune */
    if (tune(pch, &tdata, dev_num) != 0)
        return 1;

    /* set recsec */
    if (parse_time(argv[optind + 1], &tdata.recsec) != 0) // no other thread --yaz
        return 1;

    if (tdata.recsec == -1)
        tdata.indefinite = true;

    /* open output file */
    char *destfile = argv[optind + 2];
    if (destfile && !strcmp("-", destfile)) {
        use_stdout = true;
        tdata.wfd = 1; /* stdout */
    } else {
        int status;
        char *path = strdup(argv[optind + 2]);
        char *dir = dirname(path);
        status = mkpath(dir, 0777);
        if (status == -1)
            perror("mkpath");
        free(path);

        tdata.wfd = open(argv[optind + 2], (O_RDWR | O_CREAT | O_TRUNC), 0666);
        if (tdata.wfd < 0) {
            fprintf(stderr, "Cannot open output file: %s\n",
                argv[optind + 2]);
            return 1;
        }
    }

    /* initialize decoder */
    if (use_b1) {
        decoder = b1_startup(&dopt);
        if (!decoder) {
            fprintf(stderr, "Cannot start b1 decoder\n");
            fprintf(stderr, "Fall back to encrypted recording\n");
            use_b1 = false;
        }
    }

    /* prepare thread data */
    tdata.queue = p_queue;
    tdata.decoder = decoder;

    /* spawn signal handler thread */
    init_signal_handlers(&signal_thread, &tdata);

    /* spawn reader thread */
    tdata.signal_thread = signal_thread;
    pthread_create(&reader_thread, NULL, reader_func, &tdata);

    fprintf(stderr, "\nRecording...\n");

    time(&tdata.start_time);

    /* read from tuner */
    while (1) {
        if (f_exit)
            break;

        time(&cur_time);
        bufptr = malloc(sizeof(BUFSZ));
        if (!bufptr) {
            f_exit = true;
            break;
        }
        bufptr->size = read(tdata.tfd, bufptr->buffer, MAX_READ_SIZE);
        if (bufptr->size <= 0) {
            if ((cur_time - tdata.start_time) >= tdata.recsec && !tdata.indefinite) {
                f_exit = true;
                enqueue(p_queue, NULL);
                break;
            } else {
                free(bufptr);
                continue;
            }
        }
        enqueue(p_queue, bufptr);

        /* stop recording */
        time(&cur_time);
        if ((cur_time - tdata.start_time) >= tdata.recsec && !tdata.indefinite)
            break;
    }

    pthread_kill(signal_thread, SIGUSR1);

    /* wait for threads */
    pthread_join(reader_thread, NULL);
    pthread_join(signal_thread, NULL);

    /* close tuner */
    if (close_tuner(&tdata) != 0)
        return 1;

    /* release queue */
    destroy_queue(p_queue);
    /* close output file */
	if (!use_stdout) {
        fsync(tdata.wfd);
        close(tdata.wfd);
	}

    /* release decoder */
    if (use_b1)
        b1_shutdown(decoder);

    return 0;
}
