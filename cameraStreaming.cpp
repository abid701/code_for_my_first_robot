#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include "civetweb.h"

// Global camera object
cv::VideoCapture camera(0); // Adjust index if needed

// Frame buffer
cv::Mat frame;

// Mutex to safely access the frame
std::mutex frame_mutex;

// Function to capture frames continuously
void captureFrames() {
    while (true) {
        cv::Mat temp_frame;
        if (camera.read(temp_frame)) {
            std::lock_guard<std::mutex> lock(frame_mutex);
            frame = temp_frame.clone(); // Clone to ensure thread safety
        } else {
            std::cerr << "Failed to capture frame" << std::endl;
        }
    }
}

// Function to handle HTTP requests
int videoStreamHandler(struct mg_connection *conn, void *user_data) {
    // HTTP header for streaming video
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
              "\r\n");

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Limit frame rate

        cv::Mat temp_frame;
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            if (frame.empty()) continue;
            temp_frame = frame.clone(); // Clone to avoid accessing the frame directly
        }

        // Encode frame as JPEG
        std::vector<uchar> buffer;
        cv::imencode(".jpg", temp_frame, buffer);

        // Write frame as part of the HTTP response
        mg_printf(conn,
                  "--frame\r\n"
                  "Content-Type: image/jpeg\r\n"
                  "Content-Length: %zu\r\n\r\n",
                  buffer.size());
        mg_write(conn, buffer.data(), buffer.size());
        mg_printf(conn, "\r\n");
    }

    return 1;
}

int main() {
    // Open the camera
    if (!camera.isOpened()) {
        std::cerr << "Error: Cannot access the camera." << std::endl;
        return -1;
    }

    // Start capturing frames in a separate thread
    std::thread captureThread(captureFrames);

    // Set up Civetweb server
    const char *options[] = {"listening_ports", "5000", nullptr};
    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    struct mg_context *ctx = mg_start(&callbacks, nullptr, options);

    // Add the video stream endpoint
    mg_set_request_handler(ctx, "/video_feed", videoStreamHandler, nullptr);

    std::cout << "Server running at http://<your-ip>:5000/video_feed" << std::endl;

    // Keep the server running
    std::cin.get();

    // Stop the server and cleanup
    mg_stop(ctx);
    captureThread.join();

    return 0;
}
