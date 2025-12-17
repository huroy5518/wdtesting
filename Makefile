.PHONY: all pre compile post clean test

all: tool pre compile post

SRC_DIR:=src
TEST_INCLUDE_DIR:=$(realpath ./_test)
TEST_SRCS:=$(wildcard $(SRC_DIR)/*.c)
TEST_OBJS:=$(patsubst $(SRC_DIR)/%.c, ./%.o, $(TEST_SRCS))
# TEST_OBJS=$(TEST_SRCS:.c=.o)

tool:
	@echo "[*] Compile Instrument Tool"
	make -C instrument-tool

pre:
	@echo "[*] First Compile to Generate Compilation Flags"
	@echo ${KDIR}
	@echo ${ARCH}
	@echo ${CROSS_COMPILE}
	make -C ../ clean
	bear --output _compile_flag.json -- make -j$(nproc) -C ../
	python3 ./script/parse_compile_flag.py _compile_flag.json > _compile_flag.txt
	bash ./script/pre-stage.sh
	@echo "[*] Build Tools"
	make -C _test KDIR="${KDIR}" TEST_OBJS="${TEST_OBJS}" TEST_INCLUDE="${TEST_INCLUDE_DIR}"

compile:
	@echo "In compile"
	
post:
	./script/post-stage.sh

test: $(OBJS) test/test.c
	gcc -o test/test test/test.c $(OBJS)


clean:
	@echo "clean"