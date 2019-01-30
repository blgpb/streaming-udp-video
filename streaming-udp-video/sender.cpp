// This program opens a socket that listens to incoming UDP packets. When a
// video frame packet is received, it will be decoded and displayed in a GUI
// window.

#include <iostream>
#include <vector>
#include <string.h>
#include<ws2tcpip.h>
#include <thread>
#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"

#pragma comment(lib,"ws2_32.lib")

//#pragma once
// This is the maximum UDP packet size, and the buffer will be allocated for
// the max amount.
constexpr int kMaxPacketBufferSize = 65535;

class ProtocolData {
public:
	// Puts all of the relevant variables into a raw byte buffer which is
	// returned, ready to be sent as a packet over the network.
	virtual std::vector<unsigned char> PackageData() const = 0;

	// Unpacks a received data buffer into the appropriate variables, in
	// accordance to the specific protocol being implemented.
	virtual void UnpackData(
		const std::vector<unsigned char>& raw_bytes) = 0;
};

class SenderSocket {
public:
	SenderSocket(const std::string &receiver_ip, const int receiver_port);

	// TODO: add destructor to clear the socket
	// close(fd);

	void SendPacket(const std::vector<unsigned char> &data) const;

private:
	// The socket identifier (handle).
	int socket_handle_;

	// The struct that contains the receiver's address and port. This is set up
	// in the constructor.
	sockaddr_in receiver_addr_;
};  // SenderSocket

SenderSocket::SenderSocket(
	const std::string &receiver_ip, const int receiver_port) {

	socket_handle_ = socket(AF_INET, SOCK_DGRAM, 0);
	receiver_addr_.sin_family = AF_INET;
	receiver_addr_.sin_port = htons(receiver_port);
	receiver_addr_.sin_addr.s_addr = inet_addr(receiver_ip.c_str());
}

void SenderSocket::SendPacket(
	const std::vector<unsigned char> &data) const {

	sendto(
		socket_handle_,
		(const char *)data.data(),
		data.size(),
		0,
		const_cast<sockaddr*>(reinterpret_cast<const sockaddr*>(&receiver_addr_)),
		sizeof(receiver_addr_));
}


class ReceiverSocket {
public:
	// Creates a new socket and stores the handle.
	explicit ReceiverSocket(const int port_number);

	// Attempts to bind the socket on the port number provided to listen to
	// incoming UDP packets. If the socket could not be created (in the
	// constructor) or if the binding process fails, an error message will be
	// printed to stderr. The method returns true on success, false otherwise.
	const bool BindSocketToListen() const;

	// Waits for the next packet on the given port, and returns vector of bytes
	// (stored as unsigned chars) that contains the raw packet data.
	const std::vector<unsigned char> GetPacket() const;

private:
	// This buffer will be used to collect incoming packet data. It is only used
	// in the GetPacket() method.
	char buffer_[kMaxPacketBufferSize];

	// The port number that the socket will listen for packets on.
	const int port_;

	// The socket identifier (handle).
	int socket_handle_;
};  // ReceiverSocket

class VideoFrame {
public:
	// Default constructor (required) just makes an empty image.
	VideoFrame() {}

	// Initialize the video frame from an existing cv::Mat image.
	explicit VideoFrame(const cv::Mat& image) : frame_image_(image) {}

	// Initialize the video frame (image) from a buffer of raw bytes.
	explicit VideoFrame(const std::vector<unsigned char> frame_bytes);

	// Uses the underlying video/image/gui library to display the frame on the
	// user's screen. Only one frame can be displayed at a time, as all frames
	// will share the same GUI window.
	void Display() const;

	// Returns the raw byte representation of the given video frame. Singe image
	// compression to JPEG is also handled here to minimize the frame size.
	std::vector<unsigned char> GetJPEG() const;

private:
	cv::Mat frame_image_;
};

class VideoCapture {
public:
	// Initializes the OpenCV VideoCapture object by selecting the default
	// camera. If no camera is provided it will not be possile to read any video
	// frames.
	//
	// Specify whether or not the video being sent is displayed in a window, and
	// the scale = (0, 1] which will affect the size of the data.
	//The third parameter is used to define which webcamera to be captured.
	//If the camera in the laptop is chosen, camera = 0
	//If the additional camera webcamera is chosen, camera = 1
	VideoCapture(const bool show_video, const float scale, int camera);

