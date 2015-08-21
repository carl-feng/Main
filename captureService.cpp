#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include <sys/time.h>
#include <zmq.h>
#include <pthread.h>
#include <jpeglib.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <unistd.h>
#define SHM_SIZE (1024*1024)
#define SHM_MODE 0600
#define SEM_MODE 0600

char* shared;

#define TFAIL -1
#define TPASS 0

char v4l_capture_dev[100] = "/dev/video0";
int fd_capture_v4l = 0;
int g_cap_mode = 0;
int g_input = 1;
int g_fmt = V4L2_PIX_FMT_YUYV;
int g_rotate = 0;
int g_vflip = 0;
int g_hflip = 0;
int g_vdi_enable = 0;
int g_vdi_motion = 0;
int g_tb = 0;
int g_capture_num_buffers = 2;
int g_in_width = 0;
int g_in_height = 0;
int g_display_width = 720;
int g_display_height = 576;
int g_display_top = 0;
int g_display_left = 0;
int g_frame_size;
int g_frame_period = 33333;
int g_count=1;
v4l2_std_id g_current_std = V4L2_STD_NTSC;

struct testbuffer
{
	unsigned char *start;
	size_t offset;
	unsigned int length;
};

struct testbuffer capture_buffers[3];

union semun{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

const int N_BUFFER = 1;//缓冲区容量
int semSetId=-1;
union semun su;//sem union，用于初始化信号量

//semSetId 表示信号量集合的 id
//semNum 表示要处理的信号量在信号量集合中的索引
void waitSem(int semSetId,int semNum)
{
    struct sembuf sb;
    sb.sem_num = semNum;
    sb.sem_op = -1;//表示要把信号量减一
    sb.sem_flg = SEM_UNDO;//
    //第二个参数是 sembuf [] 类型的，表示数组
    //第三个参数表示 第二个参数代表的数组的大小
    if(semop(semSetId,&sb,1) < 0){
        perror("waitSem failed");
        exit(1);
    }
}
void sigSem(int semSetId,int semNum)
{
    struct sembuf sb;
    sb.sem_num = semNum;
    sb.sem_op = 1;
    sb.sem_flg = SEM_UNDO;
    //第二个参数是 sembuf [] 类型的，表示数组
    //第三个参数表示 第二个参数代表的数组的大小
    if(semop(semSetId,&sb,1) < 0){
        perror("waitSem failed");
        exit(1);
    }
}

void init()
{
    //信号量创建
    //第一个:同步信号量,表示先后顺序,必须有空间才能生产
    //第二个:同步信号量,表示先后顺序,必须有产品才能消费
    //第三个:互斥信号量,生产者和每个消费者不能同时进入缓冲区

    if((semSetId = semget((key_t)54321,3,IPC_CREAT|SEM_MODE)) < 0)
    {
        perror("create semaphore failed");
        exit(1);
    }
    //信号量初始化,其中 su 表示 union semun 
    su.val = N_BUFFER;//当前库房还可以接收多少产品
    if(semctl(semSetId,0,SETVAL, su) < 0){
        perror("semctl failed");
        exit(1);
    }
    su.val = 0;//当前没有产品
    if(semctl(semSetId,1,SETVAL,su) < 0){
        perror("semctl failed");
        exit(1);
    }
    su.val = 1;//为1时可以进入缓冲区
    if(semctl(semSetId,2,SETVAL,su) < 0){
        perror("semctl failed");
        exit(1);
    }
}

int start_capturing(void)
{
    unsigned int i;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;
    for (i = 0; i < g_capture_num_buffers; i++)
    {
        memset(&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd_capture_v4l, VIDIOC_QUERYBUF, &buf) < 0)
        {
            printf("VIDIOC_QUERYBUF error\n");
            return TFAIL;
        }
        capture_buffers[i].length = buf.length;
        capture_buffers[i].offset = (size_t) buf.m.offset;
        capture_buffers[i].start = (unsigned char*)mmap (NULL, capture_buffers[i].length,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd_capture_v4l, capture_buffers[i].offset);
		memset(capture_buffers[i].start, 0xFF, capture_buffers[i].length);
	}

	for (i = 0; i < g_capture_num_buffers; i++)
	{
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.m.offset = capture_buffers[i].offset;
		if (ioctl (fd_capture_v4l, VIDIOC_QBUF, &buf) < 0) {
			printf("VIDIOC_QBUF error\n");
			return TFAIL;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (fd_capture_v4l, VIDIOC_STREAMON, &type) < 0) {
		printf("VIDIOC_STREAMON error\n");
		return TFAIL;
	}
	return 0;
}

int v4l_capture_setup(void)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_dbg_chip_ident chip;
	struct v4l2_streamparm parm;
	v4l2_std_id id;
	unsigned int min;

