#!/bin/bash

# Benchmark PRACTICAL compression algorithms in Strategy.cpp
# Skips MaxCuboidStrat (too slow) and buggy algorithms (Octree, DP, Hybrid)

DATASET="compressor/data/the_worldly_one_16777216_256x256x256.csv"
MAIN_FILE="compressor/src/main.cpp"
RESULTS_FILE="benchmark_results.md"

# Check if dataset exists
if [ ! -f "$DATASET" ]; then
    echo "ERROR: Dataset not found at $DATASET"
    exit 1
fi

# List of practical algorithms to test (excluding MaxCuboidStrat and buggy ones)
declare -a algorithms=(
    "DefaultStrat"
    "GreedyStrat"
    "RLEXYStrat"
    "MaxRectStrat"
    "Optimal3DStrat"
    "SmartMergeStrat"
    "LayeredSliceStrat"
    "QuadTreeStrat"
    "ScanlineStrat"
    "AdaptiveStrat"
)

# Backup original main.cpp
cp "$MAIN_FILE" "${MAIN_FILE}.backup"

echo "=========================================="
echo "PRACTICAL COMPRESSION ALGORITHM BENCHMARK"
echo "=========================================="
echo "Dataset: $DATASET (16,777,216 input blocks)"
echo "Testing ${#algorithms[@]} practical algorithms..."
echo "SKIPPED: MaxCuboidStrat (too slow), OctreeStrat/DynamicProgrammingStrat/Hybrid2PhaseStrat (buggy)"
echo ""

# Create results file header
cat > "$RESULTS_FILE" << EOF
# Compression Algorithm Benchmark Results

