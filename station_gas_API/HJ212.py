#!/usr/bin/python3
#coding=utf-8
import time
import sys,time,socket
import re

'''
    由数据字典 生成 HJ212_2017 协议字符串
'''
def encode(DIC_HJ212_2017):
    _data=''
    for key,value in DIC_HJ212_2017.items():
        if(key=='CP'):
            _data+='CP=&&'
            for cp_i,cp_item in DIC_HJ212_2017[key].items():                        
                for cp_key,cp_value in DIC_HJ212_2017[key][cp_i].items():
                    _data+=cp_key+'='+str(cp_value)+','
                _data=_data[0:-1] 
                _data+=';'    
            _data=_data[0:-1]                        
            _data+='&&'                        
        else:        
            _data+=key+'='+str(value)+';'

    _head='##'
    _length=len(_data)        
    STR_HJ212_2017=''
    STR_HJ212_2017+=_head
    STR_HJ212_2017+=str(_length).zfill(4)
    STR_HJ212_2017+=_data
    STR_HJ212_2017+=str(crc16(_data)).zfill(4).upper();
    STR_HJ212_2017+='\r\n'
    return STR_HJ212_2017 



'''
    由HJ212_2017 协议字符串数据 生成 字典
'''

def decode(data):
    DIC_HJ212_2017={}
    DIC_HJ212_2017['HEAD']=data[0:2]
    DIC_HJ212_2017['LENGTH']=data[2:6]
    DIC_HJ212_2017['CRC']=data[-6:-2]
    
    DIC_HJ212_2017['DATA']={}
    DIC_HJ212_2017['DATA']['CP']={}    
    
    _d0=data[6:-6].split('&&')
    
    _d1=_d0[0][0:-4].split(';')    
    for _d2 in _d1:
        _d3=_d2.split('=')        
        DIC_HJ212_2017['DATA'][_d3[0]]=_d3[1]   

    _d4=_d0[1].split(';')
    for _d5 in _d4:
        _d6=_d5.split(',')
        for _d7 in _d6:
            _d8=_d7.split('=')
            DIC_HJ212_2017['DATA']['CP'][_d8[0]]=_d8[1]
    _data=data[6:-6]    

    if(int(DIC_HJ212_2017['LENGTH'])==len(_data)):
        print('数据长度验证通过')

    else:
        print('数据长度验证未通过')  


    if(DIC_HJ212_2017['CRC']==str(crc16(_data)).zfill(4).upper()):
        print('CRC校验通过')  
    else:
        print('CRC校验未通过') 

    #print(DIC_HJ212_2017)  调试输出 结构为双层字典，类似JAVA对象  
    return DIC_HJ212_2017


'''
    crc16效验

'''
def crc16(text):
    data = bytearray(text, encoding='utf-8')
    crc = 0xffff
    dxs = 0xa001
    for i in range(len(data)):
        hibyte = crc >> 8
        crc = hibyte ^ data[i]
        for j in range(8):
            sbit = crc & 0x0001
            crc = crc >> 1
            if sbit == 1:
                crc ^= dxs
    return hex(crc)[2:]

'''
    QN=年月日时分秒毫秒
'''
def get_time_stamp():
    timestamp=time.time()
    local_time = time.localtime(timestamp)
    data_head = time.strftime("%Y%m%d%H", local_time)+'0000'
    
    dt_ms = "%s%03d" % (data_head, 0)
    # print(dt_ms)
    return dt_ms

def extract_idx_from_id(id):
    match = re.search(r'rtsp_(\d+)', id)
    if match:
        return int(match.group(1)) + 1
    else:
        return None

def getdata(data_list,pw,mn):
    rtsp, flag, name=data_list
    DIC_HJ212_2017={}            
    DIC_HJ212_2017['QN']=get_time_stamp()
    DIC_HJ212_2017['ST']='22'
    DIC_HJ212_2017['CN']='3020'
    DIC_HJ212_2017['PW']=pw
    DIC_HJ212_2017['MN']=mn
    DIC_HJ212_2017['Flag']='5'
        
    DIC_HJ212_2017['CP']={}
    
    DIC_HJ212_2017['CP'][0]={}    
    DIC_HJ212_2017['CP'][0]['DataTime']=DIC_HJ212_2017['QN'][0:14]

    DIC_HJ212_2017['CP'][1]={}        
    DIC_HJ212_2017['CP'][1]['Event-Avg'] = flag
    DIC_HJ212_2017['CP'][1]['Event-Flag'] = 'N'

    DIC_HJ212_2017['CP'][2]={}        
    DIC_HJ212_2017['CP'][2]['VideoId-Avg'] = rtsp
    DIC_HJ212_2017['CP'][2]['VideoId-Flag'] = 'N'

    DIC_HJ212_2017['CP'][3]={}        
    DIC_HJ212_2017['CP'][3]['User-Avg'] = name
    DIC_HJ212_2017['CP'][3]['User-Flag'] = 'N'

    print(encode(DIC_HJ212_2017))
    return DIC_HJ212_2017

    

def init_socket():
    so = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    return so


def connect_server(so, host, port, message):
    so.connect((host, port))# 连接服务器端

    so.send(f"{message}".encode('utf8'))# 
    data = so.recv(1024)# 接收数据
    print(f"get message: {data}")
    so.close()# 关闭连接
    return '发送成功'

