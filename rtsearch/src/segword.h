#ifndef SEG_WORD_H
#define SEG_WORD_H

#include <scws/scws.h>

#define MAX_WORD_LEN (64)
#define MAX_WORDS_NUM (256)

typedef struct{
    char *data;
    int len;
}one_word_t;

scws_t s;
int segword_init(void);
int segword_fini(void);

#endif
