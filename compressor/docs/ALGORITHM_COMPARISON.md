# Compression Algorithm Comparison - Complete Analysis

**Dataset**: `the_worldly_one_16777216_256x256x256.csv` (256³ blocks, 7 material labels)
**Input blocks**: 16,777,216
**Benchmark Date**: October 24, 2025

---

## Executive Summary

### 🏆 Best for Speed: **RLEXYStrat**
- **Time**: 1.32 seconds
- **Compression**: 529,327 blocks (31.69x, 96.85%)
- **Use case**: Speed-critical applications where <2s response time is required

### 🥇 Best for Compression: **SmartMergeStrat**
- **Blocks**: 236,087 (71.06x compression, 98.59%)
- **Time**: 151.35 seconds (~2.5 minutes)
- **Use case**: Competition submissions, maximum compression needed

### ⚖️ Best Balance: **ScanlineStrat**
- **Blocks**: 270,104 (62.11x compression, 98.40%)
- **Time**: 9.93 seconds
- **Use case**: Good compression in <10 seconds

---

## Complete Benchmark Results

| Rank | Algorithm | Output Blocks | Compression Ratio | Compression % | Time (s) | Speed Rank | Compression Rank |
|------|-----------|---------------|-------------------|---------------|----------|------------|------------------|
| 🥇 | **SmartMergeStrat** | **236,087** | **71.06x** | **98.59%** | 151.35 | 10 | **1** |
| 🥈 | **MaxRectStrat** | 263,796 | 63.59x | 98.43% | 48.02 | 7 | 2 |
| 🥉 | **Optimal3DStrat** | 263,796 | 63.59x | 98.43% | 47.93 | 6 | 2 |
| 4 | **ScanlineStrat** | 270,104 | 62.11x | 98.40% | 9.93 | 4 | 4 |
| 5 | **GreedyStrat** | 270,343 | 62.05x | 98.39% | 1.33 | **2** | 5 |
| 6 | **LayeredSliceStrat** | 299,606 | 55.99x | 98.22% | 48.17 | 8 | 6 |
| 7 | **AdaptiveStrat** | 307,629 | 54.53x | 98.17% | 26.64 | 5 | 7 |
| 8 | **RLEXYStrat** | 529,327 | 31.69x | 96.85% | **1.32** | **1** | 8 |
| 9 | **QuadTreeStrat** | 648,621 | 25.86x | 96.14% | 102.09 | 9 | 9 |
| 10 | **DefaultStrat** | 16,777,215 | 1.00x | 0.01% | 5.56 | 3 | 10 |

---

## Algorithm Categories

### 🚀 Speed Champions (< 2 seconds)

#### 1. RLEXYStrat - **1.32s** ⚡
- **Blocks**: 529,327 (96.85% compression)
- **How it works**: Run-length encoding along X-axis + vertical merging
- **Best for**: Real-time applications, interactive tools
- **Weakness**: Lower compression than MaxRect-based approaches

#### 2. GreedyStrat - **1.33s** ⚡
- **Blocks**: 270,343 (98.39% compression)
- **How it works**: Fast row-by-row horizontal merging + vertical stacking
- **Best for**: Quick compression with good quality
- **Strength**: Excellent speed-to-compression ratio

### 💎 Compression Champions (< 250k blocks)

#### 1. SmartMergeStrat - **236,087 blocks** 🏆
- **Time**: 151.35s (~2.5 minutes)
- **How it works**: Ensemble that tries 5 strategies (Optimal3D, LayeredSlice, MaxRect, Greedy, Scanline) and picks best per label
- **Best for**: Competition submissions, final production compression
- **Success**: Improved competition score from 99.1711% to 99.1715%

#### 2. MaxRectStrat - **263,796 blocks**
- **Time**: 48.02s
- **How it works**: 2D maximal rectangle finding per Z-slice + Z-stacking
- **Best for**: Good compression in ~1 minute