	// Captures and returns a frame from the available video camera.
	//
	// If the show_video option was set to true, the frame will be displayed.
	//
	// NOTE: This method cannot be const, since the cv::VideoCapture object is
	// modified through a non-const method when getting a new frame from the
	// camera.
	VideoFrame GetFrameFromCamera();

private:
	// The OpenCV camera capture object. This is used to interface with a
	// connected camera and extract frames from it.
	cv::VideoCapture capture_;

	// The image scale should be between (0 and 1]. The image will be
	// downsampled by the given amount to reduce cost of sending the data.
	const float scale_;

	// Set to true to show the video.
	const bool show_video_;
};

VideoCapture::VideoCapture(const bool show_video, const float scale, int camera)
	: show_video_(show_video), scale_(scale), capture_(cv::VideoCapture(camera)) {

	// TODO: Verify that the scale is in the appropriate range.
}



// The name of the window in which all frames will be displayed. If a new frame
// is displayed, it will replace the previous frame displayed in that window.
static const std::string kWindowName = "Streaming Video";

// Delays thread execution for this amount of time after a frame is displayed.
// This prevents the window from being refreshed too often, which can cause
// display issues.
constexpr int kDisplayDelayTimeMS = 15;

// JPEG compression values.
static const std::string kJPEGExtension = ".jpg";
constexpr int kJPEGQuality = 60;


VideoFrame::VideoFrame(const std::vector<unsigned char> frame_bytes) {
	frame_image_ = cv::imdecode(frame_bytes, cv::IMREAD_COLOR);
}

void VideoFrame::Display() const {
	// Do nothing for empty images.
	if (frame_image_.empty()) {
		return;
	}
	cv::namedWindow(kWindowName, CV_WINDOW_NORMAL);
	cv::imshow(kWindowName, frame_image_);
	cv::waitKey(kDisplayDelayTimeMS);
}

std::vector<unsigned char> VideoFrame::GetJPEG() const {
	const std::vector<int> compression_params = {
		cv::IMWRITE_JPEG_QUALITY,
		kJPEGQuality
	};
	std::vector<unsigned char> data_buffer;
	cv::imencode(kJPEGExtension, frame_image_, data_buffer, compression_params);
	return data_buffer;
}

VideoFrame VideoCapture::GetFrameFromCamera() {
	if (!capture_.isOpened()) {
		std::cerr << "Could not get frame. Camera not available." << std::endl;
		return VideoFrame();
	}
	cv::Mat image;
	capture_ >> image;
	// If the image is being downsampled, resize it first.
	if (scale_ < 1.0) {
		cv::resize(image, image, cv::Size(0, 0), scale_, scale_);
	}

	//These codes is to offset the time difference betwenn two PCs.(Because it is hard to solve it in a correct way.)
	SYSTEMTIME sys;	GetLocalTime(&sys);
	FILETIME ft;
	int offset_sec = -2;
	int offset_ms = 10;
	SystemTimeToFileTime(&sys, &ft);
	*(__int64 *)(&ft) = *(__int64 *)(&ft) + ((__int64)offset_sec * 1000i64 + (__int64)offset_ms) * 10000i64;
	FileTimeToSystemTime(&ft, &sys);
	std::string hour = std::to_string(sys.wHour);
	std::string min = std::to_string(sys.wMinute);
	std::string sec = std::to_string(sys.wSecond);
	std::string ms = std::to_string(sys.wMilliseconds);
	std::string text = hour + ":" + min + ":" + sec + "." + ms;
	cv::putText(image, text, cv::Point2f(16, 100), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.6, cv::Scalar(0, 255, 0), 2);
	VideoFrame video_frame(image);
	if (show_video_) {
		video_frame.Display();
	}
	return video_frame;
}

class BasicProtocolData : public ProtocolData {
public:
	std::vector<unsigned char> PackageData() const;

	void UnpackData(
		const std::vector<unsigned char>& raw_bytes) override;

	// Sets the next video frame.
	void SetImage(const VideoFrame& image) {
		video_frame_ = image;
	}

	// Returns the video frame image.
	VideoFrame GetImage() const {
		return video_frame_;
	}

private:
	// The video frame received from the packet is stored here.
	VideoFrame video_frame_;
};

std::vector<unsigned char> BasicProtocolData::PackageData() const {
	return video_frame_.GetJPEG();
}

