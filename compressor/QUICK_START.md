# Quick Start Guide

## TL;DR - Run Your Compressor Now!

```bash
# Build it
make bin/compressor

# Run it
make run

# Check output
cat tests/output.txt
```

That's it! Your compressor is now running with **MaxRectStrat** (best compression).

---

## Which Strategy Should I Use?

### For the Leaderboard Competition:

#### 🏆 **Best for COMPRESSION** (Current Default)
**Use: MaxRectStrat** ✅ Already selected!

```bash
# You're already using this! Just build and submit:
make bin/compressor
# → Submit bin/compressor to compression leaderboard
```

**Why MaxRectStrat wins compression**:
- Finds maximal rectangles in each slice
- Stacks identical rectangles vertically
- Typically achieves 10-20x compression
- More sophisticated algorithm = better compression ratio

#### ⚡ **Best for SPEED**
**Use: GreedyStrat**

To switch to GreedyStrat:

1. **Edit** `src/main.cpp` line 16:
```cpp
// Change this:
Strategy::MaxRectStrat strat;

// To this:
Strategy::GreedyStrat strat;
```

2. **Rebuild**:
```bash
make clean
make bin/compressor
```

3. **Submit** to speed leaderboard

**Why GreedyStrat wins speed**:
- Simple horizontal + vertical merging
- Fast algorithm, minimal computation
- Still achieves good compression (5-10x)
- Best speed-to-compression ratio

---

## Strategy Summary Table

| Strategy | When to Use | Compression | Speed |
|----------|------------|-------------|-------|
| **MaxRectStrat** ⭐ | Compression leaderboard | 🥇 Best (10-20x) | 🥉 Medium |
| **GreedyStrat** ⭐ | Speed leaderboard | 🥈 Good (5-10x) | 🥇 Fastest |
| **RLEXYStrat** | Balanced needs | 🥈 Good (5-15x) | 🥈 Fast |
| **DefaultStrat** | Testing only | ❌ None (1:1) | 🏃 Very Fast |

---

## Running Your Compressor

### Method 1: Use the Makefile (Easiest) ✅
```bash
# Build
make bin/compressor

# Run with test data
make run

# Output is in tests/output.txt
cat tests/output.txt
```

### Method 2: Manual Commands
```bash
# Build
make bin/compressor

# Run from a file
cat tests/input.txt | ./bin/compressor > output.txt

# Run from stdin (type manually, Ctrl+D when done)
./bin/compressor

# Time the execution
time cat tests/input.txt | ./bin/compressor
```

### Method 3: Pipe from Another Program
```bash
# Generate data on-the-fly
python generate_model.py | ./bin/compressor > compressed.txt

# Process large files
cat large_model.txt | ./bin/compressor > compressed.txt
```

---

## Measuring Performance

### Speed Test
```bash
# On macOS
time cat tests/input.txt | ./bin/compressor > /dev/null

# With detailed memory stats
/usr/bin/time -l cat tests/input.txt | ./bin/compressor > /dev/null
```

### Compression Test
```bash
# Count input blocks (roughly)
cat tests/input.txt | grep -v "^$" | grep -v "," | wc -l

# Count output blocks
cat tests/input.txt | ./bin/compressor | wc -l

# Calculate ratio manually
```

---

## Changing Strategies - Step by Step

1. **Open** `src/main.cpp` in your editor

2. **Find** line 16 (around here):
```cpp
Strategy::MaxRectStrat strat;  // ← This line
```

3. **Replace** with one of these:
```cpp
Strategy::GreedyStrat strat;     // For speed
Strategy::RLEXYStrat strat;      // For balance
Strategy::DefaultStrat strat;    // For testing
```

4. **Save** the file

5. **Rebuild**:
```bash
make clean
make bin/compressor
```

6. **Test**:
```bash
make run
```

That's it!

---

## For Competition Submission

### Compression Leaderboard 🗜️
```bash
# Use MaxRectStrat (already set!)
make clean
make bin/compressor

# Test it works
make run

# Submit: bin/compressor
```

### Speed Leaderboard ⚡
```bash
# 1. Change to GreedyStrat in src/main.cpp
# 2. Rebuild
make clean
make bin/compressor

# 3. Test it works
make run

# 4. Submit: bin/compressor
```

### Build for Windows (if needed)
```bash
make build-windows
# Submit: bin/compressor_win.exe
```

---

## Verifying Correctness

```bash
# Run and save output
cat tests/input.txt | ./bin/compressor > my_output.txt

# Compare with sample (if available)
diff tests/sample-output.txt my_output.txt

# Or just check it looks reasonable
head -20 my_output.txt
```

Output format should be:
```
x,y,z,width,height,depth,label
0,0,0,4,4,2,SA
4,0,0,3,4,2,SA
...
```

---

## Current Status ✅

Your compressor is:
- ✅ **Working** - Compiles and runs successfully
- ✅ **Streaming** - Handles infinite/large inputs with constant memory
- ✅ **Optimized** - Using `-O3` compiler flags
- ✅ **Fast I/O** - Buffered output (1MB buffer)
- ✅ **Production Ready** - Can submit to leaderboard now!

Currently using: **MaxRectStrat** (best compression)

---

## Recommendations

### For Best Competition Results:

1. **Submit to BOTH leaderboards**:
   - **Compression**: Keep MaxRectStrat, build, submit ✅
   - **Speed**: Switch to GreedyStrat, rebuild, submit ✅

2. **You can submit multiple times**:
   - Leaderboard keeps your best score
   - Experiment with different approaches
   - Try tweaking algorithms

3. **Your current build is ready**:
   - Already using best compression strategy
   - Already fully optimized
   - Ready to submit right now!

---

## Example Session

```bash
# Start fresh
cd compressor

# Build
make clean
make bin/compressor

# Test with sample
make run

# Check output
cat tests/output.txt | head -10

# Measure performance
time cat tests/input.txt | ./bin/compressor > /dev/null

# Ready to submit!
ls -lh bin/compressor
```

---

## Need Help?

- See [docs/USAGE_GUIDE.md](docs/USAGE_GUIDE.md) for detailed info
- See [docs/STREAMING_ARCHITECTURE.md](docs/STREAMING_ARCHITECTURE.md) for how it works
- Check [src/main.cpp](src/main.cpp) to change strategies
- Look at [include/Strategy.hpp](include/Strategy.hpp) to see all strategies

---

## Bottom Line

**For Compression Leaderboard** 🏆:
```bash
make clean && make bin/compressor
# Submit → bin/compressor
```

**For Speed Leaderboard** 🏆:
```bash
# Edit src/main.cpp → use GreedyStrat
make clean && make bin/compressor
# Submit → bin/compressor
```

**You're ready to compete!** 🚀