#### 3. Optimal3DStrat - **263,796 blocks**
- **Time**: 47.93s (slightly faster than MaxRect!)
- **How it works**: Enhanced MaxRect with improved Z-stacking heuristics
- **Best for**: Same as MaxRect but with marginally better performance

### ⚖️ Balanced Algorithms (10-50 seconds)

#### 1. ScanlineStrat - **270,104 blocks in 9.93s** ⭐
- **Compression**: 98.40% (very close to MaxRect!)
- **How it works**: Left-to-right sweep with active rectangle tracking
- **Best for**: **Best balance** - near-MaxRect quality in 1/5th the time
- **Recommendation**: Consider using this instead of MaxRect for time-sensitive tasks

#### 2. AdaptiveStrat - **307,629 blocks in 26.64s**
- **Compression**: 98.17%
- **How it works**: Analyzes local patterns and picks strategy per region
- **Best for**: Mixed/heterogeneous datasets

#### 3. LayeredSliceStrat - **299,606 blocks in 48.17s**
- **Compression**: 98.22%
- **How it works**: Z-first grouping of identical XY slices
- **Best for**: Geological data with repeated layers

### ❌ Not Recommended

#### QuadTreeStrat - **648,621 blocks in 102.09s**
- **Issue**: Worst compression among practical algorithms AND slow
- **Reason**: 2D hierarchical subdivision doesn't exploit 3D structure well
- **Verdict**: Avoid using this

#### DefaultStrat - **16,777,215 blocks**
- **Purpose**: Baseline (1x1x1 per cell, no compression)
- **Use**: Testing only

---

## Strategy Selection Guide

### For Competition Submissions 🏆
**Use: SmartMergeStrat**
- Achieved 236,087 blocks (98.59% compression)
- Improved competition score by 0.0004% (99.1711% → 99.1715%)
- Takes ~2.5 minutes but worth it for best results

### For Production with Time Constraints ⏱️

**< 2 seconds**: Use **GreedyStrat**
- 270,343 blocks (98.39%) in 1.33s
- Best compression among ultra-fast algorithms

**< 10 seconds**: Use **ScanlineStrat** ⭐ RECOMMENDED
- 270,104 blocks (98.40%) in 9.93s
- Only 14% more blocks than SmartMerge
- 15× faster than SmartMerge
- **Best speed-to-quality ratio**

**< 1 minute**: Use **Optimal3DStrat**
- 263,796 blocks (98.43%) in 47.93s
- Very close to SmartMerge (only 11% more blocks)
- 3× faster than SmartMerge

### For Specific Data Types

**Geological/layered data**: Use **LayeredSliceStrat**
- 299,606 blocks in 48.17s
- Optimized for datasets with repeated Z-layers

**Heterogeneous data**: Use **AdaptiveStrat**
- 307,629 blocks in 26.64s
- Adapts to local data patterns

---

## Excluded Algorithms

### MaxCuboidStrat ⏳
- **Status**: TOO SLOW (did not complete benchmark in reasonable time)
- **Reason**: Iterative globally optimal cuboid extraction
- **Estimated time**: Hours to days for 256³ dataset
- **Use case**: ONLY for tiny parent blocks (< 32³) where maximum compression is critical

### Buggy Algorithms ⚠️
These cause "overlapping blocks" errors on competition platform:

1. **OctreeStrat** - 3D hierarchical octant subdivision
2. **DynamicProgrammingStrat** - Optimal rectangle tiling
3. **Hybrid2PhaseStrat** - Coarse + fine decomposition

**Status**: Need debugging before production use
**Issue**: Incomplete voxel mask clearing in recursive/multi-phase logic

---

## Key Insights

### Speed Analysis
1. **Fastest**: RLEXYStrat (1.32s) - but sacrifices 124% more blocks vs SmartMerge
2. **2nd Fastest**: GreedyStrat (1.33s) - excellent compression for the speed
3. **Slowest Practical**: SmartMergeStrat (151.35s) - but best compression

