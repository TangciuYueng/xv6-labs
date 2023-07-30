#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("more args!\n");
		exit(1);
	}
	if (argc - 1 >= MAXARG) {
		printf("to many args!\n");
		exit(1);
	}

	int index = 0, char_num, m = 0;
	char* new_args[MAXARG];
	char block[512], buf[512];
	char* p = buf;
  	for (int i = 1; i < argc; i++) {
		new_args[index++] = argv[i];
  	}
  	while ((char_num = read(0, block, sizeof(block))) > 0) {
		// 遍历每行每个字符
		for (int i = 0; i < char_num; i++) {
			// buf重新开始
	  		if (block[i] == '\n') {
				buf[m] = 0;
				new_args[index++] = p;
				new_args[index] = 0;
				m = 0;
				p = buf;
				index = argc - 1;
				if (fork() == 0) {
					exec(argv[1], new_args);
				}
				wait(0);
			} 
			// 空格 分开单词
			else if (block[i] == ' ') {
				// 加尾零
				buf[m++] = 0;
				// 每个单词的开始
				new_args[index++] = p;
				// 下一个开始
				p = buf + m; 
	  		}
			// 一般字符装进buf
			else {
				buf[m++] = block[i];
	  		}
		}
	}
 	exit(0);
}
