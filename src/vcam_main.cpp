#include <opencv2/opencv.hpp>
#include "SharedMemoryWriter.h"
#include <iostream>

// Matches standard CatxFish driver expectation (Name might be OBSVirtualCam_Texture1 depending on fork)
const std::wstring SHARED_MEM_NAME = L"OBSVirtualCam_Texture1";
const int TARGET_WIDTH = 1920;
const int TARGET_HEIGHT = 1080;
const int PIP_WIDTH = 320;
const int PIP_HEIGHT = 180;
const int PIP_MARGIN_X = 20;
const int PIP_MARGIN_Y = 20;

int main(int argc, char** argv) {
    cv::VideoCapture webcam(0, cv::CAP_DSHOW);
    cv::VideoCapture media("media.mp4");

    if (!webcam.isOpened()) {
        std::cerr << "Failed to open webcam.\n";
        return -1;
    }
    if (!media.isOpened()) {
        std::cerr << "Failed to open media file.\n";
        return -1;
    }

    // Set webcam capture resolution
    webcam.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    webcam.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    webcam.set(cv::CAP_PROP_FPS, 30);

    // Calculate NV12 frame size: Y plane + interleaved UV plane (Width * Height * 1.5)
    size_t nv12_size = static_cast<size_t>(TARGET_WIDTH * TARGET_HEIGHT * 3 / 2);
    
    SharedMemoryWriter shm(SHARED_MEM_NAME, nv12_size);
    if (!shm.IsValid()) {
        std::cerr << "Failed to create shared memory.\n";
        return -1;
    }

    cv::Mat mediaFrame, webcamFrame, pipResized;
    cv::Mat finalBgr(TARGET_HEIGHT, TARGET_WIDTH, CV_8UC3);
    cv::Mat nv12Frame;

    int pip_x = TARGET_WIDTH - PIP_WIDTH - PIP_MARGIN_X;
    int pip_y = PIP_MARGIN_Y;
    cv::Rect pipRoi(pip_x, pip_y, PIP_WIDTH, PIP_HEIGHT);

    while (true) {
        media >> mediaFrame;
        if (mediaFrame.empty()) {
            // Loop the media file deterministically
            media.set(cv::CAP_PROP_POS_FRAMES, 0);
            media >> mediaFrame;
            if (mediaFrame.empty()) break;
        }

        webcam >> webcamFrame;
        if (webcamFrame.empty()) {
            break;
        }

        // 1. Process Media Background
        if (mediaFrame.cols != TARGET_WIDTH || mediaFrame.rows != TARGET_HEIGHT) {
            cv::resize(mediaFrame, finalBgr, cv::Size(TARGET_WIDTH, TARGET_HEIGHT));
        } else {
            mediaFrame.copyTo(finalBgr);
        }

        // 2. Process Webcam PiP
        cv::resize(webcamFrame, pipResized, cv::Size(PIP_WIDTH, PIP_HEIGHT));
        pipResized.copyTo(finalBgr(pipRoi));

        // 3. Convert Composited Frame to NV12 format for the DirectShow filter
        cv::cvtColor(finalBgr, nv12Frame, cv::COLOR_BGR2YUV_NV12);

        // 4. Push raw pointer bytes directly to mapped memory
        shm.Write(nv12Frame.data, nv12_size);

        // Throttle loop (approx 30 FPS)
        if (cv::waitKey(33) == 27) { // Exit on 'ESC'
            break;
        }
    }

    webcam.release();
    media.release();
    return 0;
}