	if (ioctl (fd_capture_v4l, VIDIOC_QUERYCAP, &cap) < 0) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s is no V4L2 device\n",
					v4l_capture_dev);
			return TFAIL;
		} else {
			fprintf (stderr, "%s isn not V4L device,unknow error\n",
			v4l_capture_dev);
			return TFAIL;
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "%s is no video capture device\n",
			v4l_capture_dev);
		return TFAIL;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf (stderr, "%s does not support streaming i/o\n",
			v4l_capture_dev);
		return TFAIL;
	}

	if (ioctl(fd_capture_v4l, VIDIOC_DBG_G_CHIP_IDENT, &chip))
	{
		printf("VIDIOC_DBG_G_CHIP_IDENT failed.\n");
		close(fd_capture_v4l);
		return TFAIL;
	}
	printf("TV decoder chip is %s\n", chip.match.name);

	if (ioctl(fd_capture_v4l, VIDIOC_S_INPUT, &g_input) < 0)
	{
		printf("VIDIOC_S_INPUT failed\n");
		close(fd_capture_v4l);
		return TFAIL;
	}

	if (ioctl(fd_capture_v4l, VIDIOC_G_STD, &id) < 0)
	{
		printf("VIDIOC_G_STD failed\n");
		close(fd_capture_v4l);
		return TFAIL;
	}
	g_current_std = id;

	if (ioctl(fd_capture_v4l, VIDIOC_S_STD, &id) < 0)
	{
		printf("VIDIOC_S_STD failed\n");
		close(fd_capture_v4l);
		return TFAIL;
	}

	/* Select video input, video standard and tune here. */

	memset(&cropcap, 0, sizeof(cropcap));

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl (fd_capture_v4l, VIDIOC_CROPCAP, &cropcap) < 0) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (ioctl (fd_capture_v4l, VIDIOC_S_CROP, &crop) < 0) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					fprintf (stderr, "%s  doesn't support crop\n",
						v4l_capture_dev);
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = 0;
	parm.parm.capture.capturemode = 0;
	if (ioctl(fd_capture_v4l, VIDIOC_S_PARM, &parm) < 0)
	{
		printf("VIDIOC_S_PARM failed\n");
		close(fd_capture_v4l);
		return TFAIL;
	}

	memset(&fmt, 0, sizeof(fmt));

	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = 0;
	fmt.fmt.pix.height      = 0;
	fmt.fmt.pix.pixelformat = g_fmt;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

	if (ioctl (fd_capture_v4l, VIDIOC_S_FMT, &fmt) < 0){
		fprintf (stderr, "%s iformat not supported \n",
			v4l_capture_dev);
		return TFAIL;
	}

	/* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;

	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	if (ioctl(fd_capture_v4l, VIDIOC_G_FMT, &fmt) < 0)
	{
		printf("VIDIOC_G_FMT failed\n");
		close(fd_capture_v4l);
		return TFAIL;
	}

	g_in_width = fmt.fmt.pix.width;
	g_in_height = fmt.fmt.pix.height;

	printf("VIDIOC_G_FMT success, g_in_width = %d, g_in_height = %d\n", g_in_width, g_in_height);

	memset(&req, 0, sizeof (req));

	req.count               = g_capture_num_buffers;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;

	if (ioctl (fd_capture_v4l, VIDIOC_REQBUFS, &req) < 0) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
					 "memory mapping\n", v4l_capture_dev);
			return TFAIL;
		} else {
			fprintf (stderr, "%s does not support "
					 "memory mapping, unknow error\n", v4l_capture_dev);
			return TFAIL;
		}
	}

	if (req.count < 2) {
		fprintf (stderr, "Insufficient buffer memory on %s\n",
			 v4l_capture_dev);
		return TFAIL;
	}

	return 0;
}

int
compress_yuyv_to_jpeg (unsigned char* yuyv, int width, int height, FILE * file, int quality)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  unsigned char *line_buffer;
  int z;

  line_buffer = (unsigned char*)calloc (width * 3, 1);

  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_compress (&cinfo);
  jpeg_stdio_dest (&cinfo, file);

  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;

  jpeg_set_defaults (&cinfo);
  jpeg_set_quality (&cinfo, quality, TRUE);

  jpeg_start_compress (&cinfo, TRUE);

  z = 0;
  while (cinfo.next_scanline < cinfo.image_height) {
    int x;
    unsigned char *ptr = line_buffer;

    for (x = 0; x < width; x++) {
      int r, g, b;
      int y, u, v;

      if (!z)
        y = yuyv[0] << 8;
      else
        y = yuyv[2] << 8;
      u = yuyv[1] - 128;
      v = yuyv[3] - 128;

      r = (y + (359 * v)) >> 8;
      g = (y - (88 * u) - (183 * v)) >> 8;
      b = (y + (454 * u)) >> 8;

      *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
      *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
      *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

      if (z++) {
        z = 0;
        yuyv += 4;
      }
    }

    row_pointer[0] = line_buffer;
    jpeg_write_scanlines (&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress (&cinfo);
  jpeg_destroy_compress (&cinfo);

  free (line_buffer);

  return (0);
}

int
mxc_v4l_tvin_test(void)
{
	struct v4l2_buffer capture_buf;
	v4l2_std_id id;
	int i, j;
	enum v4l2_buf_type type;

	if (start_capturing() < 0)
	{
		printf("start_capturing failed\n");
		return TFAIL;
	}

	for (i = 0; ; i++) {
begin:
		if (ioctl(fd_capture_v4l, VIDIOC_G_STD, &id)) {
			printf("VIDIOC_G_STD failed.\n");
			return TFAIL;
		}

		if (id == g_current_std)
			goto next;
		else if (id == V4L2_STD_PAL || id == V4L2_STD_NTSC) {

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			ioctl(fd_capture_v4l, VIDIOC_STREAMOFF, &type);

			for (j = 0; j < g_capture_num_buffers; j++)
			{
				munmap(capture_buffers[j].start, capture_buffers[j].length);
			}

			if (v4l_capture_setup() < 0) {
				printf("Setup v4l capture failed.\n");
				return TFAIL;
			}

			if (start_capturing() < 0)
			{
				printf("start_capturing failed\n");
				return TFAIL;
			}
			i = 0;
			printf("TV standard changed\n");
		} else {
			sleep(1);
			/* Try again */
			if (ioctl(fd_capture_v4l, VIDIOC_G_STD, &id)) {
				printf("VIDIOC_G_STD failed.\n");
				return TFAIL;
			}

			if (id != V4L2_STD_ALL)
				goto begin;

			printf("Cannot detect TV standard\n");
			return 0;
		}
