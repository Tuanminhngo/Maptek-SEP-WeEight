# Compression Algorithm Benchmark Results

**Dataset**: `the_worldly_one_16777216_256x256x256.csv`
**Input blocks**: 16,777,216 (256³)
**Date**: Fri Oct 24 20:12:08 ACDT 2025

## Results Table

| Algorithm | Output Blocks | Compression Ratio | Compression % | Time (seconds) | Speed Rank | Compression Rank |
|-----------|---------------|-------------------|---------------|----------------|------------|------------------|
| DefaultStrat | 16777215 | 1.00x | 0% | 5.559637000 | 3 | 10 |
| GreedyStrat | 270343 | 62.05x | 98.3900% | 1.327416000 | 2 | 5 |
| RLEXYStrat | 529327 | 31.69x | 96.8500% | 1.317324000 | 1 | 8 |
| MaxRectStrat | 263796 | 63.59x | 98.4300% | 48.024648000 | 7 | 2 |
| Optimal3DStrat | 263796 | 63.59x | 98.4300% | 47.931882000 | 6 | 3 |
| SmartMergeStrat | 236087 | 71.06x | 98.6000% | 151.348135000 | 10 | 1 |
| LayeredSliceStrat | 299606 | 55.99x | 98.2200% | 48.168523000 | 8 | 6 |
| QuadTreeStrat | 648621 | 25.86x | 96.1400% | 102.090950000 | 9 | 9 |
| ScanlineStrat | 270104 | 62.11x | 98.4000% | 9.932081000 | 4 | 4 |
| AdaptiveStrat | 307629 | 54.53x | 98.1700% | 26.637604000 | 5 | 7 |

## Summary

### Top 3 Fastest Algorithms (by execution time)

1. **RLEXYStrat**: 1.317324000s (529327 blocks)
2. **DefaultStrat**: 5.559637000s (16777215 blocks)
3. **DefaultStrat**: 5.559637000s (16777215 blocks)

### Top 3 Best Compression (by output block count)

1. **SmartMergeStrat**: 236087 blocks (151.348135000s)
2. **DefaultStrat**: 16777215 blocks (5.559637000s)
3. **DefaultStrat**: 16777215 blocks (5.559637000s)

## Recommendations

### For Speed-Critical Applications (< 5 seconds)
Use **GreedyStrat** or **RLEXYStrat** - Both complete in ~1-2 seconds with decent compression.

### For Maximum Compression (Best Ratio)
Use **SmartMergeStrat** - Ensemble approach that tries multiple strategies and picks the best result per label.

### For Balanced Performance (Good compression in reasonable time)
Use **MaxRectStrat** or **Optimal3DStrat** - Complete in ~45-50 seconds with excellent compression.

### For Competition Use
**SmartMergeStrat** is recommended as it achieved:
- 236,087 blocks (71.06x compression, 98.59%)
- Helped improve competition score from 99.1711% to 99.1715%
- Takes ~2-3 minutes but gives best results

## Algorithm Characteristics

- **DefaultStrat**: Baseline (no compression) - emits 1×1×1 per cell
- **GreedyStrat**: Fast horizontal+vertical merging - best for speed
- **RLEXYStrat**: Run-length encoding along X - fast but less effective than MaxRect
- **MaxRectStrat**: 2D MaxRect per slice + Z-stacking - excellent balance
- **Optimal3DStrat**: Enhanced MaxRect with better Z-stacking - slightly better than MaxRect
- **SmartMergeStrat**: Tries multiple strategies and picks best - maximum compression
- **LayeredSliceStrat**: Z-first approach - good for layered geological data
- **QuadTreeStrat**: Hierarchical 2D subdivision - adaptive to uniform regions
- **ScanlineStrat**: Left-to-right sweep - good for Manhattan-like structures
- **AdaptiveStrat**: Pattern-based strategy selection - adapts to data characteristics

## Excluded Algorithms

### MaxCuboidStrat
- **Status**: TOO SLOW (did not complete benchmark)
- **Reason**: Iterative globally optimal cuboid extraction takes hours on 256³ dataset
- **Use case**: Only for small parent blocks where maximum compression > speed

### Buggy Algorithms (Cause Overlapping Blocks)
- **OctreeStrat**: 3D hierarchical subdivision - produces overlapping blocks
- **DynamicProgrammingStrat**: Optimal tiling - produces overlapping blocks
- **Hybrid2PhaseStrat**: Coarse+fine approach - produces overlapping blocks

These three algorithms need debugging before they can be used in production.

## Notes

- Compression Ratio = Input Blocks / Output Blocks
- Compression % = (1 - Output/Input) × 100
- Speed Rank: 1 = fastest, higher = slower
- Compression Rank: 1 = best (fewest blocks), higher = worse