void BasicProtocolData::UnpackData(
	const std::vector<unsigned char>& raw_bytes) {

	video_frame_ = VideoFrame(raw_bytes);
}

ReceiverSocket::ReceiverSocket(const int port_number) : port_(port_number) {
	socket_handle_ = socket(AF_INET, SOCK_DGRAM, 0);
}

const bool ReceiverSocket::BindSocketToListen() const {
	if (socket_handle_ < 0) {
		std::cerr << "Binding failed. Socket was not initialized." << std::endl;
		return false;
	}
	// Bind socket's address to INADDR_ANY because it's only receiving data, and
	// does not need a valid address.
	sockaddr_in socket_addr;
	memset(reinterpret_cast<char*>(&socket_addr), 0, sizeof(socket_addr));
	socket_addr.sin_family = AF_INET;
	socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// Bind the socket to the port that will be listened on.
	socket_addr.sin_port = htons(port_);
	const int bind_status = bind(
		socket_handle_,
		reinterpret_cast<sockaddr*>(&socket_addr),
		sizeof(socket_addr));
	if (bind_status < 0) {
		std::cerr << "Binding failed. Could not bind the socket." << std::endl;
		return false;
	}
	return true;
}

const std::vector<unsigned char> ReceiverSocket::GetPacket() const {
	// Get the data from the next incoming packet.
	sockaddr_in remote_addr;
	socklen_t addrlen = sizeof(remote_addr);
	const int num_bytes = recvfrom(
		socket_handle_,
		const_cast<char*>(buffer_),
		kMaxPacketBufferSize,
		0,
		reinterpret_cast<sockaddr*>(&remote_addr),
		&addrlen);
	// Copy the data (if any) into the data vector.
	std::vector<unsigned char> data;
	if (num_bytes > 0) {
		data.insert(data.end(), &buffer_[0], &buffer_[num_bytes]);
	}
	return data;
}

void send1() {

	WORD socketVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(socketVersion, &wsaData) != 0)
	{
		exit(0);
	}

	const int port = 6000;
	
	//std::string ip_address = "127.0.0.1";  // Localhost
	std::string ip_address = "192.168.43.168";

	const SenderSocket socket(ip_address, port);
	std::cout << "Sending to " << ip_address
		<< " on port " << port << "." << std::endl;
	VideoCapture video_capture(false, 0.6, 0);
	BasicProtocolData protocol_data;
	while (true) {  // TODO: break out cleanly when done.
		protocol_data.SetImage(video_capture.GetFrameFromCamera());
		socket.SendPacket(protocol_data.PackageData());
	}
}

void send2() {

	WORD socketVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(socketVersion, &wsaData) != 0)
	{
		exit(0);
	}

	const int port = 5000;

	//std::string ip_address = "127.0.0.1";  // Localhost
	std::string ip_address = "192.168.43.168";

	const SenderSocket socket(ip_address, port);
	std::cout << "Sending to " << ip_address
		<< " on port " << port << "." << std::endl;
	VideoCapture video_capture(false, 0.6, 1);
	BasicProtocolData protocol_data;
	while (true) {  // TODO: break out cleanly when done.
		protocol_data.SetImage(video_capture.GetFrameFromCamera());
		socket.SendPacket(protocol_data.PackageData());
	}
}

void send3() {

	WORD socketVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(socketVersion, &wsaData) != 0)
	{
		exit(0);
	}

	const int port = 4000;

	//std::string ip_address = "127.0.0.1";  // Localhost
	std::string ip_address = "192.168.1.3";

	const SenderSocket socket(ip_address, port);
	std::cout << "Sending to " << ip_address
		<< " on port " << port << "." << std::endl;
	VideoCapture video_capture(false, 0.6, 2);
	BasicProtocolData protocol_data;
	while (true) {  // TODO: break out cleanly when done.
		protocol_data.SetImage(video_capture.GetFrameFromCamera());
		socket.SendPacket(protocol_data.PackageData());
	}
}

int main()
{
	//系统加入了多线程功能，可以同时调用不同的摄像头给指定的IP和端口发送视频
	//每个线程发送独立的视频数据，互不干扰
	//由于带参数的send写入下列函数会发生错误，故分别定义了send1函数 send2函数 send3函数
	std::thread send1(send1);
	std::thread send2(send2);
	std::thread send3(send3);

	send1.detach();
	send2.detach();
	send3.detach();

	system("pause");

	return 0;
}