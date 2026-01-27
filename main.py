import threading
import json
import time
import socket
import select
import argparse
import tkinter as tk
from tkinter import ttk

MASTER_PORT = 12345
SLAVE_PORT  = 12346
BUFFER_SIZE = 1024

Sync = 1
Follow_Up = 2
Delay_Req = 3
Delay_Resp = 4
Log_Msg = 5
Start_PTP = 6

TYPE_FIELD = "type"
TIME_FIELD = "time"
MESSAGE_FIELD = "message"


def receive_data(s):
    try:
        data, addr = s.recvfrom(BUFFER_SIZE)
        msg = json.loads(data.decode('utf-8'))
        print(f"Received from {addr}: {msg}")
    except json.JSONDecodeError:
        print("Invalid JSON from", addr)
        return None
    return msg

def send_json_data(msg, sock, dest_ip, dest_port):
    send_bytes = json.dumps(msg).encode()
    sock.sendto(send_bytes, (dest_ip, dest_port))



def ptp(slave_ip, master_ip):
    
    # Create main window
    root = tk.Tk()
    root.title("Master/Slave Logger")
    root.geometry("800x600")

    # Create a Treeview with 2 columns
    tree = ttk.Treeview(root, columns=("Slave", "Master"), show="headings")
    tree.heading("Slave", text="Slave")
    tree.heading("Master", text="Master")
    tree.column("Slave", width=280)
    tree.column("Master", width=280)
    tree.pack(fill=tk.BOTH, expand=True)
        
        
    # Thread-safe log function
    def log_message(slave_msg=None, master_msg=None):
        tree.insert("", tk.END, values=(slave_msg or "", master_msg or ""))
        # Auto-scroll to the bottom
        tree.yview_moveto(1)

    # Sockets
    master_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    slave_sock  = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    master_sock.bind(("", MASTER_PORT))
    slave_sock.bind(("", SLAVE_PORT))

    start_msg_for_master = {"type": Start_PTP, "time": 0, "message": f"{slave_ip}:{SLAVE_PORT}"}
    start_msg_for_slave = {"type": Start_PTP, "time": 0, "message": f"{master_ip}:{MASTER_PORT}"}

    send_json_data(start_msg_for_slave, slave_sock, slave_ip, SLAVE_PORT)
    time.sleep(1)
    send_json_data(start_msg_for_master, master_sock, master_ip, MASTER_PORT)    
    print("Sent Start_PTP to master and slave")

    # Listen on both sockets
    sockets = [master_sock, slave_sock]
    print("Waiting for messages...")

    master_msg = []
    slave_msg = []

    def receive_log_message():
        while True:
            rlist, _, _ = select.select(sockets, [], [])
            for s in rlist:
                data, addr = s.recvfrom(BUFFER_SIZE)
                addr_ip = addr[0]
                msg = json.loads(data.decode('utf-8', errors='ignore'))
                message = msg["message"]
                if slave_ip == str(addr_ip):
                    slave_msg.append(msg)
                    print(f"Received from slave: {msg}")
                    root.after(0, log_message, message, None)  # log in Slave column
                elif master_ip == str(addr_ip):
                    master_msg.append(msg)
                    print(f"Received from master: {msg}")
                    root.after(0, log_message, None, message)  # log in Master column
                    
                else:
                    print(f'Unknown sender {addr_ip}')
                    print(f'Message: {msg}')
                    
    threading.Thread(target=receive_log_message, daemon=True).start()
    root.mainloop()
        

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PTP Master/Slave script")
    parser.add_argument("-slave", required=True, help="Slave IP address")
    parser.add_argument("-master", required=True, help="Master IP address")
    args = parser.parse_args()
    slave_ip = args.slave
    master_ip = args.master
    print(f"Slave IP: {slave_ip}")
    print(f"Master IP: {master_ip}")
    ptp(slave_ip=slave_ip, master_ip=master_ip)