#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/poll.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include <sys/ioctl.h>

#include "decoder.h"
#include "recskapa.h"
#include "version.h"

#define SKAPA_LO (11200)

static bool read_channels(char *filename, char *channel, int *freq, int *polarity, int *tone, int *symbol_rate, int *delsys);

/* globals */
bool f_exit = false;
char chanfile[256];

static int fefd = 0;
static int dmxfd = 0;

static int dvb_voltage_tone(int fd, int pol_vert, int tone)
{
    fe_sec_voltage_t v = pol_vert ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
    fe_sec_tone_mode_t t = tone ? SEC_TONE_ON : SEC_TONE_OFF;

    if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
        perror("FE_SET_TONE failed");
    if (ioctl(fd, FE_SET_VOLTAGE, v) == -1)
        perror("FE_SET_VOLTAGE failed");
    usleep(15 * 1000);
    if (ioctl(fd, FE_SET_TONE, t) == -1)
        perror("FE_SET_TONE failed");

    return 1;
}

int close_tuner(thread_data *tdata)
{
    int rv = 0;

    if (fefd > 0) {
      close(fefd);
      fefd = 0;
    }
    if (dmxfd > 0) {
      close(dmxfd);
      dmxfd = 0;
    }
    if (tdata->tfd == -1)
        return rv;

    close(tdata->tfd);
    tdata->tfd = -1;

    return rv;
}

void calc_cn(void)
{
      int strength=0;
      ioctl(fefd, FE_READ_SNR, &strength);
      fprintf(stderr,"SNR: %d\n",strength);
}

int parse_time(char *rectimestr, int *recsec)
{
    /* indefinite */
    if (!strcmp("-", rectimestr)) {
        *recsec = -1;
        return 0;
    } else if (strchr(rectimestr, ':')) {
        /* colon */
        int n1, n2, n3;
        if (sscanf(rectimestr, "%d:%d:%d", &n1, &n2, &n3) == 3)
            *recsec = n1 * 3600 + n2 * 60 + n3;
        else if (sscanf(rectimestr, "%d:%d", &n1, &n2) == 2)
            *recsec = n1 * 3600 + n2 * 60;
        else
            return 1; /* unsuccessful */

        return 0;
    } else {
        /* HMS */
        char *tmpstr;
        char *p1, *p2;
        int  flag;

        if (*rectimestr == '-') {
            rectimestr++;
            flag = 1;
        } else
	        flag = 0;
        tmpstr = strdup(rectimestr);
        p1 = tmpstr;
        while (*p1 && !isdigit(*p1))
            p1++;

        /* hour */
        if ((p2 = strchr(p1, 'H')) || (p2 = strchr(p1, 'h'))) {
            *p2 = '\0';
            *recsec += atoi(p1) * 3600;
            p1 = p2 + 1;
            while(*p1 && !isdigit(*p1))
                p1++;
        }

        /* minute */
        if ((p2 = strchr(p1, 'M')) || (p2 = strchr(p1, 'm'))) {
            *p2 = '\0';
            *recsec += atoi(p1) * 60;
            p1 = p2 + 1;
            while (*p1 && !isdigit(*p1))
                p1++;
        }

        /* second */
        *recsec += atoi(p1);
        if (flag)
        	*recsec *= -1;

        free(tmpstr);

        return 0;
    } /* else */

    return 1; /* unsuccessful */
}

/*

JCSAT 3A    (128) No tone
JCSAT 4B    (124) Tone

File format:

CH596:12643:H:23303
CH634:12718:VT:23303
CH641:12523:H:23303
CH669:12718:V:23303
Promo:12628:V1:21096

Name : Frequency : Options : Symbol Rate

Options:
H   Horizontal
V   Vertical
T   Tone (tune to 124, default tune to 128)
1   Set DELSYS to DVB-S (default DVB-S2)

*/

