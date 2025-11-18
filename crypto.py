# AES‑GCM, 3DES, and DES encryption and decryption utilities

import os
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

def encrypt_file(input_path, password):
    key = password.ljust(32)[:32].encode()
    iv = os.urandom(16)
    cipher = Cipher(algorithms.AES(key), modes.CFB(iv), backend=default_backend())
    encryptor = cipher.encryptor()
    
    with open(input_path, 'rb') as f:
        data = f.read()
    encrypted = encryptor.update(data) + encryptor.finalize()
    
    enc_path = input_path + ".enc"
    with open(enc_path, 'wb') as f:
        f.write(iv + encrypted)
    return enc_path

def decrypt_file(enc_path, password):
    key = password.ljust(32)[:32].encode()
    with open(enc_path, 'rb') as f:
        iv = f.read(16)
        data = f.read()
    
    cipher = Cipher(algorithms.AES(key), modes.CFB(iv), backend=default_backend())
    decryptor = cipher.decryptor()
    decrypted = decryptor.update(data) + decryptor.finalize()
    
    out_path = enc_path.replace(".enc", ".decrypted")
    with open(out_path, 'wb') as f:
        f.write(decrypted)
    return out_path