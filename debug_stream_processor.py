import sys

# read first line
try:
    header = sys.stdin.readline().strip()
    x_count, y_count, z_count, parent_x, parent_y, parent_z = map(int, header.split(','))
except (IOError, ValueError) as e:
    print(f"Error reading header: {e}", file=sys.stderr)
    sys.exit(1)

# read tag table
tag_table = {}
for line in sys.stdin:
    line = line.strip()
    if not line:
        break # blank line at the end
    try:
        tag, label = line.split(',')
        tag_table[tag] = label
    except ValueError as e:
        print(f"Error reading tag table line: {line} - {e}", file=sys.stderr)
        sys.exit(1)

# Process the block data slice by slice

for z in range(z_count):
    for y in range(y_count):
        try:
            line = sys.stdin.readline().strip()
            for x in range(x_count):
                tag = line[x]
                label = tag_table.get(tag)
                if label is None:
                    print(f"Error: Tag '{tag}' not found in tag table.", file=sys.stderr)
                    sys.exit(1)
                
                sys.stdout.write(f"{x},{y},{z},1,1,1,{label}\n")
        except (IOError, IndexError) as e:
            print(f"Error processing data at z={z}, y={y}: {e}", file=sys.stderr)
            sys.exit(1)
    
    if z < z_count - 1:
        sys.stdin.readline()