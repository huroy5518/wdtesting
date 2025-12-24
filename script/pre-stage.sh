#!/bin/bash

echo "[*] Pre Stage"
python3 ./script/parse_compile_flag.py _compile_flag.json > _compile_flag.txt

echo "[*] Create Instrumented Files"

rm -r _test
mkdir _test
cp ../*.h _test/
cp ../Makefile _test/
cp ./src/* _test/
echo ${TEST_FILES}
cp ${TEST_FILES} _test/

rm _wdtest_gen_info.txt
rm _wdtest_id_counter.txt

for i in $SRCS; do
    file="$M"/"$i"
    # cp $file _test/$i
    newpath=_test/$i
    newpath=${newpath//.c/.h}
    # ./instrument-tool/instrument _test/$i -o _test/$i --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu
    ./instrument-tool/instrument $file -o _test/$i --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu
    sed -i '1i#include "top.h"' _test/$i

    # ./instrument-tool/mock ${TEST_FILES} _test/$i --suffix="" --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu
    ./instrument-tool/module_init_macro _test/$i -o _test/$i --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu
    ./instrument-tool/remove_static _test/$i -o _test/$i --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu
    ./instrument-tool/expose_function _test/$i -o _test/$i --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu
    ./instrument-tool/generate_header _test/$i -o _test/impl_top.h --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu

    # ./instrument-tool/module_init_macro $file -o _test/$i --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu
done

# echo "[*] Compile Driver with Testing Framework"
# make -C _test TEST_OBJ=${TEST_OBJ}

# make -C _test KDIR="${KDIR}" TEST_OBJS="${TEST_OBJS}" TEST_INCLUDE="${TEST_INCLUDE_DIR}"
# ./instrument-tool/module_init_macro _test/*.c --flags-file=./_compile_flag.txt
