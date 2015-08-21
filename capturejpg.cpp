//包含zmq的头文件 
#include <zmq.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <unistd.h>
#include <jpeglib.h>
#define SHM_SIZE (1024*1024)
#define SHM_MODE 0600
#define SEM_MODE 0600

#define SKIP_NUM 5

int get_lock(void)
{
    int fdlock;
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if((fdlock = open("/tmp/capturejpg.lock", O_WRONLY|O_CREAT, 0666)) == -1)
        return 0;
    if(fcntl(fdlock, F_SETLK, &fl) == -1)
        return 0;
    return 1;
}

union semun{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

//const int N_CONSUMER = 3;//消费者数量
//const int N_BUFFER = 5;//缓冲区容量
int semSetId=-1;
union semun su;//sem union，用于初始化信号量
void *shm;//分配的共享内存的原始首地址
void *shared;//指向shm
int shmid;//共享内存标识符

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

    if(semSetId = -1 && (semSetId = semget((key_t)54321,3,SEM_MODE)) < 0)
    {
        perror("create semaphore failed");
        exit(1);
    }
	//创建共享内存
	shmid = shmget((key_t)123456, 720*576*3, 0666|IPC_CREAT);
	if(shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}
	//将共享内存连接到当前进程的地址空间
	shm = shmat(shmid, 0, 0);
	if(shm == (void*)-1)
	{
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}
	printf("\nMemory attached at %X\n", (int)shm);
   
	//设置共享内存
	shared = (void*)shm;
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

int main(int argc, char * argv[])
{
    if(argc < 2)
    {
        printf("Usage:%s filename\n", argv[0]);
        return -1;
    }
    char* filename = argv[1];

    while(!get_lock())
    {
        printf("another instance(capturejpg) is running, retry later...\n");
        sleep(1);
    }
    init();

    for(int i = 0; i < SKIP_NUM; i++)
    {
        waitSem(semSetId,1);//必须有产品才能消费
        waitSem(semSetId,2);//锁定缓冲区
        if(i == SKIP_NUM - 1)
        {
            FILE* file = fopen(filename, "wb");
	        compress_yuyv_to_jpeg((unsigned char*)shared, 720, 576, file, 90);
	        fclose(file);
	        printf("capture jpeg file %s\n", filename);
        }
        usleep(500);
        sigSem(semSetId,2);//释放缓冲区
        sigSem(semSetId,0);//告知生产者,有空间了
    }
    return 0;
}
