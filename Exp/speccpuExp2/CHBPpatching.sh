#!/bin/bash

# Check arguments
if [ $# -ne 2 ]; then
    echo "Usage: $0 <original_yaml_file> <speccpu_directory>"
    exit 1
fi

ORIGINAL_YAML="$Chimera/CHBP/spec.yaml"
SPECCPU_DIR="speccpu_execute/binaries/rv64gcv-O3"

# Check if files exist
if [ ! -f "$ORIGINAL_YAML" ]; then
    echo "Error: YAML file does not exist: $ORIGINAL_YAML"
    exit 1
fi

if [ ! -d "$SPECCPU_DIR" ]; then
    echo "Error: speccpu directory does not exist: $SPECCPU_DIR"
    exit 1
fi

# Check if necessary commands and files exist
if [ ! -f "CHBP.py" ]; then
    echo "Error: CHBP.py does not exist in current directory"
    exit 1
fi

if [ ! -f "loadhash.sh" ]; then
    echo "Error: loadhash.sh does not exist in current directory"
    exit 1
fi

# Get all filenames in speccpu directory
echo "Scanning speccpu directory: $SPECCPU_DIR"
files=($(ls "$SPECCPU_DIR"))

if [ ${#files[@]} -eq 0 ]; then
    echo "Error: speccpu directory is empty"
    exit 1
fi

echo "Found ${#files[@]} files"

# Process each file
for file in "${files[@]}"; do
    # Remove possible path prefix, keep only filename
    filename=$(basename "$file")
    
    # Generate new yaml filename
    new_yaml="${filename}.yaml"
    
    echo "Processing: $filename -> $new_yaml"
    
    # Copy original yaml file
    cp "$ORIGINAL_YAML" "$new_yaml"
    
    sed -i.tmp "s/binaryname:.*/binaryname: $filename/" "$new_yaml"
    sed -i.tmp "s/Outputbinary:.*/Outputbinary: $filename/" "$new_yaml"
    
    # Remove temporary file
    rm -f "${new_yaml}.tmp"
    
    python3 $Chimera/CHBP/CHBP.py "$new_yaml"
    
    if [ $? -eq 0 ]; then
        echo "CHBP.py executed successfully"
    else
        echo "Warning: CHBP.py execution failed"
    fi
    
    $Chimera/CHBP/loadhash.sh faulttable
    
    # Check if previous command was successful
    if [ $? -eq 0 ]; then
        echo "loadhash.sh executed successfully"
    else
        echo "Warning: loadhash.sh execution failed"
    fi
    
    echo "----------------------------------------"
done

echo "All files processed successfully!"
