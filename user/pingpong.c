#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
	// 判断参数个数
	if (argc != 1) {
		fprintf(2, "Error: argument!\n");
		// 一场结束
		exit(1);
	}

	int parent2child[2];
	int child2parent[2];
	// pipe函数接受两个整数的数组
	// 返回两个文件描述符
	// p[0]用于读取数据
	// p[1]用于写入数据
	pipe(parent2child);
	pipe(child2parent);
	// 数据
	char ping_buf[4];
	char pong_buf[4];

	// 获得当前进程号
	int pid = fork();

	// 父进程
	if (pid > 0) {
		close(parent2child[0]);
		close(child2parent[1]);
		write(parent2child[1], "ping",4); 
		// 等待子进程发送消息
		wait((int*)0);
		if (read(child2parent[0], pong_buf, 4) == 4
				&& strcmp(pong_buf, "pong") == 0) {
			printf("%d: received pong\n", getpid());
		}
		close(parent2child[1]);
		close(child2parent[0]);
		exit(0);
	}
	else if (pid == 0) {
		close(parent2child[1]);
		close(child2parent[0]);
		if (read(parent2child[0], ping_buf, 4) == 4
				&& strcmp(ping_buf, "ping") == 0) {
			printf("%d: received ping\n", getpid());
		}
		write(child2parent[1], "pong", 4);
		close(parent2child[0]);
		close(child2parent[1]);
		exit(0);
	}
	else {
		printf("Fork error!\n");
		close(parent2child[0]);
		close(parent2child[1]);
		close(child2parent[0]);
		close(child2parent[1]);
		exit(1);
	}
}
