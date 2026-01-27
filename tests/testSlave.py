from main import *
import json


# Sockets
slave_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
slave_sock.bind(("", SLAVE_PORT))
slave_ip = input("Slave IP: ")

# Discover real outgoing IP
tmp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
tmp.connect((slave_ip, SLAVE_PORT))
real_ip, local_port = tmp.getsockname()
tmp.close()

print("START PTP")
start_msg = {TYPE_FIELD : Start_PTP, TIME_FIELD : 0, MESSAGE_FIELD: f"{real_ip}:{local_port}"}
send_json_data(start_msg, slave_sock, slave_ip, SLAVE_PORT)
print("sent sync")
sync_msg = {'type': 1, 'time': 0, 'message': ''}
send_json_data(sync_msg, slave_sock, slave_ip, SLAVE_PORT)


followUp_msg = {'type': 2, 'time': 37200375, 'message': ''}
send_json_data(followUp_msg, slave_sock, slave_ip, SLAVE_PORT)

# Receiving delay request
delayRequest_msg = receive_data(slave_sock)
print(f'received {delayRequest_msg}')
assert int(delayRequest_msg[TYPE_FIELD]) == Delay_Req

delay_resp = {'type': 4, 'time': 37409994, 'message': ''}
send_json_data(delay_resp, slave_sock, slave_ip, SLAVE_PORT)


