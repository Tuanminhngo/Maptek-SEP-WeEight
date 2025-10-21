#!/bin/bash

# Test compression strategies on the real world dataset
echo "========================================"
echo "Compression Test on Real Dataset"
echo "========================================"
echo ""

DATA_FILE="data/the_worldly_one_16777216_256x256x256.csv"

if [ ! -f "$DATA_FILE" ]; then
    echo "Error: Data file not found: $DATA_FILE"
    exit 1
fi

# Count total input blocks
TOTAL_BLOCKS=16777216
echo "Input: 256x256x256 = $TOTAL_BLOCKS blocks"
echo ""

# Test function
test_strategy() {
    local strategy=$1
    local name=$2

    echo "Testing: $name"
    echo "----------------------------------------"

    # Update main.cpp to use this strategy
    sed -i.bak "s/Strategy::[A-Za-z0-9]*Strat strat;/Strategy::${strategy} strat;/" src/main.cpp

    # Rebuild
    make bin/compressor > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "Build failed"
        return
    fi

    # Run and time
    echo "Running compression (this may take a minute)..."
    START=$(date +%s)
    cat "$DATA_FILE" | ./bin/compressor > output_${strategy}.txt 2>&1
    END=$(date +%s)
    ELAPSED=$((END - START))

    # Count output
    OUTPUT_BLOCKS=$(wc -l < output_${strategy}.txt | tr -d ' ')

    # Calculate metrics
    if [ "$OUTPUT_BLOCKS" -gt 0 ]; then
        RATIO=$(echo "scale=2; $TOTAL_BLOCKS / $OUTPUT_BLOCKS" | bc)
        REDUCTION=$(echo "scale=2; (($TOTAL_BLOCKS - $OUTPUT_BLOCKS) * 100) / $TOTAL_BLOCKS" | bc)
    else
        RATIO="N/A"
        REDUCTION="N/A"
    fi

    echo "Output blocks:    $OUTPUT_BLOCKS"
    echo "Compression:      ${RATIO}x"
    echo "Reduction:        ${REDUCTION}%"
    echo "Time:             ${ELAPSED}s"
    echo ""
}

# Save original
cp src/main.cpp src/main.cpp.test_backup

# Test strategies
test_strategy "GreedyStrat" "Greedy"
test_strategy "MaxRectStrat" "MaxRect"
test_strategy "Optimal3DStrat" "Optimal3D"

echo "========================================"
echo "Summary"
echo "========================================"
echo ""
echo "Ranking by compression (best first):"
for f in output_*.txt; do
    if [ -f "$f" ]; then
        blocks=$(wc -l < "$f" | tr -d ' ')
        name=$(echo "$f" | sed 's/output_//' | sed 's/Strat.txt//')
        printf "%-20s %10d blocks\n" "$name" "$blocks"
    fi
done | sort -k2 -n

echo ""
echo "Done! Output files: output_*.txt"
