# 3D Block Model Compression - Maptek SEP WeEight

High-performance 3D geological block model compression system for the Maptek Software Engineering Project competition. Achieves **98.59% compression ratio** (71.06x) using an optimized ensemble of compression algorithms.

## 🏆 Competition Results

- **Compression Rate**: 99.1715% on competition dataset

## Quick Start

### Prerequisites
- C++17 compatible compiler (g++, clang++)
- GNU Make
- (Optional) MinGW cross-compiler for Windows executable

### Build & Run

```bash
# Navigate to compressor directory
cd compressor

# Build the compressor
make clean

# Test on sample dataset
make run

# Build competition executable (for Windows)
make build-exe

# Build competition executable (for MacOS)
make build-exe-mac

# Run on custom dataset
cat data/your_dataset.csv | ./bin/compressor > output.csv
```

### Output
- Compressed output: `tests/output.txt` (from `make run`)
- Executables: `bin/compressor` (native) or `bin/compressor-mac.exe` (Windows)

## Algorithm Performance

Our **SmartMergeStrat** ensemble algorithm evaluates 5 different compression strategies per material label and selects the best result:

| Algorithm | Blocks | Compression | Time | Best For |
|-----------|--------|-------------|------|----------|
| **SmartMergeStrat** | **236,087** | **98.59%** | 2m 31s | **Competition (Best Overall)** |
| Optimal3DStrat | 263,796 | 98.43% | 48s | Large uniform regions |
| MaxRectStrat | 263,796 | 98.43% | 48s | 2D slices with Z-stacking |
| ScanlineStrat | 270,104 | 98.40% | 10s | Manhattan structures |
| GreedyStrat | 270,343 | 98.39% | 1.3s | Speed-critical applications |
| LayeredSliceStrat | 299,606 | 98.22% | 48s | Layered geological data |
| AdaptiveStrat | 307,629 | 98.17% | 27s | Heterogeneous patterns |

*Benchmarked on `the_worldly_one_16777216_256x256x256.csv` (256³, 7 labels)*

## Features

### Compression Algorithms

#### 1. **SmartMergeStrat** (Ensemble - Default)
- Tries multiple strategies per label and picks the best
- Combines strengths of all algorithms
- Best compression quality for competition

#### 2. **Optimal3DStrat** (MaxRect + Enhanced Z-Stacking)
- 2D maximal rectangle finding per Z-slice
- Optimized vertical merging across slices
- Excellent for large uniform ore bodies

#### 3. **ScanlineStrat** (Left-to-Right Sweep)
- Linear scanline sweep with active rectangle tracking
- Near-MaxRect quality in 1/5th the time
- Best speed-to-quality ratio

#### 4. **LayeredSliceStrat** (Z-First Grouping)
- Groups identical consecutive Z-slices
- Optimized for layered sedimentary geology
- Exploits stratigraphic repetition

#### 5. **GreedyStrat** (Fast Merging)
- Row-by-row horizontal + vertical merging
- Ultra-fast (1.3 seconds)
- Good fallback for simple patterns

#### 6. **AdaptiveStrat** (Pattern-Based)
- Analyzes local patterns and adapts strategy
- Good for mixed/heterogeneous datasets

#### 7. **RLEXYStrat** (Run-Length Encoding)
- Fast RLE along X-axis + vertical merging
- Efficient for linear/stripe patterns

### Additional Algorithms (Not Used in SmartMerge)

- **MaxRectStrat**: 2D MaxRect per slice (tied with Optimal3D)
- **QuadTreeStrat**: Hierarchical 2D subdivision (lower performance)
- **MaxCuboidStrat**: Globally optimal (too slow for practical use)
- **StreamRLEXY**: Streaming variant for infinite input

## Architecture

```
compressor/
├── src/
│   ├── main.cpp           # Entry point, uses SmartMergeStrat
│   ├── Strategy.cpp       # All compression algorithms (1101 lines)
│   ├── Model.cpp          # Block model data structures
│   └── IO.cpp             # CSV input/output handling
├── include/
│   ├── Strategy.hpp       # Strategy class declarations
│   ├── Model.hpp          # BlockDesc, ParentBlock, LabelTable
│   └── IO.hpp             # Endpoint for CSV I/O
├── data/
│   └── *.csv              # Test datasets
├── tests/
│   └── output.txt         # Test output
├── bin/
│   ├── compressor         # Native executable
│   └── compressor-mac.exe # Windows executable
└── Makefile               # Build system
```

## Usage

### Basic Usage

```bash
# Compress a dataset
cat data/input.csv | ./bin/compressor > output.csv

# Count output blocks
wc -l < output.csv
```

### Switching Algorithms

Edit `compressor/src/main.cpp` line 16:

```cpp
// For competition (best compression)
Strategy::SmartMergeStrat strat;

// For fast testing (10 seconds)
Strategy::ScanlineStrat strat;

// For ultra-fast (1 second)
Strategy::GreedyStrat strat;

// For maximum quality in 1 minute
Strategy::Optimal3DStrat strat;
```

Then rebuild:
```bash
# Windows
make clean && make build-exe

# MacOS
make clean && make build-exe-mac

```
## Documentation

Detailed documentation available:

- **[ALGORITHM_COMPARISON.md](docs/ALGORITHM_COMPARISON.md)** - Complete benchmark analysis and recommendations
- **[benchmark_results.md](docs/benchmark_results.md)** - Raw benchmark data
- **[class_diagram.png](docs/class_diagram.png)** - Class Diagram
- **[sequence_prototype.png](docs/sequence_prototype.png)** - Sequence Diagram


## Algorithm Details

### SmartMerge Ensemble Strategy

SmartMerge runs 5 algorithms **per material label** and picks the winner:

1. **Optimal3DStrat** - Enhanced MaxRect with Z-stacking
2. **MaxRectStrat** - 2D MaxRect per slice
3. **ScanlineStrat** - Linear sweep 
4. **GreedyStrat** - Fast row merging
5. **LayeredSliceStrat** - Z-first grouping

**Key Insight**: Different geological patterns favor different algorithms:
- Layered sediments → LayeredSliceStrat wins
- Orthogonal veins → ScanlineStrat wins
- Large ore bodies → Optimal3D/MaxRect wins
- Scattered voxels → GreedyStrat wins

This **adaptive per-label selection** achieves 10-12% better compression than any single algorithm.

### Time Complexity

| Algorithm | Time Complexity | Space Complexity |
|-----------|----------------|------------------|
| GreedyStrat | O(XYZ) | O(XYZ) |
| RLEXYStrat | O(XYZ) | O(XYZ) |
| ScanlineStrat | O(XYZ) | O(XY) |
| MaxRectStrat | O(XYZ log Z) | O(XYZ) |
| Optimal3DStrat | O(XYZ log Z) | O(XYZ) |
| LayeredSliceStrat | O(XYZ log Z) | O(XYZ) |
| SmartMergeStrat | O(5 × XYZ log Z) | O(5 × XYZ) |

Where X, Y, Z are parent block dimensions (typically 256×256×256).


## Performance Tips

### For Maximum Compression (Competition)
Use **SmartMergeStrat** (default in `main.cpp`)
- 236,087 blocks (best quality)
- ~2.5 minutes execution time
- Worth the wait for competition

### For Fast Iteration (Development)
Use **ScanlineStrat**
- 270,104 blocks (only 14% more)
- ~10 seconds (15× faster)
- Best for rapid testing

### For Real-Time Applications
Use **GreedyStrat**
- 270,343 blocks (good quality)
- ~1.3 seconds (fastest practical)
- Great for interactive tools




