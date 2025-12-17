import sys
import os

def load_metadata(meta_file):
    """
    Parses _wdtest_gen_info.txt
    Returns: dict { filename: [ {id, start, end}, ... ] }
    """
    file_map = {}
    blk_map = {}
    if not os.path.exists(meta_file):
        print(f"Error: {meta_file} not found.")
        sys.exit(1)

    with open(meta_file, 'r') as f:
        next(f) # Skip header
        for line in f:
            parts = line.strip().split(',')
            if len(parts) < 5: continue
            
            # Format: ID, Name, Start, End, Filename
            bid = int(parts[0])
            start = int(parts[2])
            end = int(parts[3])
            fname = parts[4].strip()
            
            if fname not in file_map:
                file_map[fname] = []
            
            if bid not in blk_map:
                blk_map[bid] = {}
    
            file_map[fname].append({
                'id': bid,
                'start': start,
                'end': end,
                'len': end - start, # Used to find specificity
            })
            
            blk_map[bid] = {
                'id': bid,
                'start': start,
                'end': end,
                'len': end - start,
                'filename': fname
            }
            
            
    return file_map, blk_map

def load_hits(log_file, file_map, blk_map):
    """Reads the runtime log into a set of IDs"""
    file_hits = {}
    hits = set()
    if os.path.exists(log_file):
        with open(log_file, 'r') as f:
            for line in f:
                x = line.strip().split(',')
                
                bid = int(x[0])
                
                start = blk_map[bid]['start']
                end = blk_map[bid]['end']
                
                hits.add(bid)
                if blk_map[bid]['filename'] not in file_hits:
                    file_hits[blk_map[bid]['filename']] = set()
                
                for i in range(start, end + 1):
                    file_hits[blk_map[bid]['filename']].add(i)
                
                # if line.strip().isdigit():
                #     hits.add(int(line.strip()))
    return file_hits, hits

def print_file_coverage(filename, blocks, blk_map, file_hits, hits):
    print(f"\n{'='*60}")
    print(f" FILE: {filename}")
    print(f"{'='*60}")

    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: Could not read source file {filename}")
        return
    
    for b in blk_map:
        if b not in hits:
            for i in range(blk_map[b]['start'], blk_map[b]['end'] + 1):
                if filename in file_hits and i in file_hits[filename]:
                    file_hits[filename].remove(i)
                # blk_map[b]['filename'].discard(i)

    # Iterate over every line of the source code
    for i, content in enumerate(lines):
        line_num = i + 1
        
        # 1. Find all blocks that contain this line
        # candidates = [b for b in blocks if b['start'] <= line_num <= b['end']]
        
        # prefix = "   " # Default (Whitespace) for lines outside functions
        
        # if candidates:
            # 2. Find the 'Most Specific' block (Smallest range)
            # This handles nesting: Function(Big) -> If(Small)
            # best_block = min(candidates, key=lambda b: b['len'])
            
            # 3. Check if that specific block was executed
        if filename in file_hits and line_num in file_hits[filename]:
            prefix = "[+] " # Covered (Green)
        else:
            prefix = "[-] " # Missed (Red)

        # Print formatted line
        # line_num | status | code
        print(f"{line_num:3} | {prefix} {content.rstrip()}")

def main():
    if len(sys.argv) < 3:
        print("Usage: python line_coverage.py <meta_file> <log_file>")
    meta_file = sys.argv[1]
    log_file = sys.argv[2]

    print(f"Loading metadata from {meta_file}...")
    file_map, blk_map = load_metadata(meta_file)
    
    print(f"Loading hits from {log_file}...")
    (file_hits, hits) = load_hits(log_file, file_map, blk_map)
    print(list(hits))

    # Process each file found in the metadata
    for filename, blocks in file_map.items():
        print_file_coverage(filename, blocks, blk_map, file_hits, hits)

if __name__ == "__main__":
    main()