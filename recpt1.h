/* -*- tab-width: 4; indent-tabs-mode: nil -*- */
#ifndef _RECPT1_H_
#define _RECPT1_H_

#define NUM_BSDEV       8
#define NUM_ISDB_T_DEV  8
#define CHTYPE_SATELLITE    0        /* satellite digital */
#define CHTYPE_GROUND       1        /* terrestrial digital */
#define MAX_QUEUE           8192
#define MAX_READ_SIZE       (188 * 87) /* 188*87=16356 splitter��188���饤���Ȥ���Ԥ��Ƥ���ΤǤ��ο����Ȥ���*/
#define WRITE_SIZE          (1024 * 1024 * 2)
#define TRUE                1
#define FALSE               0

typedef struct _BUFSZ {
    int size;
    u_char buffer[MAX_READ_SIZE];
} BUFSZ;

typedef struct _QUEUE_T {
    unsigned int in;        // ��������륤��ǥå���
    unsigned int out;        // ���˽Ф�����ǥå���
    unsigned int size;        // ���塼�Υ�����
    unsigned int num_avail;    // ������ˤʤ�� 0 �ˤʤ�
    unsigned int num_used;    // ���äݤˤʤ�� 0 �ˤʤ�
    pthread_mutex_t mutex;
    pthread_cond_t cond_avail;    // �ǡ�����������ΤȤ����ԤĤ���� cond
    pthread_cond_t cond_used;    // �ǡ��������ΤȤ����ԤĤ���� cond
    BUFSZ *buffer[1];    // �Хåե��ݥ���
} QUEUE_T;

#endif
