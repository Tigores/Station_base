#include "../utils/yolo.h"
#include "yolov8.h"
#include <httplib.h>
#include <json/json.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace cv;
using namespace std;


// 队列、互斥锁和条件变量
std::queue<cv::Mat> frame_queue;
std::mutex queue_mutex;
std::condition_variable frame_available;
std::atomic<bool> keep_running(true);
const size_t max_queue_size = 3;  // 设置最大队列长度

void setParameters(::utils::InitParameter& initParameters)
{
    initParameters.class_names = ::utils::dataSets::weather_5;
    initParameters.num_class = 5; // for smoke&phone
    initParameters.batch_size = 1;
    initParameters.dst_h = 640;
    initParameters.dst_w = 640;
    initParameters.input_output_names = { "images",  "output0" };
    initParameters.conf_thresh = 0.25f;
    initParameters.iou_thresh = 0.45f;
    initParameters.save_path = "";
}

void manageJpgFiles(const std::string& directory) {
    std::vector<fs::path> jpg_files;
    
    // 遍历目录并找到所有jpg文件
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() == ".jpg") {
            jpg_files.push_back(entry.path());
        }
    }

    // 按时间排序，较早的文件在前
    std::sort(jpg_files.begin(), jpg_files.end(), [](const fs::path& a, const fs::path& b) {
        return fs::last_write_time(a) < fs::last_write_time(b);
    });

    // 如果文件数量超过300个，删除最旧的文件
    while (jpg_files.size() > 300) {
        fs::remove(jpg_files.front());
        jpg_files.erase(jpg_files.begin());
    }
}

std::string to_string_with_precision(float value, int precision = 2) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

void task(YOLOV8& yolo, const ::utils::InitParameter& param, std::vector<cv::Mat>& imgsBatch, const int& delayTime, const int& batchi,
    const bool& isShow, const bool& isSave, const std::string& rtsp_url)
{ 
    ::utils::DeviceTimer d_t0; yolo.copy(imgsBatch); float t0 = d_t0.getUsedTime();
    ::utils::DeviceTimer d_t1; yolo.preprocess(imgsBatch); float t1 = d_t1.getUsedTime();
    ::utils::DeviceTimer d_t2; yolo.infer(); float t2 = d_t2.getUsedTime();
    ::utils::DeviceTimer d_t3; yolo.postprocess(imgsBatch); float t3 = d_t3.getUsedTime();
    sample::gLogInfo << 
        "preprocess time = " << t1 / param.batch_size << "; "
        "infer time = " << t2 / param.batch_size << "; "
        "postprocess time = " << t3 / param.batch_size << std::endl;

    auto detected_objects = yolo.getObjectss();
    std::map<std::string, int> class_counts;
    bool person_detected = false;  // 添加一个标志以跟踪person的检测

    if (!detected_objects.empty()) {
        Json::Value root;
        for (const auto& batch : detected_objects) {
            for (const auto& obj : batch ) {               
                std::string class_name = param.class_names[obj.label];
                if (param.num_class != 80 || class_name == "person") {
                    Json::Value detection;
                    detection["label"] = class_name;
                    detection["left"] = to_string_with_precision(obj.left / param.src_w);
                    detection["top"] = to_string_with_precision(obj.top / param.src_h);
                    detection["right"] = to_string_with_precision(obj.right / param.src_w);
                    detection["bottom"] = to_string_with_precision(obj.bottom / param.src_h);
                    detection["confidence"] = to_string_with_precision(obj.confidence);     
                    detection["rstp"] = rtsp_url;         
                    root["detections"].append(detection);

                    if (class_counts.find(class_name) == class_counts.end()) {
                        class_counts[class_name] = 1;
                    } else {
                        class_counts[class_name]++;
                    }

                    // 如果检测到person，将标志设置为true
                    if (class_name == "person") {
                        person_detected = true;
                    }
                    if(isSave){
                        manageJpgFiles(param.save_path);  // 调用管理函数
                        ::utils::save(yolo.getObjectss(), param.class_names, param.save_path, imgsBatch, param.batch_size, batchi);
                    }
                }
            }
        }

        if (!root["detections"].empty()) {
            Json::StreamWriterBuilder writer;
            std::string json_data = Json::writeString(writer, root);

            httplib::Client cli1("http://10.3.1.180:8080");
            auto res1 = cli1.Post("/api/openApi/sendEvent", json_data, "application/json");            

            if (res1 && res1->status == 200) {
                std::cout << "Data sent successfully to 10.3.1.180:8080" << std::endl;
            } else {
                std::cout << "Failed to send data to 10.3.1.180:8080" << std::endl;
            }

            httplib::Client cli2("http://192.168.137.1:8080");
            auto res2 = cli2.Post("/receive_data", json_data, "application/json");

            if (res2 && res2->status == 200) {
                std::cout << "Data sent successfully to 192.168.137.1:8080" << std::endl;
            } else {
                std::cout << "Failed to send data to 192.168.137.1:8080" << std::endl;
            }
        }
    }

    for (const auto& count : class_counts) {
        std::cout << "Class: " << count.first << ", Count: " << count.second << std::endl;
    }

    if (person_detected) {
        std::cout << "person detected" << std::endl;  // 输出状态1
    }

    if(isShow)
        ::utils::show(yolo.getObjectss(), param.class_names, delayTime, imgsBatch);
    
    yolo.reset();
}

