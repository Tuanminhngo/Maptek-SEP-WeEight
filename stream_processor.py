import sys

# read first line

header = sys.stdin.readline().strip()
x_count, y_count, z_count, parent_x, parent_y, parent_z = map(int, header.split(','))


# read tag table
tag_table = {}
for line in sys.stdin:
    line = line.strip()
    if not line:
        break # blank line at the end

    tag, label = line.split(',')
    tag_table[tag] = label


# Process the block data slice by slice

for z in range(z_count):
    for y in range(y_count):
        line = sys.stdin.readline().strip()
        for x in range(x_count):
            tag = line[x]
            label = tag_table.get(tag)
            if label is None:
                print(f"Error: Tag '{tag}' not found in tag table.", file=sys.stderr)
                sys.exit(1)
            
            sys.stdout.write(f"{x},{y},{z},1,1,1,{label}\n")
    
    if z < z_count - 1:
        sys.stdin.readline()