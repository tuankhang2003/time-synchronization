import json
import time
import socket
import select
from main import *

# Sockets
master_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
master_sock.bind(("", MASTER_PORT))
master_ip = input("Master: ")

# Discover real outgoing IP
tmp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
tmp.connect((master_ip, MASTER_PORT))
real_ip = tmp.getsockname()[0]
tmp.close()


_, local_port = master_sock.getsockname()
print(f"computer ip {real_ip}:{local_port}")

start_msg = {TYPE_FIELD : Start_PTP, TIME_FIELD : 0, MESSAGE_FIELD: f"{real_ip}:{local_port}"}
start_bytes = json.dumps(start_msg).encode()
master_sock.sendto(start_bytes, (master_ip, MASTER_PORT))

print("Sent Start_PTP to master")

# Waiting for SYNC
sync_msg = receive_data(master_sock)
resp_type = sync_msg[TYPE_FIELD]
print(resp_type)
assert int(resp_type) == Sync
print(f'received sync from master {sync_msg}')

# Waiting for Follow Up
followup_msg = receive_data(master_sock)
assert int(followup_msg[TYPE_FIELD]) == Follow_Up
print(f'received follow_up from master {followup_msg}')

# Sending Delay Request
delay_request = {TYPE_FIELD : Delay_Req, TIME_FIELD : 0, MESSAGE_FIELD : ""}
delay_request_bytes = json.dumps(delay_request).encode()
master_sock.sendto(delay_request_bytes, (master_ip, MASTER_PORT))
print(f'sent delay_request to master: {delay_request}')
# Waiting for Delay Response
delay_response = receive_data(master_sock)
assert int(delay_response[TYPE_FIELD]) == Delay_Resp
print(f'received delay_resp from master {delay_response}')
