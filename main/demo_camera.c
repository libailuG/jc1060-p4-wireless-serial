#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "demo.h"
#include "bsp/esp-bsp.h"
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "esp_check.h"

static bool camera_ready;
esp_err_t demo_camera_capture(char *result, size_t capacity)
{
    /* 此函数展示 init -> open -> format -> MMAP -> queue -> stream -> frame -> cleanup。
     * 改成连续视频时，在 DQBUF 与 QBUF 之间处理帧；回填后该缓冲区立即归驱动所有。 */
    if (!camera_ready) {
        ESP_RETURN_ON_ERROR(bsp_i2c_init(), "camera", "I2C");
        esp_video_init_csi_config_t csi = {
            .sccb_config = {.init_sccb = false, .i2c_handle = bsp_i2c_get_handle(), .freq = 100000},
            .reset_pin = -1, .pwdn_pin = -1,
        };
        esp_video_init_config_t config = {.csi = &csi};
        ESP_RETURN_ON_ERROR(esp_video_init(&config), "camera", "CSI init");
        camera_ready = true;
    }
    int fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    if (fd < 0) return ESP_FAIL;
    esp_err_t ret = ESP_FAIL;
    bool streaming = false;
    void *buffers[2] = {0};
    size_t lengths[2] = {0};
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format format = {.type = type};
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) goto cleanup;
    /* RGB565 适合送入 LVGL；摄像头分辨率由 OV02C10 Kconfig 决定。 */
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) goto cleanup;
    struct timeval timeout = {.tv_sec = 3};
    if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) goto cleanup;
    struct v4l2_requestbuffers request = {.count = 2, .type = type, .memory = V4L2_MEMORY_MMAP};
    if (ioctl(fd, VIDIOC_REQBUFS, &request) != 0 || request.count != 2) goto cleanup;
    for (unsigned i = 0; i < 2; i++) {
        struct v4l2_buffer buffer = {.type = type, .memory = V4L2_MEMORY_MMAP, .index = i};
        if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) != 0) goto cleanup;
        lengths[i] = buffer.length;
        buffers[i] = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
        if (!buffers[i] || buffers[i] == MAP_FAILED) { buffers[i] = NULL; goto cleanup; }
        if (ioctl(fd, VIDIOC_QBUF, &buffer) != 0) goto cleanup;
    }
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) goto cleanup;
    streaming = true;
    uint32_t checksum = 0, bytes = 0;
    for (int frame = 0; frame < 5; frame++) {
        struct v4l2_buffer buffer = {.type = type, .memory = V4L2_MEMORY_MMAP};
        if (ioctl(fd, VIDIOC_DQBUF, &buffer) != 0) { ret = ESP_ERR_TIMEOUT; goto cleanup; }
        if (buffer.index >= 2 || buffer.bytesused > lengths[buffer.index]) goto cleanup;
        const uint8_t *pixels = buffers[buffer.index];
        bytes = buffer.bytesused;
        /* 在这里替换为图像算法/复制到独立的 LVGL 图像缓冲区。 */
        for (size_t j = 0; j < bytes; j += 128) checksum = checksum * 33u + pixels[j];
        if (ioctl(fd, VIDIOC_QBUF, &buffer) != 0) goto cleanup;
    }
    snprintf(result, capacity, "Captured 5 RGB565 frames: %lux%lu, %lu bytes/frame, sample checksum %08lx. Frame buffers released.",
        (unsigned long)format.fmt.pix.width, (unsigned long)format.fmt.pix.height,
        (unsigned long)bytes, (unsigned long)checksum);
    ret = ESP_OK;
cleanup:
    if (streaming) ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < 2; i++) if (buffers[i]) munmap(buffers[i], lengths[i]);
    close(fd);
    return ret;
}
