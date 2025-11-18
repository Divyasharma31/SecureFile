# SecureFile – Secure Virtual File System

**AES Encryption + Huffman Compression + Deduplication + Virtual Disk + GUI**

A complete Operating Systems project combining **security**, **compression**, and **storage virtualization**.

## Features
- Login system (admin/guest)
- AES-256 encryption
- Huffman coding compression
- SHA-256 deduplication
- 100MB virtual disk (`storage.bin`)
- Cache system for fast reads
- Beautiful GUI with live disk usage
- Upload, download, list, delete files

## Screenshots
![Login](screenshots/login.png)
![Main](screenshots/main.png)

## How to Run
```bash
git clone https://github.com/yourusername/SecureFile.git
cd SecureFile
chmod +x compile.sh
./compile.sh
python main.py