### Compression Analysis
1. **Best**: SmartMergeStrat (236,087 blocks) - worth the 2.5 minute wait
2. **2nd Best**: MaxRect/Optimal3D (263,796 blocks) - only 11% more blocks
3. **Worst Practical**: QuadTreeStrat (648,621 blocks) - avoid this

### Speed-to-Compression Sweet Spot
**ScanlineStrat** achieves 98.40% compression in just 9.93 seconds:
- Only 14% more blocks than SmartMerge
- 15× faster than SmartMerge
- Only 2.6% worse compression than MaxRect
- 4.8× faster than MaxRect

**Recommendation**: For production systems, use ScanlineStrat unless you have time for SmartMerge.

---

## SmartMergeStrat Configuration

Currently uses 5 strategies (lines 417-466 in [Strategy.cpp](compressor/src/Strategy.cpp:417-466)):

1. **Optimal3DStrat** - Enhanced Z-stacking (usually wins)
2. **LayeredSliceStrat** - Z-first for layered data
3. **MaxRectStrat** - Best for large uniform regions
4. **GreedyStrat** - Fast fallback
5. **ScanlineStrat** - Good for Manhattan structures

**Performance**: 236,087 blocks in 151.35s

### Potential Optimizations

#### Option A: Remove slower strategies (not helping much)
Remove LayeredSliceStrat (48s contribution, 6th place compression)
- Estimated time: ~103s (48s saved)
- Risk: Might lose marginal gains on specific labels

#### Option B: Add faster alternative
Add RLEXYStrat as 6th strategy (only +1.3s)
- Total time: ~152.6s
- Might help with specific linear patterns

#### Option C: Keep current configuration ✓
- Already achieves best compression
- All strategies contribute to ensemble
- Competition-proven results

---

## Recommendations Summary

| Use Case | Algorithm | Blocks | Time | Reason |
|----------|-----------|--------|------|--------|
| **Competition** | SmartMergeStrat | 236,087 | 151s | Best compression, proven results |
| **Production (balanced)** | ScanlineStrat | 270,104 | 10s | Excellent compression in <10s |
| **Production (quality)** | Optimal3DStrat | 263,796 | 48s | Near-SmartMerge quality in 1/3 time |
| **Production (speed)** | GreedyStrat | 270,343 | 1.3s | Best compression for ultra-fast |
| **Real-time** | RLEXYStrat | 529,327 | 1.3s | Fastest overall |
| **Geological data** | LayeredSliceStrat | 299,606 | 48s | Optimized for layers |

---

## Testing Methodology

1. Each algorithm tested individually on full dataset
2. Clean rebuild for each test (make clean && make)
3. Time measured with system timer (start to end)
4. Output blocks counted (lines - 1 for header)
5. Rankings calculated by sorting results

**System**: macOS (Darwin 25.0.0)
**Compiler**: Default system compiler
**Build flags**: From Makefile

---

## Next Steps

### For You (User)
1. **For competition**: Keep using SmartMergeStrat (current setup is optimal)
2. **For testing**: Use ScanlineStrat to iterate faster
3. **If time-constrained**: Use Optimal3DStrat (11% more blocks, 3× faster)

### For Future Development
1. Fix buggy algorithms (Octree, DP, Hybrid2Phase) to eliminate overlaps
2. Optimize MaxCuboidStrat for practical datasets (tiling approach?)
3. Consider hybrid: ScanlineStrat + targeted SmartMerge on difficult labels
4. Implement cross-parent-block merging for further gains

---

## File References

- Algorithm implementations: [Strategy.cpp](compressor/src/Strategy.cpp)
- Strategy declarations: [Strategy.hpp](compressor/include/Strategy.hpp)
- Main entry point: [main.cpp](compressor/src/main.cpp:16)
- Benchmark script: [benchmark_practical.sh](benchmark_practical.sh)
- Raw results: [benchmark_results.md](benchmark_results.md)
