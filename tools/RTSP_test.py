import cv2
import time

# 设置RTSP流地址
rtsp_url = "rtsp://admin:lh911456@10.3.1.168:554/Streaming/Channels/402"
camera = 0
# 打开RTSP流
cap = cv2.VideoCapture(rtsp_url)

if not cap.isOpened():
    print("无法打开RTSP流")
    exit()

# frame_width = 1024
# frame_height = 512
# cap.set(cv2.CAP_PROP_FRAME_WIDTH, frame_width)
# cap.set(cv2.CAP_PROP_FRAME_HEIGHT, frame_height)

frame_count = 0  # 初始化帧计数器
fps = 0
start_time = time.time()  # 记录开始时间

# 循环捕捉视频帧
while True:
    ret, frame = cap.read()
    if not ret:
        print("无法读取视频帧")
        break

    frame_count += 1  # 递增帧计数器

    # 获取当前时间
    current_time = time.time()
    elapsed_time = current_time - start_time

    if elapsed_time >= 1.0:  # 每秒更新一次FPS
        fps = frame_count / elapsed_time
        frame_count = 0  # 重置帧计数器
        start_time = current_time  # 重置开始时间

    # 在帧上显示FPS
    cv2.putText(frame, f'FPS: {fps:.2f}', (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2, cv2.LINE_AA)

    # 显示视频帧
    cv2.imshow('Video', frame)

    # 按下 'q' 键退出
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# 释放资源
cap.release()
cv2.destroyAllWindows()
