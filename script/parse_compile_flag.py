import sys
import json
import os

known_block_list = {
    '-mabi',
    '-o'
}

KDIR_PATH = None

def parse_compile_flag(flag_file):
    compile_flags = []
    with open(flag_file, 'r') as f:
        data = json.load(f)
        
    include_next = False
    for entry in data:
        if 'arguments' in entry and 'file' in entry:
            
            if not entry['file'].endswith('.c') or entry['file'].endswith('.mod.c'):
                continue

            for arg in entry['arguments']:

                if include_next:
                    arg = arg.replace('./', KDIR_PATH)
                    key = f"-include\n{arg}"
                    
                    if key in compile_flags:
                        continue
                    
                    compile_flags.append(key)

                include_next = False
                
                if not arg.startswith('-'):
                    continue
                
                if arg in known_block_list:
                    continue

                if arg == '-include':
                    include_next = True
                    continue
                
                if arg.startswith('-Werror'):
                    continue
                
                if arg.startswith('-fno'):
                    continue
                
                
                if arg.startswith("-I"):
                    arg = arg.replace('./', KDIR_PATH)
                    
                if arg in compile_flags:
                    continue
                

                compile_flags.append(arg)
            break
                

    return compile_flags

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python parse_compile_flag.py <flag_file>")
        sys.exit(1)
        
    KDIR_PATH = os.getenv("KDIR") + '/'

    flag_file = sys.argv[1]
    compile_flags = parse_compile_flag(flag_file)
    print('\n'.join(compile_flags))