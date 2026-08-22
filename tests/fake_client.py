import socket, struct, sys, time

HOST, PORT = sys.argv[1], int(sys.argv[2])
USER, PASSW = "testuser", "testpass"
DESKEY = bytes([1,2,3,4,5,6,7,8,9,10,11,12,13,14])

s = socket.create_connection((HOST, PORT), timeout=10)

login = b'\xe4' + DESKEY + USER.encode() + b'\x00' + PASSW.encode() + b'\x00'
s.sendall(login)
resp = s.recv(16)
print("login resp: %s" % resp.hex())
if not resp or resp[0] != 0xe5:
    print("LOGIN FAILED")
    sys.exit(1)

ecm_payload = bytes.fromhex("0001020304050607")
req = b'\x81' + struct.pack(">H", 0x2600) + bytes([0,0,0]) + struct.pack(">H", 0x1FFF) + ecm_payload
t0 = time.time()
s.sendall(req)
resp = s.recv(64)
dt = (time.time() - t0) * 1000
print("resp: %s  (%.0f ms)" % (resp.hex(), dt))
if resp and resp[0] == 0x80:
    print("DCW RECEBIDA:", resp[1:17].hex())
    print("SUCCESS")
else:
    print("SEM DCW")
    sys.exit(2)
