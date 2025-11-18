import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk
import subprocess
import os
import shutil
from crypto import encrypt_file, decrypt_file

class SecureFileApp:
    def __init__(self, root):
        self.root = root
        self.root.title("SecureFile – Virtual File System")
        self.root.geometry("1000x700")
        self.root.configure(bg="#e6f7ff")
        
        self.user = None
        self.password = None
        
        self.login_screen()
    
    def login_screen(self):
        for w in self.root.winfo_children():
            w.destroy()
        
        title = tk.Label(self.root, text="SecureFile", font=("Arial", 28, "bold"), 
                        bg="#e6f7ff", fg="#d62976")
        title.pack(pady=40)
        
        subtitle = tk.Label(self.root, text="AES Encryption + Huffman Compression + Virtual Disk", 
                           font=("Arial", 14), bg="#e6f7ff", fg="#555")
        subtitle.pack(pady=10)
        
        frame = tk.Frame(self.root, bg="#ffffff", relief="ridge", bd=4)
        frame.pack(pady=30, padx=100)
        
        tk.Label(frame, text="Username:", font=("Arial", 14), bg="white").grid(row=0, column=0, pady=15, padx=20, sticky="e")
        self.user_entry = tk.Entry(frame, font=("Arial", 14), width=25)
        self.user_entry.grid(row=0, column=1, pady=15, padx=20)
        
        tk.Label(frame, text="Password:", font=("Arial", 14), bg="white").grid(row=1, column=0, pady=15, padx=20, sticky="e")
        self.pass_entry = tk.Entry(frame, font=("Arial", 14), width=25, show="*")
        self.pass_entry.grid(row=1, column=1, pady=15, padx=20)
        
        btn = tk.Button(self.root, text="LOGIN", font=("Arial", 16, "bold"), bg="#1890ff", fg="white",
                       width=20, height=2, command=self.login)
        btn.pack(pady=30)
        
        tk.Label(self.root, text="Demo Accounts:", fg="gray", bg="#e6f7ff").pack()
        tk.Label(self.root, text="admin / 123    |    guest / guest", fg="gray", bg="#e6f7ff").pack()
    
    def login(self):
        u = self.user_entry.get().strip()
        p = self.pass_entry.get()
        if u in ["admin", "guest", "student"] and p in ["123", "guest", "stud123", "admin123"]:
            self.user = u
            self.password = p
            self.main_screen()
        else:
            messagebox.showerror("Login Failed", "Wrong credentials!\nTry: admin / 123")
    
    def get_disk_info(self):
        total = 100 * 1024 * 1024  # 100 MB
        used = 0
        free = total
        files = 0
        
        if os.path.exists("storage.bin"):
            used = os.path.getsize("storage.bin")
            free = total - used
        if os.path.exists("vdisk.meta"):
            with open("vdisk.meta", "r") as f:
                lines = f.readlines()
                files = len([l for l in lines if "|" in l])
        
        return total, used, free, files
    
    def main_screen(self):
        for w in self.root.winfo_children():
            w.destroy()
        
        # Header
        header = tk.Frame(self.root, bg="#1890ff", height=100)
        header.pack(fill="x")
        header.pack_propagate(False)
        
        tk.Label(header, text=f"Welcome, {self.user.upper()}!", font=("Arial", 20, "bold"),
                bg="#1890ff", fg="white").pack(pady=20)
        
        # Disk Info Card
        info_frame = tk.Frame(self.root, bg="white", relief="solid", bd=1)
        info_frame.pack(pady=15, padx=50, fill="x")
        
        total, used, free, files = self.get_disk_info()
        
        tk.Label(info_frame, text="Virtual Disk Status", font=("Arial", 16, "bold"), bg="white", fg="#333").pack(anchor="w", padx=20, pady=10)
        
        info_text = f"Total Space: 100 MB  |  Used: {used//(1024*1024)} MB  |  Free: {free//(1024*1024)} MB  |  Files: {files}"
        tk.Label(info_frame, text=info_text, font=("Arial", 12), bg="white", fg="#555").pack(anchor="w", padx=20, pady=5)
        
        progress = (used / total) * 100
        bar = ttk.Progressbar(info_frame, length=400, value=progress)
        bar.pack(padx=20, pady=10)
        
        # Buttons
        btn_frame = tk.Frame(self.root, bg="#e6f7ff")
        btn_frame.pack(pady=20)
        
        buttons = [
            ("Upload File", self.upload, "#52c41a"),
            ("Download File", self.download, "#1890ff"),
            ("List Files", self.list_files, "#722ed1"),
            ("Delete File", self.delete, "#f5222d"),
        ]
        
        for i, (text, cmd, color) in enumerate(buttons):
            row, col = divmod(i, 2)
            tk.Button(btn_frame, text=text, font=("Arial", 14, "bold"), bg=color, fg="white",
                     width=18, height=2, command=cmd).grid(row=row, column=col, padx=30, pady=15)
        
        tk.Button(self.root, text="Logout", font=("Arial", 12), bg="#8c8c8c", fg="white", command=self.login_screen).pack(pady=10)
        
        # Output Log
        self.output = tk.Text(self.root, height=12, font=("Courier", 11), bg="#1e1e2e", fg="#00ffaa")
        self.output.pack(padx=30, pady=10, fill="both", expand=True)
        scrollbar = tk.Scrollbar(self.output)
        scrollbar.pack(side="right", fill="y")
        self.output.config(yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.output.yview)
        
        self.log("SecureFile Ready!")
        self.log("Virtual Disk: vdisk.meta | Storage: storage.bin (100 MB)")
        self.log("-" * 70)
    
    def log(self, text):
        self.output.insert("end", text + "\n")
        self.output.see("end")
    
    def upload(self):
        path = filedialog.askopenfilename(title="Select file to upload")
        if not path: return
        name = os.path.basename(path)
        
        self.log(f"Uploading: {name}")
        enc_path = encrypt_file(path, self.password)
        self.log("Encrypted with AES-256")
        
        result = subprocess.run([
            "./compress/file_write_compressed", "vdisk.meta", "storage.bin", enc_path
        ], capture_output=True, text=True)
        
        output = result.stdout + result.stderr
        self.log(output if output else "Uploaded & compressed!")
        self.log(f"Success: {name} stored in virtual disk")
        
        if os.path.exists(enc_path):
            os.remove(enc_path)
        self.root.after(1000, self.refresh_disk_info)
    
    def download(self):
        name = simpledialog.askstring("Download", "Enter filename exactly:")
        if not name: return
        
        self.log(f"Downloading: {name}")
        
        result = subprocess.run([
            "./compress/file_read_decompression", "vdisk.meta", "storage.bin", name
        ], capture_output=True, text=True)
        
        self.log(result.stdout + result.stderr)
        
        recovered = f"recovered_{name}"
        if os.path.exists(recovered):
            decrypted = decrypt_file(recovered, self.password)
            self.log(f"Decrypted! Saved as: {os.path.basename(decrypted)}")
            os.remove(recovered)
        else:
            self.log("File not found or wrong password!")
    
    def list_files(self):
        if os.path.exists("vdisk.meta"):
            with open("vdisk.meta", "r") as f:
                content = f.read()
            messagebox.showinfo("Your Files in Virtual Disk", content or "No files yet!")
        else:
            messagebox.showinfo("Empty", "No files uploaded")
    
    def delete(self):
        name = simpledialog.askstring("Delete", "Enter filename to delete:")
        if not name: return
        
        result = subprocess.run([
            "./compress/delete_file", "vdisk.meta", name
        ], capture_output=True, text=True)
        
        self.log(result.stdout + result.stderr)
        self.log(f"{name} deleted from virtual disk!")
        self.root.after(1000, self.refresh_disk_info)
    
    def refresh_disk_info(self):
        # Rebuild disk info
        self.main_screen()

if __name__ == "__main__":
    root = tk.Tk()
    app = SecureFileApp(root)
    root.mainloop()