static bool read_channels(char *filename, char *channel, int *freq, int *polarity, int *tone, int *symbol_rate, int *delsys)
{
    FILE *cfp;
    int line = 0;
    char buf[4096];
    char *field, *tmp;

    // check invalid args
    if (!filename || !channel || !freq || !polarity || !tone || !symbol_rate || !delsys)
        return false;

    line = 0;
    if (!(cfp = fopen(filename, "r"))) {
        fprintf(stderr, "error opening channel list '%s': %d %m\n", filename, errno);
        return false;
    }

    // set defaults
    *delsys = SYS_DVBS2;
    *tone = 0;
    *polarity = 0;
    *freq = 0;

    while (!feof(cfp)) {
        if (fgets(buf, sizeof(buf), cfp)) {
            line++;

            // comment
            if (buf[0] == '#')
                continue;

            tmp = buf;
            field = strsep(&tmp, ":");

            if (!field)
                goto syntax_err;

            // wrong channel name
            if (strcasecmp(channel, field) != 0)
                continue;

            // parse freq
            if (!(field = strsep(&tmp, ":")))
                goto syntax_err;
            *freq = strtoul(field, NULL, 0);

            // parse polarity
            if (!(field = strsep(&tmp, ":")))
                goto syntax_err;

            while (field && *field) {
                switch (toupper(*field)) {
                case 'H':
                    *polarity = 0;
                    field++;
                    break;

                case 'V':
                    *polarity = 1;
                    field++;
                    break;

                case 'T':
                    *tone = 1;
                    field++;
                    break;

                case '1':
                    *delsys = SYS_DVBS;
                    field++;
                    break;
                }
            }

            if (!(field = strsep(&tmp, ":")))
                goto syntax_err;

            *symbol_rate = strtoul(field, NULL, 0) * 1000;

            fprintf(stderr, "Parsed channel %s: %d %c MHz, SR:%d, %dE (%s)\n", channel, *freq, *polarity == 0 ? 'H' : 'V', *symbol_rate, *tone == 0 ? 128 : 124, *delsys == SYS_DVBS ? "DVB-S" : "DVB-S2");

            fclose(cfp);

            return true;

syntax_err:
            fprintf(stderr, "syntax error in line %u: '%s'\n", line, buf);
        } else if (ferror(cfp)) {
            fprintf(stderr, "error reading channel list '%s': %d %m\n", filename, errno);
            fclose(cfp);
            return false;
        } else
            break;
    }

    fclose(cfp);

    return false;
}

