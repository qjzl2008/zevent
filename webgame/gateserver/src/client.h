#ifndef CLIENT_H
#define CLIENT_H

#ifdef __cplusplus
extern "C"{
#endif

enum client_state{
    INIT_STATE = 0x00,
    LOGINED_STATE = 0x01,
    ENTERED_STATE = 0x02
};

typedef struct client_t client_t;

struct client_t{
    int state;
    uint64_t gspeerid;
    uint64_t accountid;
};

#ifdef __cplusplus
}
#endif

#endif

