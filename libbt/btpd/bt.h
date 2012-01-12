#ifndef BT_IF_H
#define BT_IF_H

#if !defined(WIN32)
#define BT_DECLARE(type)	type
#define BT_DECLARE_NONSTD(type)	type
#define BT_DECLARE_DATA
#else
#define BT_DECLARE(type)	__declspec(dllexport) type __stdcall
#define BT_DECLARE_NONSTD(type)	__declspec(dllexport) type
#define BT_DECLARE_DATA		__declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C"{
#endif

typedef struct bt_t bt_t;

typedef struct{
    int net_port;//bt端口 为0 则启用随机端口
	int use_upnp;//是否启用upnp，1：启用，0：不启用
    int empty_start;//是否空启动（以往任务是否启动）
}bt_arg_t;

typedef struct{
	short ipc_port;
}btcli_arg_t;

enum ipc_tstate {
	IPC_TSTATE_INACTIVE,//未开始
	IPC_TSTATE_START,   //正开始
	IPC_TSTATE_STOP,    //结束
	IPC_TSTATE_LEECH,   //正在下载
	IPC_TSTATE_SEED     //做种中
};

struct btstat{
    long long num;
    enum ipc_tstate state;
    long long peers,tr_good;
    long long content_got,content_size,downloaded,uploaded,rate_up,
	 rate_down,tot_up;
    long long pieces_seen,torrent_pieces;
};

/*
启动bt后台服务，获得后续操作句柄
@bt_arg 启动参数
@bt 后续需要用到的句柄
@返回值 rv：
 0 成功
 -1 失败
*/
BT_DECLARE(int) bt_start_daemon(bt_arg_t *bt_arg,bt_t **bt);

/*
结束bt服务
@bt 操作句柄
@返回值 rv：
   0 成功
   -1 失败
*/
BT_DECLARE(int) bt_stop_daemon(bt_t *bt);

/*
启动bt客户端，获得后续操作句柄
@btcli_arg 启动参数
@bt 后续需要用到的句柄
@返回值 rv：
0 成功
-1 失败
*/
BT_DECLARE(int) bt_start_client(btcli_arg_t *btcli_arg,bt_t **bt);

/*
断开bt客户端
@bt 操作句柄
@返回值 rv：
0 成功
-1 失败
*/
BT_DECLARE(int) bt_stop_client(bt_t *bt);

/*
添加本地种子
@dir 下载存放目录
@torrent 种子名
@bt bt操作句柄
@返回值 rv：
  0 成功
  1 已存在
  -1 失败
*/
BT_DECLARE(int) bt_add(const char *dir,const char *torrent,bt_t *bt);

/*
web seed接口
@dir 存放目录
@savename 远程种子下载到本地后的保存名字（含路径）
@url 种子的url地址
@bt bt操作句柄
@返回值 rv：
0 成功
1 已存在
-1 失败
*/
BT_DECLARE(int) bt_add_url(const char *dir,const char *savename,const char *url,
	bt_t *bt);

/*
删除任务
@argc 删除的任务数
@argv[] 删除的任务名数组
本接口会删除掉任务在本地的种子信息等
eg:
   char *torrents[2]={"1.torrent","2.torrent"};
   bt_del(2,torrents,bt);
@返回值 rv：
   0 成功
   -1 失败
*/
BT_DECLARE(int) bt_del(int argc,char **argv,bt_t *bt);
/*
停止任务
@返回值 rv：
0 成功
-1 失败
*/
BT_DECLARE(int) bt_stop(int argc,char **argv,bt_t *bt);
/*
停止所有任务
@返回值 rv：
0 成功
-1 失败
*/
BT_DECLARE(int) bt_stopall(bt_t *bt);
/*
开始下载
@返回值 rv：
0 成功
-1 失败
*/
BT_DECLARE(int) bt_start(int argc,char **argv,bt_t *bt);

/*
查看种子的下载状态信息
@torrent 种子名（含路径)
@bt 操作句柄
@stat 状态信息
@返回值 rv：
0 成功
-1 失败
*/
BT_DECLARE(int) bt_stat(char *torrent,bt_t *bt,struct btstat *stat);

/**
 * up 上传速率阀值(bytes 字节）
 * down 下载速率阀值(bytes 字节)
 @返回值 rv：
 0 成功
 -1 失败
 */
BT_DECLARE(int) bt_rate(unsigned up, unsigned down, bt_t *bt);

/**
 * p2sp 接口
 * 长效种子接口
 * @torrent 种子名（含路径)
 * @url 资源web地址
 * @bt 操作句柄
 * @本接口为每个url建立一个http连接，如果需要多个连接下载（可以加大速度，建议5个），
 * 可多次调用本接口（即使是同一url)
 @返回值 rv：
 0 成功
 -1 失败
 */
BT_DECLARE(int) bt_add_p2sp(char *torrent,const char *url,bt_t *bt); 

#ifdef __cplusplus
}
#endif
#endif

