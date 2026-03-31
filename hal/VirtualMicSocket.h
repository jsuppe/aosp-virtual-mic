/*
 * VirtualMicSocket - Unix socket server for receiving ashmem fd from renderer
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

namespace aidl::android::hardware::audio::virtualmic {

class VirtualMicSource;

class VirtualMicSocket {
public:
    explicit VirtualMicSocket(VirtualMicSource* source);
    ~VirtualMicSocket();
    
    // Start the socket server
    bool start();
    
    // Stop the socket server
    void stop();
    
private:
    void acceptLoop();
    bool receiveSharedMemoryFd(int clientFd);
    
    VirtualMicSource* mSource;
    int mServerFd = -1;
    std::atomic<bool> mRunning{false};
    std::thread mAcceptThread;
    
    static constexpr const char* SOCKET_PATH = "/data/vendor/virtualmic/virtual_mic.sock";
};

}  // namespace aidl::android::hardware::audio::virtualmic
