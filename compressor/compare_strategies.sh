#!/bin/bash

# Strategy Comparison Script
# Compares all compression strategies on the test input

echo "=================================="
echo "Compression Strategy Comparison"
echo "=================================="
echo ""

# Colors for output (if terminal supports it)
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

INPUT_FILE="tests/input.txt"
TEMP_DIR="tmp_strategy_test"
mkdir -p "$TEMP_DIR"

# Count input blocks (approximate - count non-empty data lines)
INPUT_LINES=$(grep -v "^$" "$INPUT_FILE" | grep -v "," | tail -n +2 | wc -l | tr -d ' ')
# For 8x8x8 with data, estimate total blocks
ESTIMATE_BLOCKS=$((8 * 8 * 8))

echo "Test Input: $INPUT_FILE"
echo "Estimated input blocks: $ESTIMATE_BLOCKS"
echo ""

# Array of strategies to test
# Format: "ClassName|DisplayName"
STRATEGIES=(
    "DefaultStrat|Default (No Compression)"
    "GreedyStrat|Greedy (Fast)"
    "RLEXYStrat|RLE-XY (Balanced)"
    "MaxRectStrat|MaxRect (Best Compression)"
)

# Function to update main.cpp with a specific strategy
update_strategy() {
    local class_name=$1
    sed -i.bak "s/Strategy::[A-Za-z]*Strat strat;/Strategy::${class_name} strat;/" src/main.cpp
}

# Function to test a strategy
test_strategy() {
    local class_name=$1
    local display_name=$2
    local output_file="$TEMP_DIR/output_${class_name}.txt"

    echo -e "${BLUE}Testing: $display_name${NC}"
    echo "----------------------------------------"

    # Update strategy in main.cpp
    update_strategy "$class_name"

    # Rebuild
    make bin/compressor > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo -e "${YELLOW}⚠ Build failed for $class_name${NC}"
        echo ""
        return
    fi

    # Run and time it
    START_TIME=$(date +%s%N)
    cat "$INPUT_FILE" | ./bin/compressor > "$output_file" 2>&1
    END_TIME=$(date +%s%N)

    # Calculate execution time in milliseconds
    ELAPSED_MS=$(( (END_TIME - START_TIME) / 1000000 ))

    # Count output blocks
    OUTPUT_BLOCKS=$(wc -l < "$output_file" | tr -d ' ')

    # Calculate compression ratio
    if [ "$OUTPUT_BLOCKS" -gt 0 ]; then
        COMPRESSION=$(echo "scale=2; $ESTIMATE_BLOCKS / $OUTPUT_BLOCKS" | bc)
    else
        COMPRESSION="N/A"
    fi

    # Calculate reduction percentage
    if [ "$OUTPUT_BLOCKS" -gt 0 ]; then
        REDUCTION=$(echo "scale=1; (($ESTIMATE_BLOCKS - $OUTPUT_BLOCKS) * 100) / $ESTIMATE_BLOCKS" | bc)
    else
        REDUCTION="N/A"
    fi

    # Display results
    echo "  Output blocks:    $OUTPUT_BLOCKS"
    echo "  Compression:      ${COMPRESSION}x"
    echo "  Size reduction:   ${REDUCTION}%"
    echo "  Execution time:   ${ELAPSED_MS}ms"

    # Show first few output lines as sample
    echo "  Sample output:"
    head -3 "$output_file" | sed 's/^/    /'

    echo ""
}

# Save original main.cpp
cp src/main.cpp src/main.cpp.backup

# Test each strategy
for strategy in "${STRATEGIES[@]}"; do
    IFS='|' read -r class_name display_name <<< "$strategy"
    test_strategy "$class_name" "$display_name"
done

# Restore original main.cpp (keep MaxRectStrat as default)
update_strategy "MaxRectStrat"
make bin/compressor > /dev/null 2>&1

echo "=================================="
echo "Summary"
echo "=================================="
echo ""
echo "Strategy Ranking:"
echo ""
echo "🏆 COMPRESSION (fewer output blocks = better):"
for strategy in "${STRATEGIES[@]}"; do
    IFS='|' read -r class_name display_name <<< "$strategy"
    output_file="$TEMP_DIR/output_${class_name}.txt"
    if [ -f "$output_file" ]; then
        blocks=$(wc -l < "$output_file" | tr -d ' ')
        printf "   %-30s %5d blocks\n" "$display_name" "$blocks"
    fi
done | sort -k2 -n

echo ""
echo "⚡ SPEED (lower time = better):"
echo "   Run the script with /usr/bin/time for detailed stats"

echo ""
echo "Files saved in: $TEMP_DIR/"
echo "Backup saved: src/main.cpp.backup"
echo ""
echo -e "${GREEN}✓ Comparison complete!${NC}"