void saveImagesToDirectory(const std::vector<cv::Mat>& imgsBatch, int batchIndex) 
{    
    for (size_t i = 0; i < imgsBatch.size(); ++i) 
    {         
        std::ostringstream filename;         
        filename << "image_" << std::setw(4) << std::setfill('0') << batchIndex << "_" << std::setw(4) << std::setfill('0') << i << ".jpg"; 
        cv::imwrite(filename.str(), imgsBatch[i]);     
    }
}

void producer(cv::VideoCapture& capture, const std::string& rtsp_url) {
    while (keep_running) {
        if (!capture.isOpened()) {
            std::cerr << "Failed to open RTSP stream, retrying..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 重试前等待5秒
            capture.open(rtsp_url);
            continue;
        }

        cv::Mat frame;
        if (capture.read(frame)) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            // 清理队列以保持长度小于 max_queue_size
            while (frame_queue.size() >= max_queue_size) {
                frame_queue.pop();
            }
            frame_queue.push(frame);
            frame_available.notify_one();
        } else {
            std::cerr << "Failed to read frame, retrying..." << std::endl;
            capture.release();
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 重试前等待5秒
        }
    }
}

void consumer(YOLOV8& yolo, const ::utils::InitParameter& param, const int& delayTime, const bool& isShow, const bool& isSave, const std::string& rtsp_url) {
    int batchi = 0;
    while (keep_running) {
        std::vector<cv::Mat> imgsBatch;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            frame_available.wait(lock, []{ return !frame_queue.empty() || !keep_running; });
            while (!frame_queue.empty() && imgsBatch.size() < param.batch_size) {
                imgsBatch.push_back(frame_queue.front());
                frame_queue.pop();
            }
        }
        if (!imgsBatch.empty()) {
            task(yolo, param, imgsBatch, delayTime, batchi, isShow, isSave, rtsp_url);
            batchi++;
        }
    }
}

