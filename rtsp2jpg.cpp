#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <jpeglib.h>

#ifdef __cplusplus
}
#endif

int mkjpeg(int width, int height, char *buffer, char *filename) { 
	struct jpeg_compress_struct jcs; 
	struct jpeg_error_mgr jem; 
	JSAMPROW row_pointer[1]; 
	jcs.err = jpeg_std_error(&jem); 
	jpeg_create_compress(&jcs); 
	FILE *fp = fopen(filename,"wb"); 
	if (fp==NULL) { return 0; }
	jpeg_stdio_dest(&jcs, fp); 
	jcs.image_width = width; 
	jcs.image_height = height;
	jcs.input_components = 3; 
	jcs.in_color_space = JCS_RGB; 
	jpeg_set_defaults(&jcs); 
	jpeg_set_quality (&jcs, 80, TRUE); 
	jpeg_start_compress(&jcs, TRUE); 
	while (jcs.next_scanline < jcs.image_height) {
		row_pointer[0] = (unsigned char*)&buffer[jcs.next_scanline * width * 3]; 
		jpeg_write_scanlines(&jcs, row_pointer, 1); 
	} 
	jpeg_finish_compress(&jcs); 
	jpeg_destroy_compress(&jcs); 
	fclose(fp); 
	return 1; 
}

int main(int argc, char* argv[])
{
  AVFormatContext	*pFormatCtx;
  int			i, videoindex;
  AVCodecContext	*pCodecCtx;
  AVCodec		*pCodec;
  char filepath[]="nwn.mp4";
  const char* rtspUrl = "rtsp://192.168.3.250:554/1/h264major";
  if(argc != 3)
  {
    printf("Usage: %s rtsp_url filename\n", argv[0]);
    return -1;
  }
  rtspUrl = argv[1];

  av_register_all();
  avformat_network_init();
  pFormatCtx = avformat_alloc_context();
  AVDictionary *avdic=NULL;
  char option_key[]="rtsp_transport";
  char option_value[]="tcp";
  av_dict_set(&avdic,option_key,option_value,0);
  if(avformat_open_input(&pFormatCtx,/*filepath*/rtspUrl,NULL,&avdic)!=0){
    printf("avformat_open_input failed.\n");
    return -1;
  }
  printf("avformat_open_input success.\n");
  AVDictionary* pOptions = NULL;
  pFormatCtx->probesize = 100 *1024;
  pFormatCtx->max_analyze_duration = 5 * AV_TIME_BASE;
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0) 
  {
    printf("Couldn't find stream information.\n");
    return -1;
  }
  printf("find stream information.\n");
  videoindex=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
    {
      videoindex=i;
      break;
    }
  if(videoindex==-1)
  {
    printf("Didn't find a video stream.\n");
    return -1;
  }
  printf("find a video stream.\n");
  pCodecCtx=pFormatCtx->streams[videoindex]->codec;
  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);//查找解码器
  if(pCodec==NULL)
  {
    printf("Codec not found.\n");
    return -1;
  }
  if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)//打开解码器
  {
    printf("Could not open codec.\n");
    return -1;
  }
  AVFrame	*pFrame,*pFrameRGB;
  pFrame=avcodec_alloc_frame();//存储解码后AVFrame
  pFrameRGB=avcodec_alloc_frame();//存储转换后AVFrame（为什么要转换？后文解释）
  uint8_t *out_buffer;
  out_buffer=new uint8_t[avpicture_get_size( PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height)];//分配AVFrame所需内存
  avpicture_fill((AVPicture *)pFrameRGB, out_buffer,  PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);//填充AVFrame

  int ret, got_picture;
  static struct SwsContext *img_convert_ctx;
  int y_size = pCodecCtx->width * pCodecCtx->height;

  AVPacket *packet=(AVPacket *)malloc(sizeof(AVPacket));//存储解码前数据包AVPacket
  av_new_packet(packet, y_size);

  while(av_read_frame(pFormatCtx, packet)>=0)//循环获取压缩数据包AVPacket
  {
    if(packet->stream_index==videoindex)
    {
      ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);//解码。输入为AVPacket，输出为AVFrame
      if(ret < 0)
      {
        printf("解码错误\n");
        return -1;
      }
      if(got_picture)
      {
        //像素格式转换。pFrame转换为pFrameRGB。
        img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, 
		pCodecCtx->width, pCodecCtx->height,  PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL); 
        printf("pCodecCtx->pix_fmt = %d\n", pCodecCtx->pix_fmt);
        sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
		pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
        sws_freeContext(img_convert_ctx);
	//static int i = 0;
	//char buffer[10];
	//snprintf(buffer, 10, "%s%d.jpg", i++);
	mkjpeg(pCodecCtx->width, pCodecCtx->height, (char*) pFrameRGB->data[0], argv[2]);
	printf("finished mkjpeg\n");
        break;
      }
    }
    av_free_packet(packet);
  }
  delete[] out_buffer;
  av_free(pFrameRGB);
  avcodec_close(pCodecCtx);
  avformat_close_input(&pFormatCtx);

  return 0;
}
