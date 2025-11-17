
# ビルドターゲット
# normal     : 通常使用用
# tournament : 大会で使う用


# ターゲットCPU
# 利用できるCPU拡張命令を指定する。
# ARM系ならOTHERを指定する。
# 実行速度を最大化するため、ご使用のCPUに合わせて設定してください

# TARGET_CPU = AVX2    # Intel Core i7/i9 (第2世代以降) 向け
# TARGET_CPU = SSE42   # Intel Core i7 (Nehalem) 向け
# TARGET_CPU = SSE41   # Intel Core 2 (Penryn) 向け
# TARGET_CPU = SSSE3   # Intel Core 2 向け
# TARGET_CPU = SSE2    # 基本的なSSE2対応CPU向け
# TARGET_CPU = NO_SSE  # 32bit用
TARGET_CPU = OTHER     # ARM用、または汎用的な互換性重視
# TARGET_CPU = ZEN1    # AMD Ryzen 1000/2000シリーズ向け
# TARGET_CPU = ZEN2    # AMD Ryzen 3000/5000シリーズ向け（推奨）

# デバッガーを使用するか
DEBUG = OFF
# DEBUG = ON


# CEDの端末でコンパイルする場合はg++8を指定する
# COMPILER = g++8
COMPILER = g++
# COMPILER = clang++


# 標準的なコンパイルオプション
CFLAGS   = -std=c++17 -fno-exceptions -fno-rtti -Wextra -Ofast -MMD -MP -fpermissive -DIS_ARM # devcontainer用に=-DIS_ARMオプションを追加
WCFLAGS  =
LDFLAGS  =
LIBS     =
INCLUDE  = # -I../include

# 最適化フラグ
CFLAGS += -O3

# 実行速度最大化のための追加最適化オプション
CFLAGS += -march=native              # 現在のCPUアーキテクチャに最適化
CFLAGS += -mtune=native              # 現在のCPUにチューニング
CFLAGS += -funroll-loops             # ループ展開を強制
CFLAGS += -fprefetch-loop-arrays     # 配列アクセスのプリフェッチ
CFLAGS += -fassociative-math         # 浮動小数点の結合法則を利用
CFLAGS += -fno-signed-zeros         # 符号付きゼロを無視
CFLAGS += -freciprocal-math         # 除算を逆数計算に置き換え
CFLAGS += -funsafe-math-optimizations # 数学的な厳密性を犠牲にした最適化

# デバッガーを使わないなら、NDEBUGをdefineする。
ifeq ($(DEBUG),OFF)
	CFLAGS += -DNDEBUG
endif

# clang用にCFLAGSなどを変更
ifeq ($(findstring clang++,$(COMPILER)),clang++)
	# stdlib
	CFLAGS += -stdlib=libstdc++

	# C++17のfilesystem
	# LDFLAGS += -lstdc++fs

	# 関数の引数が関数本体で使われていないときに警告出るのうざすぎるので抑制。
	CFLAGS += -Wno-unused-parameter

	# static リンクを行う際に __cxa_guard_acquire __cxa_guard_release の生成を抑止
	#   undefined reference to `__imp___cxa_guard_acquire'
	#   undefined reference to `__imp___cxa_guard_release'
	# static 変数を初期化したかを pthread_mutex_t でロックを取って確認し、
	# 最初の実行なら初期化するスレッドセーフなコードを生成するためのもの。
	# → 本当に消してしまっても大丈夫か？
	WCFLAGS += -fno-threadsafe-statics

else
	ifeq ($(findstring g++,$(COMPILER)),g++)
		# mingw g++ で AVX512 向けビルドを行おうとするとエラーになる問題の回避
		# https://stackoverflow.com/questions/43152633/invalid-register-for-seh-savexmm-in-cygwin
		# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65782
		WCFLAGS += -fno-asynchronous-unwind-tables

		# C++17のfilesystem
		# LDFLAGS += -lstdc++fs

		# GCC固有の最適化オプション
		CFLAGS += -frename-registers    # レジスタのリネーム
		CFLAGS += -fweb                 # Web最適化

	endif
endif

ifeq ($(OS),Windows_NT)
	CFLAGS += $(WCFLAGS)
	LDFLAGS += -static -Wl,--stack,25000000
	TARGET = minishogi-by-gcc.exe
else
	CFLAGS += -D_LINUX
	TARGET = minishogi-by-gcc
endif

# リンク時最適化。
# CFLAGSとLDFLAGSの両方で-fltoを指定する必要がある。
# cf. https://gcc.gnu.org/onlinedocs/gcc-6.3.0/gcc/Optimize-Options.html#Optimize-Options
LTOFLAGS = -flto

