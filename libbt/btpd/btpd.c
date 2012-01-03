#include "btpd.h"

#include <openssl/sha.h>
#include <signal.h>

static uint8_t m_peer_id[20];
static struct timeout m_heartbeat;
static int m_signal;
static int m_shutdown;
static int m_ghost;

long btpd_seconds;
volatile int daemon_stop = 0;

void
btpd_exit(int code)
{
    btpd_log(BTPD_L_BTPD, "Exiting.\r\n");
    exit(code);
}

void net_shutdown(void);

static void
death_procedure(void)
{
    assert(m_shutdown);
    /*if (torrent_count() == 0)
        btpd_exit(0);*/
    if (!m_ghost && torrent_count() == torrent_ghosts()) {
        btpd_log(BTPD_L_BTPD, "Entering pre exit mode. Bye!\r\n");
        fclose(stderr);
        fclose(stdout);
        net_shutdown();
        m_ghost = 1;
    }
}

void
btpd_shutdown(void)
{
    struct torrent *tp, *next;
    m_shutdown = 1;
    BTPDQ_FOREACH_MUTABLE(tp, torrent_get_all(), entry, next)
        torrent_stop(tp, 0);
    death_procedure();
}

int btpd_is_stopping(void)
{
    return m_shutdown;
}

const uint8_t *
btpd_get_peer_id(void)
{
    return m_peer_id;
}

static void
signal_handler(int signal)
{
    m_signal = signal;
}

static void
heartbeat_cb(int fd, short type, void *arg)
{
    struct timespec tm;
    tm.tv_sec = 1;
    tm.tv_nsec = 0;
    btpd_timer_add(&m_heartbeat, &tm);
    btpd_seconds++;
    net_on_tick();
    torrent_on_tick_all();
    if (m_signal) {
        m_signal = 0;
        if (!m_shutdown)
            btpd_shutdown();
    }
    if (m_shutdown)
        death_procedure();
}

void tr_init(void);
int ipc_init(bt_t *bt_arg);
void addrinfo_init(void);

int
btpd_init(bt_t *bt)
{
    unsigned long seed;
    uint8_t idcon[1024];

    int n;
    struct timespec ts;
	short bt_port = bt->bt_port;

    DWORD now = timeGetTime();
    n = sprintf(idcon, "%ld%d", (long)now,net_port);
    if (n < sizeof(idcon))
        gethostname(idcon + n, sizeof(idcon) - n);
    idcon[sizeof(idcon) - 1] = '\0';
    n = strlen(idcon);

    SHA1(idcon, n, m_peer_id);
    memcpy(&seed,m_peer_id,sizeof(seed));
    memcpy(m_peer_id, BTPD_VERSION, sizeof(BTPD_VERSION) - 1);
    m_peer_id[sizeof(BTPD_VERSION) - 1] = '|';

    srand(seed);

    if(td_init() != 0)
	return -1;
    addrinfo_init();

    if(net_init(&bt_port) != 0)
	return -1;
    if(ipc_init(bt)!=0)
	return -1;
    ul_init();
    cm_init();
    tr_init();
    if(tlib_init()!=0)
	return -1;

    evtimer_init(&m_heartbeat, heartbeat_cb, NULL);
    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    btpd_timer_add(&m_heartbeat, &ts);
    return 0;
}
