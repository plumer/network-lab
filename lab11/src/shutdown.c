#include "btdata.h"
#include "pwp_daemon.h"
#include "util.h"
#include "fileio.h"

extern int g_done;

// 正确的关闭客户端
void client_shutdown(int sig)
{
	puts("client_shutting down..");
	// 仅使得主程序退出循环，剩下的事情全部由主程序完成。
	// 设置全局停止变量以停止连接到其他peer, 以及允许其他peer的连接. 
	// Set global stop variable so that we stop trying to connect to peers and
	// 这控制了其他peer连接的套接字和连接到其他peer的线程.
	g_done = 1;

	// 依次关闭所有的连接，现在由主函数来完成
}