# wstringを使うためにこのシンボル定義が必要。
CFLAGS  += -DUNICODE

# stripの指示。(実行ファイルのサイズを小さく)
LDFLAGS += -Wl,-s

# mingw64では-D_WIN64,-D_WIN32は環境に応じて自動で設定されるので指定すべきではない。
# CFLAGS += -D_WIN64

# これを指定していると、各CPU向けの実行ファイルが生成されないので指定すべきではない。
CFLAGS   += -march=native

# デバッグ情報を付加
# CFLAGS  += -g3 -ggdb

ifeq ($(findstring g++,$(COMPILER)),g++)
	OPENMP   = -fopenmp
	OPENMP_LDFLAGS =
else ifeq ($(findstring clang++,$(COMPILER)),clang++)
	ifeq ($(MSYSTEM),MINGW64)
		# MSYS2 MINGW64 env
		# libgompを指定した場合、ビルドは通るがOpenMPは無効化される？
		OPENMP = -fopenmp=libgomp
		OPENMP_LDFLAGS =
	else
		ifeq ($(findstring w64-mingw32,$(COMPILER)),w64-mingw32)
			# Ubuntu mingw-w64 clang++ env (experimental)
			OPENMP = -fopenmp=libgomp
			OPENMP_LDFLAGS =
		else
			# Other (normal clang++/libomp env)
			OPENMP = -fopenmp
			OPENMP_LDFLAGS = -lomp
		endif
	endif
endif

CFLAGS += -DNO_EXCEPTIONS
LDFLAGS += -lpthread
LDFLAGS += -v

OBJDIR   = ../obj
ifeq "$(strip $(OBJDIR))" ""
	OBJDIR = ..
endif

SOURCES  = \
	main.cpp            \
	types.cpp           \
	bitboard.cpp        \
	misc.cpp            \
	movegen.cpp         \
	position.cpp        \
	usi.cpp             \
	evaluate.cpp        \
	search.cpp          \
	tt.cpp              \
	mate.cpp            \
	parallel_debug.cpp  \
	thread_pool.cpp     \
	extra/rp_cmd.cpp    \
	extra/user_test.cpp \

ifeq ($(TARGET_CPU),ZEN1)
	CFLAGS += -DUSE_AVX2 -mbmi -mno-bmi2 -mavx2 -march=znver1

else ifeq ($(TARGET_CPU),ZEN2)
	CFLAGS += -DUSE_AVX2 -mbmi -mno-bmi2 -mavx2 -march=znver2

else ifeq ($(TARGET_CPU),AVX2)
	CFLAGS += -DUSE_AVX2 -DUSE_BMI2 -mbmi -mbmi2 -mavx2 -march=corei7-avx

else ifeq ($(TARGET_CPU),SSE42)
	CFLAGS += -DUSE_SSE42 -msse4.2 -march=corei7

else ifeq ($(TARGET_CPU),SSE41)
	CFLAGS += -DUSE_SSE41 -msse4.1 -march=core2

else ifeq ($(TARGET_CPU),SSSE3)
	CFLAGS += -DUSE_SSSE3 -msse3 -march=core2

else ifeq ($(TARGET_CPU),SSE2)
	CFLAGS += -DUSE_SSE2 -msse2 -march=core2

else ifeq ($(TARGET_CPU),NO_SSE)
	# 32bit用。-m32は、MSYS2 MinGW-64だと無視されるので、
	# MinGW-64の32bit環境用でコンパイルする必要がある。
	CFLAGS += -DNO_SSE -m32 -march=pentium3

else ifeq ($(TARGET_CPU),OTHER)
	CFLAGS += -DNO_SSE

endif


CFLAGS += -DUSE_MAKEFILE

OBJECTS  = $(addprefix $(OBJDIR)/, $(SOURCES:.cpp=.o))
DEPENDS  = $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS) $(LIBS)
	$(COMPILER) -o $@ $^ $(LDFLAGS) $(CFLAGS)

$(OBJDIR)/%.o: %.cpp
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(COMPILER) $(CFLAGS) $(INCLUDE) -o $@ -c $<

all: clean $(TARGET)

# https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html


# ビルドターゲット

# 通常使用。
normal:
	$(MAKE) CFLAGS='$(CFLAGS)' LDFLAGS='$(LDFLAGS) $(LTOFLAGS)' $(TARGET)

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET) ${OBJECTS:.o=.gcda}

-include $(DEPENDS)
