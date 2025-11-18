#!/bin/bash
echo "Building SecureFile Engine..."

mkdir -p compress

# Compile C programs
gcc compress/file_write_compressed.c -o compress/file_write_compressed -lssl -lcrypto
gcc compress/file_read_decompression.c -o compress/file_read_decompression
gcc compress/delete_file.c -o compress/delete_file

# Create 100MB storage pool
if [ ! -f storage.bin ]; then
    echo "Creating 100MB virtual hard disk (storage.bin)..."
    dd if=/dev/zero of=storage.bin bs=1M count=100 2>/dev/null
fi

mkdir -p cache

echo ""
echo "Build Complete!"
echo "Now run: python main.py"
echo "Login: admin / 123"