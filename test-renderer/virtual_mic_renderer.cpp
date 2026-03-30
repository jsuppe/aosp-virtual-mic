/*
 * Virtual Mic Test Renderer
 * 
 * Generates a sine wave and sends it to the Virtual Mic HAL via ashmem.
 * Build with Android NDK and push to device.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <linux/ashmem.h>
#include <fcntl.h>
#include <errno.h>

// Audio parameters
static constexpr int SAMPLE_RATE = 48000;
static constexpr int CHANNELS = 2;
static constexpr int BITS_PER_SAMPLE = 16;
static constexpr int FRAME_SIZE = CHANNELS * (BITS_PER_SAMPLE / 8);  // 4 bytes

// Buffer parameters
static constexpr size_t BUFFER_FRAMES = 4096;
static constexpr size_t BUFFER_SIZE = BUFFER_FRAMES * FRAME_SIZE;

// Socket path
static constexpr const char* SOCKET_PATH = "/data/vendor/virtualmic/virtual_mic.sock";

// Audio buffer header (must match HAL's AudioBufferHeader)
struct AudioBufferHeader {
    uint32_t magic;           // 0x56434D46 ("VMIC")
    uint32_t version;
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t format;          // 1 = PCM_16
    uint32_t bufferSize;
    uint32_t writePos;
    uint32_t readPos;
    uint32_t flags;
};

static constexpr uint32_t VMIC_MAGIC = 0x56434D46;

// Create ashmem region
int create_ashmem(const char* name, size_t size) {
    int fd = open("/dev/ashmem", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/ashmem");
        return -1;
    }
    
    if (ioctl(fd, ASHMEM_SET_NAME, name) < 0) {
        perror("Failed to set ashmem name");
        close(fd);
        return -1;
    }
    
    if (ioctl(fd, ASHMEM_SET_SIZE, size) < 0) {
        perror("Failed to set ashmem size");
        close(fd);
        return -1;
    }
    
    return fd;
}

// Send file descriptor over Unix socket
bool send_fd(int socket, int fd) {
    char buf[1] = {0};
    struct iovec iov = { .iov_base = buf, .iov_len = 1 };
    
    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    memset(cmsgbuf, 0, sizeof(cmsgbuf));
    
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *((int*)CMSG_DATA(cmsg)) = fd;
    
    if (sendmsg(socket, &msg, 0) < 0) {
        perror("Failed to send fd");
        return false;
    }
    
    return true;
}

// Generate sine wave sample
int16_t generate_sine(double phase, double amplitude) {
    return static_cast<int16_t>(amplitude * sin(phase) * 32767.0);
}

int main(int argc, char* argv[]) {
    double frequency = 440.0;  // A4 note
    double amplitude = 0.5;
    int duration_sec = 10;
    
    // Parse args
    if (argc > 1) frequency = atof(argv[1]);
    if (argc > 2) amplitude = atof(argv[2]);
    if (argc > 3) duration_sec = atoi(argv[3]);
    
    printf("Virtual Mic Test Renderer\n");
    printf("Frequency: %.1f Hz, Amplitude: %.2f, Duration: %d sec\n",
           frequency, amplitude, duration_sec);
    
    // Connect to HAL socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        return 1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    printf("Connecting to %s...\n", SOCKET_PATH);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Failed to connect");
        close(sock);
        return 1;
    }
    printf("Connected!\n");
    
    // Create ashmem buffer
    size_t total_size = sizeof(AudioBufferHeader) + BUFFER_SIZE;
    int ashmem_fd = create_ashmem("virtual_mic_audio", total_size);
    if (ashmem_fd < 0) {
        close(sock);
        return 1;
    }
    
    // Map the buffer
    void* buffer = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, 
                        MAP_SHARED, ashmem_fd, 0);
    if (buffer == MAP_FAILED) {
        perror("Failed to mmap");
        close(ashmem_fd);
        close(sock);
        return 1;
    }
    
    // Initialize header
    AudioBufferHeader* header = static_cast<AudioBufferHeader*>(buffer);
    header->magic = VMIC_MAGIC;
    header->version = 1;
    header->sampleRate = SAMPLE_RATE;
    header->channels = CHANNELS;
    header->format = 1;  // PCM_16
    header->bufferSize = BUFFER_SIZE;
    header->writePos = 0;
    header->readPos = 0;
    header->flags = 0;
    
    int16_t* audio_data = reinterpret_cast<int16_t*>(
        static_cast<uint8_t*>(buffer) + sizeof(AudioBufferHeader));
    
    // Send fd to HAL
    printf("Sending ashmem fd to HAL...\n");
    if (!send_fd(sock, ashmem_fd)) {
        munmap(buffer, total_size);
        close(ashmem_fd);
        close(sock);
        return 1;
    }
    printf("Ashmem fd sent!\n");
    
    // Generate and write audio
    printf("Generating %d seconds of %.1f Hz sine wave...\n", duration_sec, frequency);
    
    double phase = 0.0;
    double phase_increment = 2.0 * M_PI * frequency / SAMPLE_RATE;
    int total_frames = SAMPLE_RATE * duration_sec;
    int frames_written = 0;
    
    while (frames_written < total_frames) {
        // Calculate how many frames we can write
        uint32_t write_pos = header->writePos;
        uint32_t read_pos = header->readPos;
        
        size_t available;
        if (write_pos >= read_pos) {
            available = BUFFER_FRAMES - (write_pos - read_pos) - 1;
        } else {
            available = read_pos - write_pos - 1;
        }
        
        if (available == 0) {
            // Buffer full, wait a bit
            usleep(1000);
            continue;
        }
        
        // Write one frame at a time
        size_t frame_offset = write_pos * CHANNELS;
        int16_t sample = generate_sine(phase, amplitude);
        audio_data[frame_offset] = sample;      // Left
        audio_data[frame_offset + 1] = sample;  // Right
        
        phase += phase_increment;
        if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
        
        // Update write position (atomic)
        header->writePos = (write_pos + 1) % BUFFER_FRAMES;
        frames_written++;
        
        // Progress
        if (frames_written % SAMPLE_RATE == 0) {
            printf("  %d/%d seconds\n", frames_written / SAMPLE_RATE, duration_sec);
        }
    }
    
    printf("Done! Wrote %d frames\n", frames_written);
    
    // Cleanup
    munmap(buffer, total_size);
    close(ashmem_fd);
    close(sock);
    
    return 0;
}