next:
		waitSem(semSetId,0);//获取一个空间用于存放产品
        waitSem(semSetId,2);//占有产品缓冲区
		
        memset(&capture_buf, 0, sizeof(capture_buf));
		capture_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		capture_buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(fd_capture_v4l, VIDIOC_DQBUF, &capture_buf) < 0) {
			printf("VIDIOC_DQBUF failed.\n");
			return TFAIL;
		}

        //printf("copy yuyv\n");
		memcpy(shared,capture_buffers[capture_buf.index].start,g_in_width*g_in_height*2);
		//printf("after copy yuyv\n");
        sigSem(semSetId,2);//释放产品缓冲区
        sigSem(semSetId,1);//告知消费者有产品了
		
		if (ioctl(fd_capture_v4l, VIDIOC_QBUF, &capture_buf) < 0) {
			printf("VIDIOC_QBUF failed\n");
			return TFAIL;
		}
	}

	return 0;
}

void* capture_thread(void* arg)
{
	void *shm = NULL;
	int shmid;
	shmid = shmget((key_t)123456, g_display_width*g_display_height*3, 0666|IPC_CREAT);
	if(shmid == -1)
	{
        fprintf(stderr, "shmget failed\n");
        return NULL;
    }
	//将共享内存连接到当前进程的地址空间
	shm = shmat(shmid, (void*)0, 0);
	if(shm == (void*)-1)
	{
        fprintf(stderr, "shmat failed\n");
        return NULL;
	}
	printf("Memory attached at %X\n", (int)shm);
	shared = (char*)shm;
    
    init();
	
	enum v4l2_buf_type type;

	if ((fd_capture_v4l = open(v4l_capture_dev, O_RDWR, 0)) < 0)
	{
		printf("Unable to open %s\n", v4l_capture_dev);
		return NULL;
	}

	if (v4l_capture_setup() < 0) {
		printf("Setup v4l capture failed.\n");
		return NULL;
	}

	mxc_v4l_tvin_test();

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd_capture_v4l, VIDIOC_STREAMOFF, &type);
	
	int i;
	for (i = 0; i < g_capture_num_buffers; i++)
	{
		munmap(capture_buffers[i].start, capture_buffers[i].length);
	}

	close(fd_capture_v4l);
    if(shmdt(shm) == -1)
        fprintf(stderr, "shmdt failed\n");
    if(shmctl(shmid, IPC_RMID, 0) == -1)
    {
	    fprintf(stderr, "shmctl(IPC_RMID) failed\n");
    }
	return NULL;
}


int get_lock(void)
{
    int fdlock;
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if((fdlock = open("/tmp/capture_service.lock", O_WRONLY|O_CREAT, 0666)) == -1)
        return 0;
    if(fcntl(fdlock, F_SETLK, &fl) == -1)
        return 0;
    return 1;
}

int main(int argc, char * argv[])
{
    pthread_t id;
    void * pCtx = NULL;
    void * pSock = NULL;
    const char * pAddr = "tcp://*:7766";
    
    if(!get_lock())
    {
        printf("another instance (capture service) is running, exit.\n");
        exit(0);
    }

    int ret=pthread_create(&id, NULL, capture_thread, NULL);  
    if(ret!=0)  
    {
        printf("Create pthread error!\n");  
        return -1;  
    }
    while(1) sleep(10);
    return 0;
}

#ifdef __cplusplus
}
#endif
