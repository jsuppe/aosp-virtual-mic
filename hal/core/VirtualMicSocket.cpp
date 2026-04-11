/*
 * VirtualMicSocket - Unix socket server for receiving ashmem fd from renderer
 */

#define LOG_TAG "VirtualMicSocket"

#include "VirtualMicSocket.h"
#include "VirtualMicSource.h"

#include <log/log.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace virtualmic {

VirtualMicSocket::VirtualMicSocket(VirtualMicSource* source)
    : mSource(source) {
}

VirtualMicSocket::~VirtualMicSocket() {
    stop();
}

bool VirtualMicSocket::start() {
    // Ensure the socket directory exists. It should be created at boot by
    // init.rc (`mkdir /data/vendor/virtualmic ...`), but create it defensively
    // in case the service is launched before post-fs-data.
    const char* socketDir = "/data/vendor/virtualmic";
    if (mkdir(socketDir, 0775) < 0 && errno != EEXIST) {
        ALOGE("Failed to create socket directory %s: %s", socketDir, strerror(errno));
        // Continue anyway — bind() below will fail with a clearer error
    }

    // Create Unix domain socket
    mServerFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (mServerFd < 0) {
        ALOGE("Failed to create socket: %s", strerror(errno));
        return false;
    }

    // Remove any existing socket file
    unlink(SOCKET_PATH);

    // Bind to socket path
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(mServerFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ALOGE("Failed to bind socket: %s", strerror(errno));
        close(mServerFd);
        mServerFd = -1;
        return false;
    }

    // Make socket accessible
    chmod(SOCKET_PATH, 0666);

    // Listen for connections
    if (listen(mServerFd, 1) < 0) {
        ALOGE("Failed to listen on socket: %s", strerror(errno));
        close(mServerFd);
        mServerFd = -1;
        return false;
    }

    // Start accept thread
    mRunning.store(true, std::memory_order_release);
    mAcceptThread = std::thread(&VirtualMicSocket::acceptLoop, this);

    ALOGI("Socket server started at %s", SOCKET_PATH);
    return true;
}

void VirtualMicSocket::stop() {
    mRunning.store(false, std::memory_order_release);

    if (mServerFd >= 0) {
        shutdown(mServerFd, SHUT_RDWR);
        close(mServerFd);
        mServerFd = -1;
    }

    if (mAcceptThread.joinable()) {
        mAcceptThread.join();
    }

    unlink(SOCKET_PATH);
}

void VirtualMicSocket::acceptLoop() {
    ALOGI("Accept loop started");

    while (mRunning.load(std::memory_order_acquire)) {
        struct sockaddr_un clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientFd = accept(mServerFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) {
            if (mRunning.load(std::memory_order_acquire)) {
                ALOGE("Accept failed: %s", strerror(errno));
            }
            continue;
        }

        ALOGI("Renderer connected");

        if (!receiveSharedMemoryFd(clientFd)) {
            ALOGE("Failed to receive shared memory fd");
        }

        close(clientFd);
    }

    ALOGI("Accept loop ended");
}

bool VirtualMicSocket::receiveSharedMemoryFd(int clientFd) {
    // Receive the ashmem fd via SCM_RIGHTS
    char buf[1];
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    union {
        struct cmsghdr cmh;
        char control[CMSG_SPACE(sizeof(int))];
    } controlUnion;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = controlUnion.control;
    msg.msg_controllen = sizeof(controlUnion.control);

    ssize_t bytesReceived = recvmsg(clientFd, &msg, 0);
    if (bytesReceived <= 0) {
        ALOGE("Failed to receive message: %s", strerror(errno));
        return false;
    }

    // Extract the fd
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr || cmsg->cmsg_level != SOL_SOCKET ||
        cmsg->cmsg_type != SCM_RIGHTS) {
        ALOGE("No SCM_RIGHTS in message");
        return false;
    }

    int fd = *reinterpret_cast<int*>(CMSG_DATA(cmsg));

    // Now receive the size
    uint64_t size;
    if (recv(clientFd, &size, sizeof(size), 0) != sizeof(size)) {
        ALOGE("Failed to receive size");
        close(fd);
        return false;
    }

    ALOGI("Received shared memory fd: %d, size: %llu", fd, (unsigned long long)size);

    // Notify the source
    mSource->onRendererConnected(fd, size);

    return true;
}

}  // namespace virtualmic
