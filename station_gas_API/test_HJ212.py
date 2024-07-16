from HJ212 import *



PW = '123456'
MN = 'VIDEO001'
HOST = '192.168.1.9'
PORT = 6005

dalist = [2, 1, 'unknown']
mess = encode(getdata(dalist, PW, MN))
x = connect_server(init_socket(),HOST,PORT,mess)