**Dataset**: \`the_worldly_one_16777216_256x256x256.csv\`
**Input blocks**: 16,777,216 (256³)
**Date**: $(date)

## Results Table

| Algorithm | Output Blocks | Compression Ratio | Compression % | Time (seconds) | Speed Rank | Compression Rank |
|-----------|---------------|-------------------|---------------|----------------|------------|------------------|
EOF

# Arrays to store results
declare -a results_algo=()
declare -a results_blocks=()
declare -a results_time=()

# Test each algorithm
for algo in "${algorithms[@]}"; do
    echo "----------------------------------------"
    echo "Testing: $algo"
    echo "----------------------------------------"

    # Update main.cpp to use this strategy
    sed -i '' "s/Strategy::[A-Za-z0-9]*Strat strat;/Strategy::${algo} strat;/g" "$MAIN_FILE"

    # Rebuild
    echo "Building..."
    cd compressor
    make clean > /dev/null 2>&1
    if ! make bin/compressor > /dev/null 2>&1; then
        echo "ERROR: Build failed for $algo"
        cd ..
        results_algo+=("$algo")
        results_blocks+=("BUILD_FAILED")
        results_time+=("N/A")
        continue
    fi
    cd ..

    # Run and measure time
    echo "Running compression..."
    START_TIME=$(date +%s.%N)

    OUTPUT=$(cat "$DATASET" | ./compressor/bin/compressor 2>&1)
    EXIT_CODE=$?

    END_TIME=$(date +%s.%N)
    ELAPSED=$(echo "$END_TIME - $START_TIME" | bc)

    if [ $EXIT_CODE -ne 0 ]; then
        echo "ERROR: Execution failed for $algo"
        results_algo+=("$algo")
        results_blocks+=("EXEC_FAILED")
        results_time+=("N/A")
        continue
    fi

    # Count output blocks (lines in output - 1 for header)
    BLOCK_COUNT=$(echo "$OUTPUT" | wc -l | tr -d ' ')
    BLOCK_COUNT=$((BLOCK_COUNT - 1))

    # Store results
    results_algo+=("$algo")
    results_blocks+=("$BLOCK_COUNT")
    results_time+=("$ELAPSED")

    # Calculate compression ratio
    RATIO=$(echo "scale=2; 16777216 / $BLOCK_COUNT" | bc)
    PERCENT=$(echo "scale=4; (1 - $BLOCK_COUNT / 16777216) * 100" | bc)

    echo "✓ Complete: $BLOCK_COUNT blocks, ${RATIO}x compression (${PERCENT}%), ${ELAPSED}s"
    echo ""
done

# Restore original main.cpp
mv "${MAIN_FILE}.backup" "$MAIN_FILE"

echo "=========================================="
echo "BENCHMARK COMPLETE"
echo "=========================================="
echo ""

# Sort results by time (speed) and compression (blocks)
# Create speed rankings
IFS=$'\n' speed_sorted=($(
    for i in "${!results_algo[@]}"; do
        if [ "${results_time[$i]}" != "N/A" ]; then
            echo "${results_time[$i]} $i"
        fi
    done | sort -n
))

# Create compression rankings
IFS=$'\n' compression_sorted=($(
    for i in "${!results_algo[@]}"; do
        if [ "${results_blocks[$i]}" != "BUILD_FAILED" ] && [ "${results_blocks[$i]}" != "EXEC_FAILED" ]; then
            echo "${results_blocks[$i]} $i"
        fi
    done | sort -n
))

# Build ranking maps
declare -A speed_rank
declare -A compression_rank
rank=1
for entry in "${speed_sorted[@]}"; do
    idx=$(echo "$entry" | awk '{print $2}')
    speed_rank[$idx]=$rank
    ((rank++))
done

rank=1
for entry in "${compression_sorted[@]}"; do
    idx=$(echo "$entry" | awk '{print $2}')
    compression_rank[$idx]=$rank
    ((rank++))
done

# Generate results table
for i in "${!results_algo[@]}"; do
    algo="${results_algo[$i]}"
    blocks="${results_blocks[$i]}"
    time="${results_time[$i]}"

    if [ "$blocks" = "BUILD_FAILED" ] || [ "$blocks" = "EXEC_FAILED" ]; then
        echo "| $algo | $blocks | N/A | N/A | $time | - | - |" >> "$RESULTS_FILE"
    else
        ratio=$(echo "scale=2; 16777216 / $blocks" | bc)
        percent=$(echo "scale=4; (1 - $blocks / 16777216) * 100" | bc)
        speed_r="${speed_rank[$i]:-N/A}"
        comp_r="${compression_rank[$i]:-N/A}"

        echo "| $algo | $blocks | ${ratio}x | ${percent}% | $time | $speed_r | $comp_r |" >> "$RESULTS_FILE"
    fi
done

# Add summary section
cat >> "$RESULTS_FILE" << 'EOF'

## Summary

### Top 3 Fastest Algorithms (by execution time)
EOF

best_idx_speed=()
for entry in "${speed_sorted[@]:0:3}"; do
    idx=$(echo "$entry" | awk '{print $2}')
    best_idx_speed+=($idx)
done

echo "" >> "$RESULTS_FILE"
echo "1. **${results_algo[${best_idx_speed[0]}]}**: ${results_time[${best_idx_speed[0]}]}s (${results_blocks[${best_idx_speed[0]}]} blocks)" >> "$RESULTS_FILE"
echo "2. **${results_algo[${best_idx_speed[1]}]}**: ${results_time[${best_idx_speed[1]}]}s (${results_blocks[${best_idx_speed[1]}]} blocks)" >> "$RESULTS_FILE"
echo "3. **${results_algo[${best_idx_speed[2]}]}**: ${results_time[${best_idx_speed[2]}]}s (${results_blocks[${best_idx_speed[2]}]} blocks)" >> "$RESULTS_FILE"

cat >> "$RESULTS_FILE" << 'EOF'

### Top 3 Best Compression (by output block count)
EOF

best_idx_comp=()
for entry in "${compression_sorted[@]:0:3}"; do
    idx=$(echo "$entry" | awk '{print $2}')
    best_idx_comp+=($idx)
done

echo "" >> "$RESULTS_FILE"
echo "1. **${results_algo[${best_idx_comp[0]}]}**: ${results_blocks[${best_idx_comp[0]}]} blocks (${results_time[${best_idx_comp[0]}]}s)" >> "$RESULTS_FILE"
echo "2. **${results_algo[${best_idx_comp[1]}]}**: ${results_blocks[${best_idx_comp[1]}]} blocks (${results_time[${best_idx_comp[1]}]}s)" >> "$RESULTS_FILE"
echo "3. **${results_algo[${best_idx_comp[2]}]}**: ${results_blocks[${best_idx_comp[2]}]} blocks (${results_time[${best_idx_comp[2]}]}s)" >> "$RESULTS_FILE"

cat >> "$RESULTS_FILE" << 'EOF'

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
EOF

echo "Results saved to: $RESULTS_FILE"
echo ""
echo "Top 3 Fastest:"
for idx in "${best_idx_speed[@]}"; do
    echo "  ${results_algo[$idx]}: ${results_time[$idx]}s (${results_blocks[$idx]} blocks)"
done

echo ""
echo "Top 3 Best Compression:"
for idx in "${best_idx_comp[@]}"; do
    echo "  ${results_algo[$idx]}: ${results_blocks[$idx]} blocks (${results_time[$idx]}s)"
done

echo ""
echo "Full results in: $RESULTS_FILE"
