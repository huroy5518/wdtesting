.PHONY: all clean test

SRCS:=$(wildcard *.c)
OBJS:=$(SRCS:.c=.o)

%.o: %.c
	gcc -c $^ -o $@
	
test: $(OBJS) test/test.c
	gcc -o test/test test/test.c $(OBJS)


all: pre compile post

pre:
	./script/pre-stage.sh
	
compile:
	echo $(KERNEL_INCLUDE_OPT)
	echo $(MAKE)
	make -C ath12k
# 		CC="$(CC)" \
#  		CFLAGS="$(CFLAGS)" \
# 		KERNEL_INCLUDE_OPT="$(KERNEL_INCLUDE_OPT)"
# 		LDFLAGS=$(LDFLAGS)


# 	echo "In compile"
# 	echo $(CC)
# 	echo $(CFLAG)
# 	echo $(LDFLAGS)
	
post:
	./script/post-stage.sh


clean:
	echo "Clean"
	
