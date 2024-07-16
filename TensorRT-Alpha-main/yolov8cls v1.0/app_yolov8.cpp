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


void task(YOLOV8& yolo, const ::utils::InitParameter& param, std::vector<cv::Mat>& imgsBatch, const int& delayTime, const int& batchi,
    const bool& isShow, const bool& isSave, const std::string& rtsp_url)
{ 
    ::utils::DeviceTimer d_t0; yolo.copy(imgsBatch); float t0 = d_t0.getUsedTime();
    ::utils::DeviceTimer d_t1; yolo.preprocess(imgsBatch); float t1 = d_t1.getUsedTime();
    ::utils::DeviceTimer d_t2; yolo.infer(); float t2 = d_t2.getUsedTime();

    sample::gLogInfo << 
        "preprocess time = " << t1 / param.batch_size << "; "
        "infer time = " << t2 / param.batch_size << "; ";

    // 获取分类结果
    std::vector<std::pair<int, float>> classifications = yolo.getClassifications();
    bool spray_detected = false;

    Json::Value root;
    for (const auto& classification : classifications) {
        int class_id = classification.first;
        float confidence = classification.second;

        Json::Value classification_json;
        std::string class_name = param.class_names[class_id];
        classification_json["label"] = class_name;
        classification_json["confidence"] = confidence;
        classification_json["rtsp"] = rtsp_url;
        root["classification"].append(classification_json);

        if (class_name == "rainy") {
            spray_detected = true;
        }
    }

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
    auto res2 = cli2.Post("/receive_data/weather", json_data, "application/json");
    if (res2 && res2->status == 200) {
        std::cout << "Data sent successfully to 192.168.137.1:8080" << std::endl;
    } else {
        std::cout << "Failed to send data to 192.168.137.1:8080" << std::endl;
    }

    if (spray_detected) {
        std::cout << "spray detected" << std::endl;
    }

    if (isShow)
        ::utils::showClassifications(classifications, param.class_names, delayTime, imgsBatch);
    yolo.reset();
}


void producer(cv::VideoCapture& capture) {
    while (keep_running) {
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
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 等待20毫秒后再尝试读取
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
            "{weather   || weather classify dataset}"
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

    if(parser.has("weather"))
    {
        param.class_names = ::utils::dataSets::weather_5;
        param.num_class = 5;
        model_path = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8cls v1.0/task_weather1.trt";
        sample::gLogInfo << "dataset = weather classify"  << std::endl;
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
    std::thread producer_thread(producer, std::ref(capture));

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
