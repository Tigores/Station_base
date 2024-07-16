import signal
import sys
from fastapi import FastAPI
from fastapi.responses import JSONResponse
import subprocess
import threading
import time

app = FastAPI()

# 整合全局变量
task_person_status = {}
last_time = {}
task_smoking_p = {}
task_fire_p = {}
task_smoking_r = {}
task_fire_r = {}

app_run = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8v1.0/build/app_yolov8"
rtsps = [
    "--rtsp=rtsp://admin:lh911456@10.3.1.168:554/Streaming/Channels/102",
    "--rtsp=rtsp://admin:lh911456@10.3.1.168:554/Streaming/Channels/202",
    "--rtsp=rtsp://admin:lh911456@10.3.1.168:554/Streaming/Channels/302",
    "--rtsp=rtsp://admin:lh911456@10.3.1.168:554/Streaming/Channels/402",
    "--rtsp=rtsp://admin:lh911456@10.3.1.167:554/Streaming/Channels/102",
    "--rtsp=rtsp://admin:lh911456@10.3.1.167:554/Streaming/Channels/202",
    "--rtsp=rtsp://admin:lh911456@10.3.1.167:554/Streaming/Channels/302",
    # 添加更多的 RTSP 流
]
task_person = "--coco"
task_smoking = "--smoking"
task_fire = "--fire_smoke"

# 要使用的 RTSP 流索引，从1开始
use_rtsps = [2, 3]

# 全局进程列表
all_processes = []

def run_task(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        stdout, stderr = process.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate()
    return stdout

def start_task(command, task_p, stop_p, id, task_flag):
    global all_processes
    if stop_p:
        stop_p.terminate()
        stop_p.wait()
    if task_p is None or stop_p is not None:  # 只有在需要时才启动新进程
        task_p = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        all_processes.append(task_p)  # 将新启动的进程加入全局进程列表
        if task_flag == "task_smoking":
            task_smoking_r[id] = True
            task_fire_r[id] = False
        elif task_flag == "task_fire":
            task_fire_r[id] = True
            task_smoking_r[id] = False
    return task_p

def task_person_handler(id, rtsp):
    global task_person_status, last_time, task_smoking_p, task_fire_p
    while True:
        result = run_task([app_run, rtsp, task_person])
        output = result
        
        previous_status = task_person_status.get(id)
        current_status = 0 if "person detected" in output else 1
        
        if previous_status != current_status:
            task_person_status[id] = current_status
            
            if current_status == 0:  # 假设 0 是检测到人的状态码
                last_time[id] = time.time()
                if task_smoking_p.get(id) is None:
                    task_smoking_p[id] = start_task([app_run, rtsp, task_smoking], task_smoking_p.get(id), task_fire_p.get(id), id, "task_smoking")
                if task_fire_p.get(id) is not None:
                    task_fire_p[id].terminate()
                    task_fire_p[id] = None
            elif current_status == 1:  # 假设 1 是未检测到人的状态码
                if task_fire_p.get(id) is None:
                    task_fire_p[id] = start_task([app_run, rtsp, task_fire], task_fire_p.get(id), task_smoking_p.get(id), id, "task_fire")
                if task_smoking_p.get(id) is not None:
                    task_smoking_p[id].terminate()
                    task_smoking_p[id] = None
        
        if last_time.get(id) and time.time() - last_time[id] > 1800:  # 30分钟
            if task_fire_p.get(id) is None:
                task_fire_p[id] = start_task([app_run, rtsp, task_fire], task_fire_p.get(id), task_smoking_p.get(id), id, "task_fire")
            if task_smoking_p.get(id) is not None:
                task_smoking_p[id].terminate()
                task_smoking_p[id] = None
            last_time[id] = None
        
        time.sleep(1)  # 每秒检查一次状态

def startup_event():
    for idx in use_rtsps:
        rtsp_idx = idx - 1  # 从1开始，所以减1来获取正确的索引
        rtsp = rtsps[rtsp_idx]
        id = str(idx)
        task_fire_p[id] = start_task([app_run, rtsp, task_fire], None, None, id, "task_fire")
        threading.Thread(target=task_person_handler, args=(id, rtsp), daemon=True).start()

@app.get("/status")
async def get_status():
    global task_person_status, task_smoking_r, task_fire_r
    return JSONResponse(content={
        "task_person_status": task_person_status,
        "task_smoking_r": task_smoking_r,
        "task_fire_r": task_fire_r
    })

def shutdown_event():
    global all_processes
    for process in all_processes:
        process.terminate()
    sys.exit(0)

if __name__ == "__main__":
    app.add_event_handler("startup", startup_event)
    signal.signal(signal.SIGINT, lambda s, f: shutdown_event())
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)

