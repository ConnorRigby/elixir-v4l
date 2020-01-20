#include <sys/mman.h>
#include <sys/fcntl.h> 
#include <sys/ioctl.h>      

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>

#include <linux/videodev2.h>
#include <ei.h>

int main(void) {
  struct pollfd fds[2];
  int fd, rc;
  int streaming = 1;

	/* watch stdin for input */
	fds[0].fd = 3;
	fds[0].events = POLLIN;
  fds[0].revents = 0;

  fds[1].fd = 4;
  fds[1].events = POLLOUT;
  fds[1].revents = 0;

  uint32_t headerBuffer;
  uint32_t header;
  char* inputBuffer = NULL;

  if((fd = open("/dev/video0", O_RDWR)) < 0){
    perror("open");
    exit(1);
  }

  struct v4l2_capability cap;
  if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0){
    perror("VIDIOC_QUERYCAP");
    exit(1);
  }

  if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
    fprintf(stderr, "The device does not handle single-planar video capture.\n");
    exit(1);
  }

  struct v4l2_format format;
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  format.fmt.pix.width = 800;
  format.fmt.pix.height = 600;
  
  if(ioctl(fd, VIDIOC_S_FMT, &format) < 0){
    perror("VIDIOC_S_FMT");
    exit(1);
  }

  struct v4l2_requestbuffers bufrequest;
  bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  bufrequest.memory = V4L2_MEMORY_MMAP;
  bufrequest.count = 1;
  
  if(ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0){
    perror("VIDIOC_REQBUFS");
    exit(1);
  }

  struct v4l2_buffer bufferinfo;
  
  memset(&bufferinfo, 0, sizeof(bufferinfo));
  bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  bufferinfo.memory = V4L2_MEMORY_MMAP;
  bufferinfo.index = 0; /* Queueing buffer index 0. */
  
  // Put the buffer in the incoming queue.
  if(ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0){
    perror("VIDIOC_QBUF");
    exit(1);
  }
 
  // Activate streaming
  int type = bufferinfo.type;
  if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
    perror("VIDIOC_STREAMON");
    exit(1);
  }

  void* buffer_start = mmap(
    NULL,
    bufferinfo.length,
    PROT_READ | PROT_WRITE,
    MAP_SHARED,
    fd,
    bufferinfo.m.offset
  );
 
  if(buffer_start == MAP_FAILED){
    perror("mmap");
    exit(1);
  }
  
  memset(buffer_start, 0, bufferinfo.length);
  
  while(streaming == 1){
    rc = poll(fds, 2, -1);
    if (rc < 0) {
      // Retry if EINTR
      if (errno == EINTR)
          continue;

      perror("poll");
      exit(1);
    }

    // process input from erlang
    if (fds[0].revents & (POLLIN | POLLHUP)) {
      int numBytesRead = read(3, &headerBuffer, 4);
      if(numBytesRead == 0) {
        fprintf(stderr, "Port closed. Exiting\r\n");
        streaming = 0;
      }

      header = htonl(headerBuffer);
      inputBuffer = malloc(header);
      if(inputBuffer) {
        read(3, inputBuffer, header);
        fprintf(stderr, "got data: [%s]\r\n", inputBuffer);
      } else {
        perror("failed to create buffer\r\n");
        streaming = 0;
      }
    }

    // Dequeue the buffer.
    if(ioctl(fd, VIDIOC_DQBUF, &bufferinfo) < 0){
      perror("VIDIOC_QBUF");
      streaming = 0;
    }

    ei_x_buff outputEiBuffer;
    if (ei_x_new_with_version(&outputEiBuffer) < 0) {
      perror("ei_x_new_with_version");
      exit(1);
    }

    rc = ei_x_encode_binary(&outputEiBuffer, buffer_start, bufferinfo.length);
    if(rc != 0) {
      fprintf(stderr, "could not encode frame\r\n");
      exit(1);
    }
    uint32_t packetheader = ntohl(outputEiBuffer.index);
    write(4, &packetheader, 4);
    rc = write(4, outputEiBuffer.buff, outputEiBuffer.index);
    if(rc != outputEiBuffer.index) {
      fprintf(stderr, "wrote %d bytes\r\n", outputEiBuffer.index);
    }
    ei_x_free(&outputEiBuffer);

    bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufferinfo.memory = V4L2_MEMORY_MMAP;
    /* Set the index if using several buffers */

    // Queue the next one.
    if(ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0){
      perror("VIDIOC_QBUF");
      streaming = 0;
    }
  }
  
  // Deactivate streaming
  if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
    perror("VIDIOC_STREAMOFF");
    exit(1);
  }

  close(fd);
  return EXIT_SUCCESS;
}