int tune(char *channel, thread_data *tdata, int dev_num)
{
    int ifreq = -1;
    int polarity = -1;
    int tone = -1;
    int symbol_rate = -1;
    int delsys = -1;
    int modulation = -1;

    struct dtv_property prop[] = {
            {.cmd = DTV_DELIVERY_SYSTEM,   .u.data = delsys },
            {.cmd = DTV_FREQUENCY,         .u.data = ifreq },
            {.cmd = DTV_MODULATION,        .u.data = modulation },
            {.cmd = DTV_SYMBOL_RATE,       .u.data = symbol_rate },
            {.cmd = DTV_INNER_FEC,         .u.data = FEC_AUTO },
            {.cmd = DTV_INVERSION,         .u.data = INVERSION_AUTO },
            {.cmd = DTV_ROLLOFF,           .u.data = ROLLOFF_35 },
            {.cmd = DTV_PILOT,             .u.data = PILOT_AUTO },
            {.cmd = DTV_TUNE },
    };

    struct dtv_properties cmdseq = {
        .num = 9,
        .props = prop
    };

    struct dvb_frontend_info fe_info;
    int rc, i;
    struct dmx_pes_filter_params filter;
    struct dvb_frontend_event event;
    struct pollfd pfd[1];
    char device[32];

    if (!read_channels(chanfile, channel, &ifreq, &polarity, &tone, &symbol_rate, &delsys)) {
        fprintf(stderr, "Channel %s not found\n", channel);
        return 1;
    }

    /* open tuner */
    if (fefd == 0) {
        sprintf(device, "/dev/dvb/adapter%d/frontend0", dev_num);
        fefd = open(device, O_RDWR);
        if (fefd < 0) {
            fprintf(stderr, "cannot open frontend device\n");
            fefd = 0;
            return 1;
        }
        fprintf(stderr, "device = %s\n", device);
    }

    if ((ioctl(fefd, FE_GET_INFO, &fe_info) < 0)) {
        fprintf(stderr, "FE_GET_INFO failed\n");
        return 1;
    }
    if (fe_info.type != FE_QPSK) {
        fprintf(stderr, "type is not supported\n");
        return 1;
    }

    fprintf(stderr, "Using DVB card \"%s\"\n", fe_info.name);

    // configure tune properties

    // Delivery System
    prop[0].u.data = delsys;
    // Frequency
    prop[1].u.data = (ifreq - SKAPA_LO) * 1000;
    fprintf(stderr, "Tuning to %d MHz\n", prop[1].u.data / 1000);
    // Modulation
    prop[2].u.data = delsys == SYS_DVBS ? QPSK : PSK_8;
    // Symbol Rate
    prop[3].u.data = symbol_rate;

    if (!dvb_voltage_tone(fefd, polarity, tone)) {
        fprintf(stderr, "Error setting voltage/tone\n");
        return 1;
    }

    if (ioctl(fefd, FE_SET_PROPERTY, &cmdseq) == -1) {
        perror("ioctl FE_SET_PROPERTY\n");
        return 1;
    }

    pfd[0].fd = fefd;
    pfd[0].events = POLLIN;
    event.status = 0;
    fprintf(stderr, "polling");
    for (i = 0; (i < 5) && ((event.status & FE_TIMEDOUT) == 0) && ((event.status & FE_HAS_LOCK) == 0); i++) {
        fprintf(stderr, ".");
        if (poll(pfd, 1, 5000)) {
            if (pfd[0].revents & POLLIN) {
                if ((rc = ioctl(fefd, FE_GET_EVENT, &event)) < 0) {
                    if (errno != EOVERFLOW) {
                        perror("ioctl FE_GET_EVENT");
                        fprintf(stderr, "status = %d\n", rc);
                        fprintf(stderr, "errno = %d\n", errno);
                        return -1;
                    } else fprintf(stderr, "\nOverflow error, trying again (status = %d, errno = %d)", rc, errno);
                }
            }
        }
    }

    if ((event.status & FE_HAS_LOCK) == 0) {
        fprintf(stderr, "\nCannot lock to the signal on the given channel\n");
        return 1;
    } else fprintf(stderr, "ok\n");

    if (dmxfd == 0) {
        sprintf(device, "/dev/dvb/adapter%d/demux0", dev_num);
        if ((dmxfd = open(device, O_RDWR)) < 0) {
            dmxfd = 0;
            fprintf(stderr, "cannot open demux device\n");
            return 1;
        }
    }

    filter.pid = 0x2000;
    filter.input = DMX_IN_FRONTEND;
    filter.output = DMX_OUT_TS_TAP;
    //    filter.pes_type = DMX_PES_OTHER;
    filter.pes_type = DMX_PES_VIDEO;
    filter.flags = DMX_IMMEDIATE_START;
    if (ioctl(dmxfd, DMX_SET_PES_FILTER, &filter) == -1) {
        fprintf(stderr, "FILTER %i: ", filter.pid);
        perror("ioctl DMX_SET_PES_FILTER");
        close(dmxfd);
        dmxfd = 0;
        return 1;
    }

    if (tdata->tfd < 0) {
        sprintf(device, "/dev/dvb/adapter%d/dvr0", dev_num);
        if ((tdata->tfd = open(device, O_RDONLY)) < 0) {
            fprintf(stderr, "cannot open dvr device\n");
            close(dmxfd);
            dmxfd = 0;
            return 1;
        }
    }

    /* show signal strength */
    calc_cn();

    return 0; /* success */
}
