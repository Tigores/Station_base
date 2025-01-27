#include"../utils/yolo.h"
#include"yolov8.h"

void setParameters(utils::InitParameter& initParameters)
{
	initParameters.class_names = utils::dataSets::weather_5;
	initParameters.num_class = 5; 
	initParameters.batch_size = 1;
	initParameters.dst_h = 640;
	initParameters.dst_w = 640;
	initParameters.input_output_names = { "images",  "output0" };
	initParameters.conf_thresh = 0.25f;
	initParameters.iou_thresh = 0.45f;
	initParameters.save_path = "";
}

void task(YOLOV8& yolo, const utils::InitParameter& param, std::vector<cv::Mat>& imgsBatch, const int& delayTime, const int& batchi,
	const bool& isShow, const bool& isSave)
{
	utils::DeviceTimer d_t0; yolo.copy(imgsBatch);	      float t0 = d_t0.getUsedTime();
	utils::DeviceTimer d_t1; yolo.preprocess(imgsBatch);  float t1 = d_t1.getUsedTime();
	utils::DeviceTimer d_t2; yolo.infer();				  float t2 = d_t2.getUsedTime();

	sample::gLogInfo <<
        "preprocess time = " << t1 / param.batch_size << "; "
        "infer time = " << t2 / param.batch_size << "; " << std::endl;
  
  // 获取分类结果
  std::vector<std::pair<int, float>> classifications = yolo.getClassifications();

	if (isShow)
      ::utils::showClassifications(classifications, param.class_names, delayTime, imgsBatch);
  if (isSave)
      ::utils::saveClassifications(classifications, param.class_names, param.save_path, imgsBatch, param.batch_size, batchi);
  yolo.reset();
}

int main(int argc, char** argv)
{
	cv::CommandLineParser parser(argc, argv,
		{
			"{model 	|| tensorrt model file	   }"
			"{size      || image (h, w), eg: 640   }"
			"{batch_size|| batch size              }"
			"{video     || video's path			   }"
			"{img       || image's path			   }"
			"{cam_id    || camera's device id	   }"
			"{show      || if show the result	   }"
			"{savePath  || save path, can be ignore}"
      "{weather   || weather classify dataset}"
		});
	// parameters
	utils::InitParameter param;
	setParameters(param);
	// path
	std::string model_path = "../task_weather1.trt";
	std::string video_path = "../../data/people.mp4";
	std::string image_path = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8cls_test/spray";
  std::string rtsp_url = "rtsp://admin:lh1014qq@192.168.137.144/Streaming/Channels/102"; // 使用RTSP URL
	// camera' id
	int camera_id = 0;

	// get input
	utils::InputStream source;
	source = utils::InputStream::IMAGE;
	//source = utils::InputStream::VIDEO;
	// source = utils::InputStream::CAMERA;

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
		source = utils::InputStream::VIDEO;
		video_path = parser.get<std::string>("video");
		sample::gLogInfo << "video_path = " << video_path << std::endl;
	}
	if(parser.has("img"))
	{
		source = utils::InputStream::IMAGE;
		image_path = parser.get<std::string>("img");
		sample::gLogInfo << "image_path = " << image_path << std::endl;
	}
	if(parser.has("cam_id"))
	{
		source = utils::InputStream::CAMERA;
		camera_id = parser.get<int>("cam_id");
		sample::gLogInfo << "camera_id = " << camera_id << std::endl;
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
      model_path = "/home/jetson/dxh/TensorRT-Alpha-main/yolov8cls v1.0/task_weather.trt";
      sample::gLogInfo << "dataset = weather classify"  << std::endl;
  }

	int total_batches = 0;
	int delay_time = 1;
	cv::VideoCapture capture;
	if (!setInputStream(source, image_path, video_path, rtsp_url, camera_id,
		capture, total_batches, delay_time, param))
	{
		sample::gLogError << "read the input data errors!" << std::endl;
		return -1;
	}

	YOLOV8 yolo(param);

	// read model
	std::vector<unsigned char> trt_file = utils::loadModel(model_path);
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
	cv::Mat frame;
	std::vector<cv::Mat> imgs_batch;
	imgs_batch.reserve(param.batch_size);
	sample::gLogInfo << imgs_batch.capacity() << std::endl;
	int batchi = 0;
	while (capture.isOpened())
	{
		if (batchi >= total_batches && source != utils::InputStream::CAMERA)
		{
			break;
		}
		if (imgs_batch.size() < param.batch_size) // get input
		{
			if (source != utils::InputStream::IMAGE)
			{
				capture.read(frame);
			}
			else
			{
				frame = cv::imread(image_path);
			}

			if (frame.empty())
			{
				sample::gLogWarning << "no more video or camera frame" << std::endl;
				task(yolo, param, imgs_batch, delay_time, batchi, is_show, is_save);
				imgs_batch.clear();
				batchi++;
				break;
			}
			else
			{
				imgs_batch.emplace_back(frame.clone());
			}
		}
		else
		{
			task(yolo, param, imgs_batch, delay_time, batchi, is_show, is_save);
			imgs_batch.clear();
			batchi++;
		}
	}
	return  -1;
}

