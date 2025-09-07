# 指定编译器
CC = gcc

# 指定编译器标志
CFLAGS = -Wall -I. -g

# 指定目标文件名
TARGET = patchinst

# 源文件
SRCS = patchinst.c

# 生成的可执行文件
OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 链接目标文件以生成可执行文件
$(TARGET): $(OBJS)
	$(CC) -o $@ $^

# 编译源文件生成目标文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理目标
clean:
	rm -f $(OBJS) $(TARGET)

# 显示使用说明
help:
	@echo "使用以下目标："
	@echo "  all    - 编译生成可执行文件"
	@echo "  clean  - 清理生成的文件"
	@echo "  help   - 显示帮助信息"

.PHONY: all clean help
