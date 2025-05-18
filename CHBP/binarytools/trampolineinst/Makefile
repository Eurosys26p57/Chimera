
CC = gcc


CFLAGS = -Wall -I. -g


TARGET = patchinst


SRCS = patchinst.c


OBJS = $(SRCS:.c=.o)


all: $(TARGET)


$(TARGET): $(OBJS)
	$(CC) -o $@ $^


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f $(OBJS) $(TARGET)


help:
	@echo "使用以下目标："
	@echo "  all    - 编译生成可执行文件"
	@echo "  clean  - 清理生成的文件"
	@echo "  help   - 显示帮助信息"

.PHONY: all clean help
