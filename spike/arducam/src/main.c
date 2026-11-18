// ArduCAM Mega spike (#37): capture one JPEG and print it base64 between
// markers; tools/arducam_rx.py on the host turns the console log into a file.
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/sys/base64.h>
#include <zephyr/drivers/spi.h>
#include <stdio.h>

#define WIDTH 640
#define HEIGHT 480
#define CHUNK CONFIG_VIDEO_BUFFER_POOL_SZ_MAX

static void print_base64(const uint8_t* data, size_t len) {
  static char line[80];  // 57 bytes -> 76 characters
  for (size_t i = 0; i < len; i += 57) {
    size_t n = len - i < 57 ? len - i : 57, olen;
    base64_encode(line, sizeof(line), &olen, data + i, n);
    printf("%.*s\n", (int)olen, line);
  }
}

// One frame: enqueue the one buffer, start, take chunks until one is short.
static int capture(const struct device* cam, struct video_buffer* buf, int frame) {
  size_t total = 0;
  int64_t start = k_uptime_get();
  video_enqueue(cam, buf);
  if (video_stream_start(cam, VIDEO_BUF_TYPE_OUTPUT) != 0) return -1;
  printf("JPEG-BEGIN %d\n", frame);
  while (true) {
    struct video_buffer* got;
    if (video_dequeue(cam, &got, K_SECONDS(5)) != 0) {
      printf("JPEG-ABORT %d: no buffer in 5 s after %u B\n", frame, (unsigned)total);
      video_stream_stop(cam, VIDEO_BUF_TYPE_OUTPUT);
      return -1;
    }
    if (total == 0) printf("# first bytes %02x %02x\n", got->buffer[0], got->buffer[1]);
    print_base64(got->buffer, got->bytesused);
    total += got->bytesused;
    bool last = got->bytesused < got->size;
    if (last) {
      video_stream_stop(cam, VIDEO_BUF_TYPE_OUTPUT);
      printf("JPEG-END %d %u B, last bytes %02x %02x, %lld ms\n", frame, (unsigned)total,
             got->buffer[got->bytesused - 2], got->buffer[got->bytesused - 1], k_uptime_get() - start);
      return 0;
    }
    video_enqueue(cam, got);
  }
}


// Raw register read, the driver's framing (address, dummy, value), to see what
// the camera answers when the driver's init says "no connection".
static void probe(void) {
  static const struct spi_dt_spec bus = SPI_DT_SPEC_GET(
      DT_CHOSEN(zephyr_camera), SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE, 0);
  if (!spi_is_ready_dt(&bus)) {
    printf("probe: spi not ready\n");
    return;
  }
  static const uint8_t regs[] = {0x44, 0x40, 0x41, 0x42, 0x43};
  for (int round = 0; round < 3; round++) {
    printf("probe %d:", round);
    for (size_t i = 0; i < sizeof(regs); i++) {
      uint8_t tx[3] = {regs[i] & 0x7f, 0, 0}, rx[3] = {0};
      struct spi_buf tb = {.buf = tx, .len = 3}, rb = {.buf = rx, .len = 3};
      struct spi_buf_set ts = {.buffers = &tb, .count = 1}, rs = {.buffers = &rb, .count = 1};
      int err = spi_transceive_dt(&bus, &ts, &rs);
      printf(" %02x=%02x %02x %02x(%d)", regs[i], rx[0], rx[1], rx[2], err);
    }
    printf("\n");
    k_msleep(500);
  }
}

int main(void) {
  k_msleep(3000);  // time for the host logger to reopen the port after a flash
  const struct device* cam = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));
  probe();
  if (!device_is_ready(cam)) {
    printf("arducam: not ready (init failed: wiring, power or SPI)\n");
    return 0;
  }
  struct video_format fmt = {.type = VIDEO_BUF_TYPE_OUTPUT, .pixelformat = VIDEO_PIX_FMT_JPEG,
                             .width = WIDTH, .height = HEIGHT};
  int err = video_set_format(cam, &fmt);
  printf("arducam: ready, set JPEG %dx%d: %d\n", WIDTH, HEIGHT, err);
  struct video_buffer* buf = video_buffer_alloc(CHUNK, K_NO_WAIT);
  if (buf == NULL) {
    printf("arducam: no %d B buffer\n", CHUNK);
    return 0;
  }
  for (int frame = 0; frame < 2; frame++) {
    if (capture(cam, buf, frame) != 0) break;
    k_msleep(2000);
  }
  printf("arducam: done\n");
  return 0;
}
