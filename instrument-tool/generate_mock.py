import os
import re
import sys
import glob

# 設定: 你要掃描的原始碼目錄
SOURCE_DIR = "./example"  # <-- 請修改成你的原始碼路徑
# 設定: 產生的 Mock 資料夾
MOCK_DIR = "./mock_env"
MOCK_HEADER = os.path.join(MOCK_DIR, "mock_all.h")

# 基礎的核心定義 (這些很難用 regex 抓，直接內建)
BASE_DEFS = """
#ifndef MOCK_ALL_H
#define MOCK_ALL_H

// --- 基礎型別 ---
#define NULL ((void*)0)
typedef int bool;
typedef int char; // 防止某些奇怪的 typedef
typedef unsigned int u32;
typedef int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long long u64;
typedef long long s64;
typedef unsigned long size_t;
typedef long ssize_t;
typedef long loff_t;
typedef unsigned long uintptr_t;
typedef unsigned long dma_addr_t;
typedef int gfp_t;
typedef int mode_t;
typedef int dev_t;

// --- 常用巨集 ---
#define __init
#define __exit
#define __user
#define __iomem
#define __force
#define __weak
#define KBUILD_MODNAME "mock_driver"
#define THIS_MODULE 0
#define GFP_KERNEL 0
#define ENOMEM 12
#define EBUSY 16
#define EINVAL 22

// --- 巨集函式 ---
#define module_init(x)
#define module_exit(x)
#define MODULE_LICENSE(x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_DEVICE_TABLE(type, name)
#define PCI_DEVICE(vend, dev) {0}
#define PCI_ANY_ID 0
#define container_of(ptr, type, member) ((type *)0)
#define IS_ERR(ptr) 0
#define PTR_ERR(ptr) 0
#define ERR_PTR(val) ((void*)0)

// --- Printk ---
#define pr_info(fmt, ...) 0
#define pr_err(fmt, ...) 0
#define pr_warn(fmt, ...) 0
#define pr_debug(fmt, ...) 0
#define dev_info(dev, fmt, ...) 0
#define dev_err(dev, fmt, ...) 0

#endif
"""

def scan_files(root_dir):
    """遞迴掃描所有 .c 和 .h 檔案"""
    files = []
    for root, _, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename.endswith(('.c', '.h')):
                files.append(os.path.join(root, filename))
    return files

def extract_includes_and_structs(files):
    includes = set()
    structs = set()
    funcs = set()
    
    # Regex 模式
    re_include = re.compile(r'#include\s*[<"](linux/[^>"]+)[>"]')
    re_struct = re.compile(r'\bstruct\s+(\w+)')
    # 簡單抓函式呼叫的 pattern (抓變數或函式名)
    re_func_call = re.compile(r'\b(\w+)\s*\(')

    for fpath in files:
        try:
            with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
                # 1. 抓 Includes
                found_incs = re_include.findall(content)
                includes.update(found_incs)
                
                # 2. 抓 Structs
                found_structs = re_struct.findall(content)
                structs.update(found_structs)

                # 3. 抓可能的函式 (過濾掉 if, for, while 等關鍵字)
                found_calls = re_func_call.findall(content)
                keywords = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'typeof'}
                for fc in found_calls:
                    if fc not in keywords and fc not in found_structs:
                        funcs.add(fc)

        except Exception as e:
            print(f"Skipping {fpath}: {e}")

    return includes, structs, funcs

def generate_mocks(includes, structs, funcs):
    # 1. 建立資料夾結構 (解決 fatal error)
    if not os.path.exists(MOCK_DIR):
        os.makedirs(MOCK_DIR)
        
    for inc in includes:
        full_path = os.path.join(MOCK_DIR, inc)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        # 建立空檔案
        with open(full_path, 'w') as f:
            f.write(f"/* Mocked {inc} */\n")
    
    print(f"[+] Created {len(includes)} dummy header files.")

    # 2. 建立 mock_all.h (解決 unknown type)
    with open(MOCK_HEADER, 'w') as f:
        f.write(BASE_DEFS + "\n\n")
        
        f.write("// --- Auto-Generated Structs ---\n")
        for s in sorted(structs):
            # 為了讓 compiler 通過 struct init (e.g., .probe = ...)，
            # 我們給它幾個通用的 dummy field，這樣至少不會因為空 struct 而報錯
            f.write(f"struct {s} {{ int dummy_a; void* dummy_b; int (*dummy_func)(void); }};\n")
            
        f.write("\n// --- Auto-Generated Function Stubs ---\n")
        for func in sorted(funcs):
            # 全部宣告為回傳 int 且接受不定參數的函式
            # 這樣可以符合大多數使用情境
            f.write(f"int {func}();\n")
            
    print(f"[+] Generated mock header: {MOCK_HEADER}")
    print(f"    - Structs: {len(structs)}")
    print(f"    - Functions (estimated): {len(funcs)}")

if __name__ == "__main__":
    print(f"[*] Scanning directory: {SOURCE_DIR}")
    files = scan_files(SOURCE_DIR)
    inc, stru, funcs = extract_includes_and_structs(files)
    generate_mocks(inc, stru, funcs)