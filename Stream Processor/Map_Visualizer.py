import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import ListedColormap
from matplotlib.patches import Patch

colors = None  # we'll build a colormap automatically

def parse_input(path):
    with open(path) as f:
        x_count, y_count, z_count, parent_x, parent_y, parent_z = map(int, f.readline().split(","))
        # tag table
        tags = {}
        while True:
            pos = f.tell()
            line = f.readline()
            if not line or line.strip() == "":
                break
            t, label = line.split(",", 1)
            tags[t.strip()] = label.strip()
        # slices
        slices, cur = [], []
        for line in f:
            line = line.rstrip("\n")
            if line == "":
                if cur:
                    slices.append(cur)
                    cur = []
            else:
                cur.append([tags[ch] for ch in line])
        if cur: slices.append(cur)
    return (x_count, y_count, z_count), slices

def parse_output(path):
    blocks = []
    with open(path) as f:
        for line in f:
            if not line.strip(): continue
            x,y,z,dx,dy,dz,label = [p.strip() for p in line.split(",")]
            blocks.append((int(x),int(y),int(z),int(dx),int(dy),int(dz),label))
    return blocks

def reconstruct_from_output(blocks, dims):
    x_count, y_count, z_count = dims
    vol = [[["" for _ in range(x_count)] for _ in range(y_count)] for _ in range(z_count)]
    for x,y,z,dx,dy,dz,label in blocks:
        for zz in range(z, z+dz):
            for yy in range(y, y+dy):
                for xx in range(x, x+dx):
                    vol[zz][yy][xx] = label
    return vol

def build_label_mapping(volume_like):
    # get labels in order of first appearance
    labels = []
    seen = set()
    for slc in volume_like:
        for row in slc:
            for lab in row:
                if lab not in seen:
                    seen.add(lab); labels.append(lab)
    cmap = ListedColormap([plt.get_cmap("tab20")(i % 20) for i in range(len(labels))])
    to_idx = {lab:i for i,lab in enumerate(labels)}
    return to_idx, labels, cmap

def labels_to_ints(slc, to_idx):
    h, w = len(slc), len(slc[0])
    arr = np.zeros((h,w), dtype=int)
    for i in range(h):
        for j in range(w):
            arr[i,j] = to_idx[slc[i][j]]
    return arr

from matplotlib.patches import Patch

def save_slice(arr_int, labels, cmap, title, outfile):
    import matplotlib.pyplot as plt
    n = len(labels)

    plt.figure(figsize=(4,4))
    plt.imshow(arr_int, origin="lower", cmap=cmap, vmin=0, vmax=n-1)
    plt.title(title)
    plt.axis("off")

    # Build legend handles with colors matching the colormap indices
    swatches = [Patch(facecolor=cmap(i), edgecolor="none", label=labels[i]) for i in range(n)]
    # ✅ Either of these is fine:
    # plt.legend(handles=swatches, loc="upper right", fontsize=8, frameon=False)
    plt.legend(swatches, [h.get_label() for h in swatches], loc="upper right", fontsize=8, frameon=False)

    plt.tight_layout()
    plt.savefig(outfile, dpi=200, bbox_inches="tight")
    plt.close()

# ----- Run on your files -----
dims, input_slices = parse_input("input.txt")           # (x_count,y_count,z_count), list[z][y][x] of labels
to_idx_in, labels_in, cmap_in = build_label_mapping(input_slices)
for z in range(dims[2]):
    arr = labels_to_ints(input_slices[z], to_idx_in)
    save_slice(arr, labels_in, cmap_in, f"Input Slice z={z}", f"input_slice_z{z}.png")

blocks = parse_output("output.txt")
output_vol = reconstruct_from_output(blocks, dims)
to_idx_out, labels_out, cmap_out = build_label_mapping(output_vol)
for z in range(dims[2]):
    arr = labels_to_ints(output_vol[z], to_idx_out)
    save_slice(arr, labels_out, cmap_out, f"Output Slice z={z}", f"output_slice_z{z}.png")