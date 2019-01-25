// This program opens a socket that listens to incoming UDP packets. When a
// video frame packet is received, it will be decoded and displayed in a GUI
// window.

#include <iostream>
#include <vector>
#include <string.h>
//#include <Winsock.h>
//#include <ws2tcpip.h>
#include<ws2tcpip.h>
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
	VideoCapture(const bool show_video, const float scale);

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

VideoCapture::VideoCapture(const bool show_video, const float scale)
	: show_video_(show_video), scale_(scale), capture_(cv::VideoCapture(0)) {

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
	constexpr int kJPEGQuality = 90;


VideoFrame::VideoFrame(const std::vector<unsigned char> frame_bytes) {
	frame_image_ = cv::imdecode(frame_bytes, cv::IMREAD_COLOR);
}

void VideoFrame::Display() const {
	// Do nothing for empty images.
	if (frame_image_.empty()) {
		return;
	}
	cv::namedWindow(kWindowName, CV_WINDOW_NORMAL);

	SYSTEMTIME sys;	GetLocalTime(&sys);

	std::string hour = std::to_string(sys.wHour);
	std::string min = std::to_string(sys.wMinute);
	std::string sec = std::to_string(sys.wSecond);
	std::string ms = std::to_string(sys.wMilliseconds);
	std::string text = hour + ":" + min + ":" + sec + "." + ms;
	cv::putText(frame_image_, text, cv::Point2f(16, 40), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.6, cv::Scalar(0, 0, 255), 2);
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
	if (socket_handle_ == INVALID_SOCKET) {
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
		(char*)(buffer_),
		kMaxPacketBufferSize,
		0,
		(sockaddr*)(&remote_addr),
		&addrlen);
	// Copy the data (i	f any) into the data vector.
	std::vector<unsigned char> data;

	if (num_bytes > 0) {
		data.insert(data.end(), &buffer_[0], &buffer_[num_bytes]);
	}
	return data;
}

int main() {

	const int port = 5000;

	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		return 0;
	}

	const ReceiverSocket socket(port);
	if (!socket.BindSocketToListen()) {
		std::cerr << "Could not bind socket." << std::endl;
		//system("pause");
		return -1;
	}
	std::cout << "Listening on port " << port << "." << std::endl;
	
	BasicProtocolData protocol_data;
	while (true) {  // TODO: break out cleanly when done.
		protocol_data.UnpackData(socket.GetPacket());
		protocol_data.GetImage().Display();
	}

	return 0;
}
