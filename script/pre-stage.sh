#!/bin/bash

echo "[*] Pre Stage"
python3 ./script/parse_compile_flag.py _compile_flag.json > _compile_flag.txt

echo "[*] Create Instrumented Files"

rm -r _test
mkdir _test
cp ../*.h _test/
cp ../Makefile _test/
cp ./src/* _test/


for i in $SRCS; do
    file="$M"/"$i"
    # cp $file _test/$i
    ./instrument-tool/module_init_macro $file -o _test/$i --flags-file=./_compile_flag.txt -- --target=aarch64-linux-gnu
done

# echo "[*] Compile Driver with Testing Framework"
# make -C _test TEST_OBJ=${TEST_OBJ}

# make -C _test KDIR="${KDIR}" TEST_OBJS="${TEST_OBJS}" TEST_INCLUDE="${TEST_INCLUDE_DIR}"
# ./instrument-tool/module_init_macro _test/*.c --flags-file=./_compile_flag.txt
