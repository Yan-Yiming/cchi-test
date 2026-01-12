# ==============================================================================
#                      Compact CHI 仿真环境构建脚本 (Makefile)
# ==============================================================================
#
# 给新手的快速指南：
# 1. 在终端输入 'make' 即可编译项目。
# 2. 输入 'make run' 可以编译并立即运行。
# 3. 输入 'make clean' 可以清理掉所有生成的文件。
#
# 核心原理：
# 源文件(.cpp) -> 编译器(g++) -> 对象文件(.o) -> 链接器 -> 可执行程序(chi_sim)
# ==============================================================================

# ------------------------------------------------------------------------------
# 1. 变量定义 (Configuration)
# ------------------------------------------------------------------------------

# 指定使用的 C++ 编译器
CXX      := g++

# 编译选项 (CFLAGS):
# -std=c++17 : 使用 C++17 标准 (支持更现代的语法)
# -Wall      : 打开所有常见的警告 (Warning All)，帮助发现代码隐患
# -Wextra    : 打开额外的警告
# -g         : 生成调试信息 (Debug info)，如果程序崩了，可以用 gdb 调试
# -O2        : 开启二级优化 (Optimize)，让程序跑得更快
# -MMD -MP   : [魔法选项] 自动生成依赖文件(.d)。
#              如果你修改了 .h 头文件，Make 会自动发现并重新编译包含它的 .cpp
CXXFLAGS := -std=c++17 -Wall -Wextra -g -O2 -MMD -MP -DVM_TRACE=1 -DVM_TRACE_FST=1

# 头文件搜索路径 (Includes):
# -I 告诉编译器去哪里找 #include "..." 的文件
INCLUDES := -Imain \
			-I$(VERILATOR_ROOT)/include \
            -I$(VERILATOR_ROOT)/include/vltstd \
			-I./verilated

# 最终生成的可执行文件的名字
TARGET   := chi_sim

# 构建目录: 所有的中间文件 (.o, .d) 都会被扔到这个文件夹里，保持源码目录干净
BUILD_DIR := build

# DUT 目录: 存放 Verilog 源文件的地方
DUT_DIR := ./main/dut/CoupledL2

# ------------------------------------------------------------------------------
# 2. 文件列表 (Files)
# ------------------------------------------------------------------------------

# 源文件列表 (SRCS):
# [重要] 如果你添加了新的 .cpp 文件，请在这里把路径加进去！
# 使用反斜杠 (\) 来换行，看起来更清晰
SRCS     := main/main.cpp \
            main/CHISequencer.cpp \
            main/agent/FCAgent.cpp \
            main/cdut/MockL2Cache.cpp

# 对象文件列表 (OBJS):
# 这是一个"变量替换"操作。
# 意思是：把 SRCS 列表里所有的 ".cpp" 替换成 ".o"，并且加上 "$(BUILD_DIR)/" 前缀。
# 例如: main/main.cpp  -->  build/main/main.o
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

# 依赖文件列表 (DEPS):
# 把对象列表里的 .o 换成 .d
DEPS := $(OBJS:.o=.d)

# ------------------------------------------------------------------------------
# 3. 构建规则 (Rules)
# ------------------------------------------------------------------------------

# .PHONY 伪目标:
# 告诉 Make，'all', 'clean', 'run' 不是文件名。
# 防止目录里刚好有一个叫 "clean" 的文件导致命令失效。
.PHONY: all clean run help

# --- 默认目标 ---
# 当你只输入 'make' 时，它会寻找第一个目标，也就是这个 'all'
all: $(TARGET)

# --- 链接规则 (Linking) ---
# 目标: $(TARGET) (即 chi_sim)
# 依赖: $(OBJS)   (即所有的 .o 文件)
# 动作: 调用 g++ 把所有 .o 文件合并成一个可执行文件
$(TARGET): $(OBJS) $(VLT_DIR)/vltdut.a
	@echo "  [LINK] 正在生成最终可执行文件: $@"
	@$(CXX) $(OBJS) $(LDFLAGS) -o $@
	@echo "构建成功！输入 './$(TARGET)' 开始运行。"

# --- 编译规则 (Compiling) ---
# 这是一个 "模式规则" (Pattern Rule)。
# 目标: $(BUILD_DIR)/%.o (build 目录下的任意 .o 文件)
# 依赖: %.cpp            (对应的 .cpp 源文件)
#
# 符号解释:
#   $@  代表 "目标文件" (例如 build/main/main.o)
#   $<  代表 "第一个依赖文件" (例如 main/main.cpp)
#   dir 函数: 取出路径部分。例如 $(dir build/main/main.o) 得到 build/main/
$(BUILD_DIR)/%.o: %.cpp
	@# 第一步：自动创建存放 .o 文件的目录 (例如 build/main/)，-p 表示如果目录已存在则不报错
	@mkdir -p $(dir $@)
	@# 第二步：打印正在编译的文件名 (为了好看)
	@echo "  [CXX]  正在编译: $<"
	@# 第三步：执行编译。-c 表示只编译不链接，-o 指定输出位置
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# --- 引入依赖关系 ---
# 把编译器自动生成的 .d 文件包含进来。
# 前面的 '-' 表示如果文件不存在也不要报错 (第一次编译时肯定不存在)
-include $(DEPS)


coupledL2-verilog:
	@echo "  [CHISEL] 正在生成 TestTop Verilog..."
	$(MAKE) -C $(DUT_DIR) test-top-cchi-dummyl2

# 第二步：调用 Verilator 将 Verilog 转为 C++ 库
# 使用 --lib-create 生成静态库 vltdut.a，方便后续链接
coupledL2-verilate: coupledL2-verilog
	@echo "  [VERILATOR] 正在将 Verilog 编译为静态库..."
	@rm -rf $(VLT_DIR)
	@mkdir -p $(VLT_DIR)
	verilator --trace-fst --cc --build --lib-create vltdut \
		--Mdir $(VLT_DIR) \
		$(DUT_DIR)/build/*.v \
		-Wno-fatal \
		--top TestTop \
		--build-jobs $(THREADS_BUILD) \
		--verilate-jobs $(THREADS_BUILD) \
		-DSIM_TOP_MODULE_NAME=TestTop

# 快捷入口：一键完成硬件构建
$(VLT_DIR)/vltdut.a: coupledL2-verilate

# --- 清理规则 ---
# 删除 build 目录和最终的可执行文件
clean:
	@echo "正在清理旧文件..."
	@rm -rf $(BUILD_DIR) $(TARGET)
	@echo "清理完成。"

# --- 运行规则 ---
# 先触发 $(TARGET) 确保编译完成，然后运行它
run: $(TARGET)
	@echo "================ 启动仿真 ================"
	@./$(TARGET)

# --- 帮助信息 ---
help:
	@echo "可用命令:"
	@echo "  make       : 编译项目"
	@echo "  make run   : 编译并运行仿真"
	@echo "  make clean : 删除所有生成的文件"