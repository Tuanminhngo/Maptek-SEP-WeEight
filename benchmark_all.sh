#!/bin/bash

# Benchmark all compression algorithms in Strategy.cpp
# Tests each algorithm on the_worldly_one dataset and records:
# - Output block count (compression quality)
# - Execution time (speed)

DATASET="compressor/data/the_worldly_one_16777216_256x256x256.csv"
MAIN_FILE="compressor/src/main.cpp"
RESULTS_FILE="benchmark_results.md"

# Check if dataset exists
if [ ! -f "$DATASET" ]; then
    echo "ERROR: Dataset not found at $DATASET"
    exit 1
fi

# List of all algorithms to test
declare -a algorithms=(
    "DefaultStrat"
    "GreedyStrat"
    "RLEXYStrat"
    "MaxRectStrat"
    "Optimal3DStrat"
    "SmartMergeStrat"
    "MaxCuboidStrat"
    "LayeredSliceStrat"
    "QuadTreeStrat"
    "ScanlineStrat"
    "AdaptiveStrat"
    "OctreeStrat"
    "DynamicProgrammingStrat"
    "Hybrid2PhaseStrat"
)

# Backup original main.cpp
cp "$MAIN_FILE" "${MAIN_FILE}.backup"

echo "=========================================="
echo "COMPRESSION ALGORITHM BENCHMARK"
echo "=========================================="
echo "Dataset: $DATASET (16,777,216 input blocks)"
echo "Testing ${#algorithms[@]} algorithms..."
echo ""

# Create results file header
cat > "$RESULTS_FILE" << 'EOF'
# Compression Algorithm Benchmark Results

**Dataset**: `the_worldly_one_16777216_256x256x256.csv`
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

### Fastest Algorithms (by execution time)
EOF

echo "" >> "$RESULTS_FILE"
echo "1. **${results_algo[${speed_sorted[0]##* }]}**: ${results_time[${speed_sorted[0]##* }]}s" >> "$RESULTS_FILE"
echo "2. **${results_algo[${speed_sorted[1]##* }]}**: ${results_time[${speed_sorted[1]##* }]}s" >> "$RESULTS_FILE"
echo "3. **${results_algo[${speed_sorted[2]##* }]}**: ${results_time[${speed_sorted[2]##* }]}s" >> "$RESULTS_FILE"

cat >> "$RESULTS_FILE" << 'EOF'

### Best Compression (by output block count)
EOF

echo "" >> "$RESULTS_FILE"
echo "1. **${results_algo[${compression_sorted[0]##* }]}**: ${results_blocks[${compression_sorted[0]##* }]} blocks" >> "$RESULTS_FILE"
echo "2. **${results_algo[${compression_sorted[1]##* }]}**: ${results_blocks[${compression_sorted[1]##* }]} blocks" >> "$RESULTS_FILE"
echo "3. **${results_algo[${compression_sorted[2]##* }]}**: ${results_blocks[${compression_sorted[2]##* }]} blocks" >> "$RESULTS_FILE"

cat >> "$RESULTS_FILE" << 'EOF'

## Recommendations

- **For speed-critical applications**: Use the fastest algorithm
- **For maximum compression**: Use the best compression algorithm
- **For balanced performance**: Use SmartMergeStrat (ensemble of top strategies)

## Notes

- Compression Ratio = Input Blocks / Output Blocks
- Compression % = (1 - Output/Input) × 100
- Speed Rank: 1 = fastest, higher = slower
- Compression Rank: 1 = best (fewest blocks), higher = worse
EOF

echo "Results saved to: $RESULTS_FILE"
echo ""
echo "Top 3 Fastest:"
for entry in "${speed_sorted[@]:0:3}"; do
    idx=$(echo "$entry" | awk '{print $2}')
    echo "  ${results_algo[$idx]}: ${results_time[$idx]}s"
done

echo ""
echo "Top 3 Best Compression:"
for entry in "${compression_sorted[@]:0:3}"; do
    idx=$(echo "$entry" | awk '{print $2}')
    echo "  ${results_algo[$idx]}: ${results_blocks[$idx]} blocks"
done

echo ""
echo "Full results in: $RESULTS_FILE"