int main(int argc, char** argv)
{
    cv::CommandLineParser parser(argc, argv,
        {
            "{model 	  || tensorrt model file	   }"
            "{size      || image (h, w), eg: 640   }"
            "{batch_size|| batch size              }"
            "{video     || video's path			       }"
            "{img       || image's path			       }"
            "{cam_id    || camera's device id	     }"
            "{rtsp      || rtsp's url 	           }"
            "{show      || if show the result	     }"
            "{savePath  || save path, can be ignore}"
            "{coco      || using coco dataset      }"
            "{fire_smoke|| using fire_smoke dataset}"
            "{smoking   || using smoking dataset   }"
        });
    // parameters
    ::utils::InitParameter param;
    setParameters(param);
    // path
    std::string model_path = "../best.trt";
    std::string video_path = "../../data/people.mp4";
    std::string image_path = "/home/lihe/YOLO/TensorRT-Alpha-main/yolov8_http/image_0000_0001.jpg";
    std::string rtsp_url = "rtsp://admin:lh1014qq@192.168.137.144/Streaming/Channels/102"; // 使用RTSP URL
    // camera' id
    int camera_id = 0;

    // get input
    ::utils::InputStream source;
    source = ::utils::InputStream::RTSP;

    // update params from command line parser
    int size = -1; // w or h
    int batch_size = 1;
    bool is_show = false;
    bool is_save = false;
    if(parser.has("model"))
    {
        model_path = parser.get<std::string>("model");
        sample::gLogInfo << "model_path = " << model_path << std::endl;
    }
    if(parser.has("size"))
    {
        size = parser.get<int>("size");
        sample::gLogInfo << "size = " << size << std::endl;
        param.dst_h = param.dst_w = size;
    }
    if(parser.has("batch_size"))
    {
        batch_size = parser.get<int>("batch_size");
        sample::gLogInfo << "batch_size = " << batch_size << std::endl;
        param.batch_size = batch_size;
    }
    if(parser.has("video"))
    {
        source = ::utils::InputStream::VIDEO;
        video_path = parser.get<std::string>("video");
        sample::gLogInfo << "video_path = " << video_path << std::endl;
    }
    if(parser.has("img"))
    {
        source = ::utils::InputStream::IMAGE;
        image_path = parser.get<std::string>("img");
        sample::gLogInfo << "image_path = " << image_path << std::endl;
    }
    if(parser.has("cam_id"))
    {
        source = ::utils::InputStream::CAMERA;
        camera_id = parser.get<int>("cam_id");
        sample::gLogInfo << "camera_id = " << camera_id << std::endl;
    }
    if(parser.has("rtsp"))
    {
        source = ::utils::InputStream::RTSP;
        rtsp_url = parser.get<std::string>("rtsp");
        sample::gLogInfo << "rtsp_url = " << rtsp_url << std::endl;
    }
    if(parser.has("show"))
    {
        is_show = true;
        sample::gLogInfo << "is_show = " << is_show << std::endl;
    }
    if(parser.has("savePath"))
    {
        is_save = true;
        param.save_path = parser.get<std::string>("savePath");
        sample::gLogInfo << "save_path = " << param.save_path << std::endl;
    }

    if(parser.has("coco"))
    {
        param.class_names = ::utils::dataSets::coco80;
        param.num_class = 80;
        model_path = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8v1.0/task_coco.trt";
        sample::gLogInfo << "dataset = coco"  << std::endl;
    }
    if(parser.has("fire_smoke"))
    {
        param.class_names = ::utils::dataSets::fire_smoke;
        param.num_class = 2;
        model_path = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8v1.0/task_fire_smoke1.trt";
        sample::gLogInfo << "dataset = fire_smoke"  << std::endl;

        is_save = true;
        param.save_path = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8v1.0/fire_result/";
        sample::gLogInfo << "save_path = " << param.save_path << std::endl;
    }
    if(parser.has("smoking"))
    {
        param.class_names = ::utils::dataSets::smoke;
        param.num_class = 1;
        model_path = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8v1.0/task_smoking1.trt";
        sample::gLogInfo << "dataset = smoking"  << std::endl;

        is_save = true;
        param.save_path = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8v1.0/smoking_result/";
        sample::gLogInfo << "save_path = " << param.save_path << std::endl;
    }

    int total_batches = 0;
    int delay_time = 1;
    cv::VideoCapture capture;
    if (!::utils::setInputStream(source, image_path, video_path, rtsp_url, camera_id,
        capture, total_batches, delay_time, param))
    {
        sample::gLogError << "read the input data errors!" << std::endl;
        return -1;
    }

    YOLOV8 yolo(param);

    // read model
    std::vector<unsigned char> trt_file = ::utils::loadModel(model_path);
    if (trt_file.empty())
    {
        sample::gLogError << "trt_file is empty!" << std::endl;
        return -1;
    }
    // init model
    if (!yolo.init(trt_file))
    {
        sample::gLogError << "initEngine() ocur errors!" << std::endl;
        return -1;
    }
    yolo.check();

    // 创建并启动生产者线程
    std::thread producer_thread(producer, std::ref(capture), std::ref(rtsp_url));

    // 创建并启动消费者线程
    std::thread consumer_thread(consumer, std::ref(yolo), std::ref(param), delay_time, is_show, is_save, std::ref(rtsp_url));

    // 等待用户输入以停止运行
    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();
    keep_running = false;

    // 等待线程结束
    producer_thread.join();
    consumer_thread.join();

    cv::destroyAllWindows();

    return 0;
}
