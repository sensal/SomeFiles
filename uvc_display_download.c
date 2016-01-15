/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>
#include <signal.h>

#include <SDL2/SDL.h>
#include <pthread.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#include <libusb-1.0/libusb.h>

#define FLASHPROG_VID		(0x04b4)	// USB VID for the FX3 flash programmer.

#define MAX_FWIMG_SIZE		(512 * 1024)	// Maximum size of the firmware binary.
#define MAX_WRITE_SIZE		(2 * 1024)	// Max. size of data that can be written through one vendor command.

#define I2C_PAGE_SIZE		(64)		// Page size for I2C EEPROM.
#define I2C_SLAVE_SIZE		(64 * 1024)	// Max. size of data that can fit on one EEPROM address.

#define SPI_PAGE_SIZE		(256)		// Page size for SPI flash memory.
#define SPI_SECTOR_SIZE		(64 * 1024)	// Sector size for SPI flash memory.

#define VENDORCMD_TIMEOUT	(5000)		// Timeout (in milliseconds) for each vendor command.
#define GETHANDLE_TIMEOUT	(5)		// Timeout (in seconds) for getting a FX3 flash programmer handle.

/* Utility macros. */
#define ROUND_UP(n,v)	((((n) + ((v) - 1)) / (v)) * (v))	// Round n upto a multiple of v.
#define GET_LSW(v)	((unsigned short)((v) & 0xFFFF))	// Get Least Significant Word part of an integer.
#define GET_MSW(v)	((unsigned short)((v) >> 16))		// Get Most Significant Word part of an integer.

/* Enumeration representing the FX3 firmware target. */
typedef enum {
	FW_TARGET_NONE = 0,	// Invalid value
	FW_TARGET_RAM,		// Program to device RAM
	FW_TARGET_I2C,		// Program to I2C EEPROM
	FW_TARGET_SPI		// Program to SPI Flash
} fx3_fw_target;

/* Array representing physical size of EEPROM corresponding to each size encoding. */
const int i2c_eeprom_size[] =
{
	1024,		// bImageCtl[2:0] = 'b000
	2048,		// bImageCtl[2:0] = 'b001
	4096,		// bImageCtl[2:0] = 'b010
	8192,		// bImageCtl[2:0] = 'b011
	16384,		// bImageCtl[2:0] = 'b100
	32768,		// bImageCtl[2:0] = 'b101
	65536,		// bImageCtl[2:0] = 'b110
	131072		// bImageCtl[2:0] = 'b111
};

//#define OUT_FILE "/home/coco/work/fx3_mt9v034/data/out.yuv"
//#define OUT_FILE "/data/out.mjpg"

typedef enum {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
} io_method;

struct buffer {
	void *start;
	size_t length;
};

typedef struct {
	char *dev_name;
	int fd;
	io_method io;
	unsigned int width;
	unsigned int height;
}uvc_cam_t;

static uvc_cam_t intel_cam = {
	.dev_name = "/dev/video1",
	.fd = -1,
	.io = IO_METHOD_MMAP,
	.width = 640,
	.height = 480,
};

struct buffer *buffers = NULL;
static unsigned int n_buffers = 0;
#define IMG_BUFFER_COUNT 10

static char ch = 'a';

static int gobal_exit_flag = 0;

static void errno_exit (const char *s)  {
	fprintf (stderr, "%s error %d, %s\n", s, errno, strerror (errno));
	exit (EXIT_FAILURE);
}

static int xioctl(int fd, int request, void *arg) {
	int r;
	do {
		r = ioctl (fd, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

SDL_Window *sdlWindow;
SDL_Renderer *sdlRenderer;
SDL_Texture *sdlTexture;
void convert(unsigned char *buffer, unsigned char *dataout)
{
	int x, y;
	for(y = 0; y < 480; ++y)
		for(x = 0; x < 640; ++x)
		{
			buffer[640*y*4 + 4*x] = 0;
			buffer[640*y*4 + 4*x + 1] = dataout[640*y + x];
			buffer[640*y*4 + 4*x + 2] = dataout[640*y + x];
			buffer[640*y*4 + 4*x + 3] = dataout[640*y + x];
		}
}

void display(void *args)
{
	int size;
	unsigned char buffer[640 *480 *4];

	convert(buffer, (unsigned char *)args);
	SDL_UpdateTexture(sdlTexture, NULL, buffer, 640 * sizeof(unsigned char) * 4);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);
	SDL_Delay(0); 
}

static struct timespec ts = {0, 0};
static int i = 0;
static void process(void *p, size_t len) {
	display(p);

	//	struct timeval tv;
	//	struct timezone tz;
	//	gettimeofday(&tv, NULL);
	//
	//	int out_fd;
	//	char OUT_FILE[100];
	//	++i;
	//	sprintf(OUT_FILE,"/home/coco/work/fx3_mt9v034/data/pic_%ld_%ld.pic", tv.tv_sec, tv.tv_usec);
	//
	//	out_fd = open(OUT_FILE, O_RDWR | O_CREAT);
	//	if (out_fd <= 0) {
	//		perror("Failed to create output file");
	//		exit;
	//	}
	//
	//
	//	if (0 > write(out_fd, p, len)) {
	//		printf("[%d, %s] Called write failed, errno:%d, %s.\n", __LINE__, __func__, errno, strerror(errno));
	//	}
	//
	//	//	fwrite(p,len,1,out_fd);
	//	printf("image_len = %d\n",(int)len);
	//	close(out_fd);

	return; 
}

static void (*process_image)(void *p, size_t len);

static int read_frame(void)
{
	struct v4l2_buffer buf;
	unsigned int i;

	switch (intel_cam.io) {
		case IO_METHOD_READ:
			if (-1 == read (intel_cam.fd, buffers[0].start, buffers[0].length)) {
				switch (errno) {
					case EAGAIN:
						return 0;
					case EIO:
						/* Could ignore EIO, see spec. */
						/* fall through */
					default:
						errno_exit ("read");
				}
			}
			process_image (buffers[0].start, buffers[0].length);
			break;
		case IO_METHOD_MMAP:
			CLEAR (buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			if (-1 == xioctl (intel_cam.fd, VIDIOC_DQBUF, &buf)) {
				switch (errno) {
					case EAGAIN:
						return 0;
					case EIO:
						/* Could ignore EIO, see spec. */
						/* fall through */
					default:
						errno_exit ("VIDIOC_DQBUF");
				}
			}

			assert (buf.index < n_buffers);
			process_image (buffers[buf.index].start, buffers[buf.index].length);
			//printf("Current frame length: %d.\n", (int)buffers[buf.index].length);
			if (-1 == xioctl (intel_cam.fd, VIDIOC_QBUF, &buf))
				errno_exit ("VIDIOC_QBUF");

			break;
		case IO_METHOD_USERPTR:
			CLEAR (buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;

			if (-1 == xioctl (intel_cam.fd, VIDIOC_DQBUF, &buf)) {
				switch (errno) {
					case EAGAIN:
						return 0;
					case EIO:
						/* Could ignore EIO, see spec. */
						/* fall through */
					default:
						errno_exit ("VIDIOC_DQBUF");
				}
			}

			for (i = 0; i < n_buffers; ++i)
				if (buf.m.userptr == (unsigned long) buffers[i].start
						&& buf.length == buffers[i].length)
					break;

			assert (i < n_buffers);
			process_image ((void *) buf.m.userptr, buf.length);

			if (-1 == xioctl (intel_cam.fd, VIDIOC_QBUF, &buf))
				errno_exit ("VIDIOC_QBUF");

			break;
	}
	return 1;
}
static void stop_capturing();
static void start_capturing();

static void mainloop(void) {
#if 0
	open_yuvfile();
#endif
	int i = 0;
	while (0 == gobal_exit_flag) {
		//printf("%c\n", ch);
		if(ch == 'a')
		{
			if(i == 0)
			{
				printf("&&&&&&&&&&&&&&&&&&&\n");
				start_capturing();
				i = 1;
			}
			for (;;) {
				fd_set fds;
				struct timeval tv;
				int r;

				FD_ZERO (&fds);
				FD_SET (intel_cam.fd, &fds);

				/* Timeout. */
				tv.tv_sec = 1;
				tv.tv_usec = 0;
				r = select (intel_cam.fd + 1, &fds, NULL, NULL, &tv);
				if (-1 == r) {
					if (EINTR == errno)
						continue;

					errno_exit ("select");
				}

				if (0 == r) {
					fprintf (stderr, "select timeout\n");
					exit (EXIT_FAILURE);
				}

				if (read_frame())
					break;
				/* EAGAIN - continue select loop. */
			}
		}
		else// if(ch == 's')
		{
			stop_capturing();
			i = 0;
		}
	}
#if 0
	printf("------------------>write frame count:%d.\n", count);
	close_yuvfile();
#endif   
}

static void stop_capturing(void) {
	enum v4l2_buf_type type;
	switch (intel_cam.io) {
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			//printf("stop capturing\n");

			if (-1 == xioctl (intel_cam.fd, VIDIOC_STREAMOFF, &type))
				errno_exit ("VIDIOC_STREAMOFF");

			break;
	}
}

static void start_capturing(void)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (intel_cam.io) {
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;

		case IO_METHOD_MMAP:
			for (i = 0; i < n_buffers; ++i) {
				struct v4l2_buffer buf;
				CLEAR (buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = i;

				if (-1 == xioctl (intel_cam.fd, VIDIOC_QBUF, &buf))
					errno_exit ("VIDIOC_QBUF");
			}

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			//sleep(1);
			if (-1 == xioctl (intel_cam.fd, VIDIOC_STREAMON, &type))
				errno_exit ("VIDIOC_STREAMON");

			break;

		case IO_METHOD_USERPTR:
			for (i = 0; i < n_buffers; ++i) {
				struct v4l2_buffer buf;
				CLEAR (buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_USERPTR;
				buf.m.userptr = (unsigned long) buffers[i].start;
				buf.length = buffers[i].length;

				if (-1 == xioctl (intel_cam.fd, VIDIOC_QBUF, &buf))
					errno_exit ("VIDIOC_QBUF");
			}

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl (intel_cam.fd, VIDIOC_STREAMON, &type))
				errno_exit ("VIDIOC_STREAMON");
			break;
	}
}

static void uninit_device(void) {
	unsigned int i;

	switch (intel_cam.io) {
		case IO_METHOD_READ:
			free (buffers[0].start);
			break;

		case IO_METHOD_MMAP:
			for (i = 0; i < n_buffers; ++i)
				if (-1 == munmap (buffers[i].start, buffers[i].length))
					errno_exit ("munmap");
			break;

		case IO_METHOD_USERPTR:
			for (i = 0; i < n_buffers; ++i)
				free (buffers[i].start);
			break;
	}

	free (buffers);
}

static void init_read(unsigned int	 buffer_size) {
	buffers = calloc(1, sizeof (*buffers));

	if (!buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}

	buffers[0].length = buffer_size;
	buffers[0].start = malloc (buffer_size);

	if (!buffers[0].start) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}
}

static void init_mmap(void)
{
	struct v4l2_requestbuffers req;
	CLEAR (req);

	req.count = IMG_BUFFER_COUNT/*4*/;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(intel_cam.fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
					"memory mapping\n", intel_cam.dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf (stderr, "Insufficient buffer memory on %s\n",
				intel_cam.dev_name);
		exit (EXIT_FAILURE);
	}

	buffers = calloc (req.count, sizeof (*buffers));

	if (!buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR (buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;

		if (-1 == xioctl (intel_cam.fd, VIDIOC_QUERYBUF, &buf))
			errno_exit ("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
			mmap (NULL /* start anywhere */,
					buf.length,
					PROT_READ | PROT_WRITE /* required */,
					MAP_SHARED /* recommended */,
					intel_cam.fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errno_exit ("mmap");
	}
}

static void init_userp(unsigned int buffer_size) {
	struct v4l2_requestbuffers req;

	CLEAR (req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl (intel_cam.fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
					"user pointer i/o\n", intel_cam.dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_REQBUFS");
		}
	}

	buffers = calloc (4, sizeof (*buffers));

	if (!buffers) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
		buffers[n_buffers].length = buffer_size;
		buffers[n_buffers].start = malloc (buffer_size);

		if (!buffers[n_buffers].start) {
			fprintf (stderr, "Out of memory\n");
			exit (EXIT_FAILURE);
		}
	}
}

static void init_device(void) {
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl (intel_cam.fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s is no V4L2 device\n",
					intel_cam.dev_name);
			exit (EXIT_FAILURE);
		} else {
			errno_exit ("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "%s is no video capture device\n",
				intel_cam.dev_name);
		exit (EXIT_FAILURE);
	}

	switch (intel_cam.io) {
		case IO_METHOD_READ:
			if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
				fprintf (stderr, "%s does not support read i/o\n",
						intel_cam.dev_name);
				exit (EXIT_FAILURE);
			}

			break;

		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
			if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
				fprintf (stderr, "%s does not support streaming i/o\n",
						intel_cam.dev_name);
				exit (EXIT_FAILURE);
			}

			break;
	}

	/* Select video input, video standard and tune here. */
	CLEAR (cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (0 == xioctl (intel_cam.fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl (intel_cam.fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index = 0;
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("Support format:\n");
	while(xioctl(intel_cam.fd,VIDIOC_ENUM_FMT,&fmtdesc) != -1)
	{
		printf("\t%d.%s\n",fmtdesc.index+1,fmtdesc.description);
		fmtdesc.index++;
	}    


	CLEAR (fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	// fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (-1 == xioctl (intel_cam.fd, VIDIOC_S_FMT, &fmt))
		errno_exit ("VIDIOC_S_FMT");
	/* Note VIDIOC_S_FMT may change width and height. */
	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	switch (intel_cam.io) {
		case IO_METHOD_READ:
			init_read (fmt.fmt.pix.sizeimage);
			break;

		case IO_METHOD_MMAP:
			init_mmap ();
			break;

		case IO_METHOD_USERPTR:
			init_userp (fmt.fmt.pix.sizeimage);
			break;
	}
}

static void close_device(void) {
	if (-1 == close (intel_cam.fd))
		errno_exit ("close");
	intel_cam.fd = -1;
}

static void open_device(void) {
	struct stat st;
	if (-1 == stat (intel_cam.dev_name, &st)) {
		fprintf (stderr, "Cannot identify '%s': %d, %s\n",
				intel_cam.dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (!S_ISCHR (st.st_mode)) {
		fprintf (stderr, "%s is no device\n", intel_cam.dev_name);
		exit (EXIT_FAILURE);
	}

	intel_cam.fd = open (intel_cam.dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == intel_cam.fd) {
		fprintf (stderr, "Cannot open '%s': %d, %s\n",
				intel_cam.dev_name, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}
}

static void usage(FILE *fp, int argc, char **argv) {
	fprintf (fp,
			"Usage: %s [options]\n\n"
			"Options:\n"
			"-d | --device name   Video device name [/dev/video]\n"
			"-h | --help          Print this message\n"
			"-m | --mmap          Use memory mapped buffers\n"
			"-r | --read          Use read() calls\n"
			"-u | --userp         Use application allocated buffers\n"
			"-k | --width         Use capture image width\n"
			"-g | --height        Use capture image height\n"
			"",
			argv[0]);
}

static const char short_options [] = "d:hmru:k:g:";

static const struct option
long_options [] = {
	{ "device", required_argument, NULL, 'd' },
	{ "help", no_argument, NULL, 'h' },
	{ "mmap", no_argument, NULL, 'm' },
	{ "read", no_argument, NULL, 'r' },
	{ "userp", no_argument, NULL, 'u' },
	{ "width", required_argument, NULL, 'k'},
	{ "height", required_argument, NULL, 'g'},
	{ 0, 0, 0, 0 }
}; 

static int init_param(int argc, char **argv) {
	for (;;) {
		int index;
		int c;

		c = getopt_long (argc, argv,
				short_options, long_options,
				&index);
		if (-1 == c)
			break;

		switch (c) {
			case 0:
				/* getopt_long() flag */
				break;
			case 'd':
				intel_cam.dev_name = optarg;
				break;
			case 'h':
				usage (stdout, argc, argv);
				exit (EXIT_SUCCESS);
			case 'm':
				intel_cam.io = IO_METHOD_MMAP;
				break;
			case 'r':
				intel_cam.io = IO_METHOD_READ;
				break;
			case 'u':
				intel_cam.io = IO_METHOD_USERPTR;
				break;
			case 'k':
				intel_cam.width = atoi(optarg);
				printf("[%s:%d] image width:%d, width:%s.\n", __func__, __LINE__, intel_cam.width, optarg);
				break;
			case 'g':
				intel_cam.height = atoi(optarg);
				printf("[%s:%d] image height:%d, height:%s.\n", __func__, __LINE__, intel_cam.height, optarg);
				break;  
			default:
				usage (stderr, argc, argv);
				exit (EXIT_FAILURE);
		}
	}
	return 0;    
}

static void sigstop(int signal_num) {
	printf("RECV signal number:%d\n", signal_num);
	switch (signal_num) {
		case (SIGINT):
			printf("Recv SIGINT signal\n"); break;
		case (SIGQUIT):
			printf("Recv SIGQUIT signal\n"); break;
		case (SIGTERM):
			printf("Recv SIGTERM signal\n"); break;
		case (SIGPIPE):
			printf("Recv SIGPIPE signal\n"); break;
		default:
			printf("RECV signal number:%d\n", signal_num);
	}
	gobal_exit_flag = 1;
	return ;   
}

static int init_signal_handle(void) {
	/*register signal handler for Ctrl+C,  Ctrl+'\'  ,  and "kill" sys cmd*/
	signal(SIGINT,  sigstop);
	signal(SIGQUIT, sigstop);
	signal(SIGTERM, sigstop);

	return 0;
}

void *callback(void *argv)
{
	while(1)
	{
		ch = getchar();
		printf("%c\n",ch);
	}
}

	static int
fx3_ram_write (
		libusb_device_handle  *h,
		unsigned char *buf,
		unsigned int   ramAddress,
		int            len)
{
	int r;
	int index = 0;
	int size;

	while (len > 0) {
		size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
		r = libusb_control_transfer (h, 0x40, 0xA0, GET_LSW(ramAddress), GET_MSW(ramAddress),
				&buf[index], size, VENDORCMD_TIMEOUT);
		if (r != size) {
			fprintf (stderr, "Error: Vendor write to FX3 RAM failed\n");
			return -1;
		}

		ramAddress += size;
		index      += size;
		len        -= size;
	}

	return 0;
}

/* Read the firmware image from the file into a buffer. */
	static int
read_firmware_image (
		const char    *filename,
		unsigned char *buf,
		int           *romsize,
		int           *filesize)
{
	int fd;
	int nbr;
	struct stat filestat;

	if (stat (filename, &filestat) != 0) {
		fprintf (stderr, "Error: Failed to stat file %s\n", filename);
		return -1;
	}

	// Verify that the file size does not exceed our limits.
	*filesize = filestat.st_size;
	if (*filesize > MAX_FWIMG_SIZE) {
		fprintf (stderr, "Error: File size exceeds maximum firmware image size\n");
		return -2;
	}

	fd = open (filename, O_RDONLY);
	if (fd < 0) { 
		fprintf (stderr, "Error: File not found\n");
		return -3;
	}
	nbr = read (fd, buf, 2);		/* Read first 2 bytes, must be equal to 'CY'	*/
	if (strncmp ((char *)buf,"CY",2)) {
		fprintf (stderr, "Error: Image does not have 'CY' at start.\n");
		return -4;
	}
	nbr = read (fd, buf, 1);		/* Read 1 byte. bImageCTL	*/
	if (buf[0] & 0x01) {
		fprintf (stderr, "Error: Image does not contain executable code\n");
		return -5;
	}
	if (romsize != 0)
		*romsize = i2c_eeprom_size[(buf[0] >> 1) & 0x07];

	nbr = read (fd, buf, 1);		/* Read 1 byte. bImageType	*/
	if (!(buf[0] == 0xB0)) {
		fprintf (stderr, "Error: Not a normal FW binary with checksum\n");
		return -6;
	}

	// Read the complete firmware binary into a local buffer.
	lseek (fd, 0, SEEK_SET);
	nbr = read (fd, buf, *filesize);

	close (fd);
	return 0;
}

	static int
fx3_usbboot_download (
		libusb_device_handle *h,
		const char   *filename)
{
	unsigned char *fwBuf;
	unsigned int  *data_p;
	unsigned int i, checksum;
	unsigned int address, length;
	int r, index, filesize;

	fwBuf = (unsigned char *)calloc (1, MAX_FWIMG_SIZE);
	if (fwBuf == 0) {
		fprintf (stderr, "Error: Failed to allocate buffer to store firmware binary\n");
		return -1;
	}

	// Read the firmware image into the local RAM buffer.
	r = read_firmware_image (filename, fwBuf, NULL, &filesize);
	if (r != 0) {
		fprintf (stderr, "Error: Failed to read firmware file %s\n", filename);
		free (fwBuf);
		return -2;
	}

	// Run through each section of code, and use vendor commands to download them to RAM.
	index    = 4;
	checksum = 0;
	while (index < filesize) {
		data_p  = (unsigned int *)(fwBuf + index);
		length  = data_p[0];
		address = data_p[1];
		if (length != 0) {
			for (i = 0; i < length; i++)
				checksum += data_p[2 + i];
			r = fx3_ram_write (h, fwBuf + index + 8, address, length * 4);
			if (r != 0) {
				fprintf (stderr, "Error: Failed to download data to FX3 RAM\n");
				free (fwBuf);
				return -3;
			}
		} else {
			if (checksum != data_p[2]) {
				fprintf (stderr, "Error: Checksum error in firmware binary\n");
				free (fwBuf);
				return -4;
			}

			r = libusb_control_transfer (h, 0x40, 0xA0, GET_LSW(address), GET_MSW(address), NULL,
					0, VENDORCMD_TIMEOUT);
			if (r != 0)
				printf ("Info: Ignored error in control transfer: %d\n", r);
			break;
		}

		index += (8 + length * 4);
	}

	free (fwBuf);
	return 0;
}

	void
print_usage_info (
		const char *arg0)
{
	printf ("%s: FX3 firmware programmer\n\n", arg0);
	printf ("Usage:\n");
	printf ("\t%s -h: Print this help message\n\n", arg0);
	printf ("\t%s -t <target> -i <img filename>: Program firmware binary from <img filename>\n", arg0);
	printf ("\t\tto <target>\n");
	printf ("\t\t\twhere <target> is one of\n");
	printf ("\t\t\t\t\"RAM\": Program to FX3 RAM\n");
	printf ("\t\t\t\t\"I2C\": Program to I2C EEPROM\n");
	printf ("\t\t\t\t\"SPI\": Program to SPI FLASH\n");
	printf ("\n\n");
}



int download_fx3(int argc, char **argv)
{
	char         *filename = NULL;
	char         *tgt_str  = NULL;
	fx3_fw_target tgt = FW_TARGET_NONE;
	int r, i;

	/* Parse command line arguments. */
	for (i = 1; i < argc; i++) {
		if ((strcasecmp (argv[i], "-h") == 0) || (strcasecmp (argv[i], "--help") == 0)) {
			print_usage_info (argv[0]);
			return 0;
		} else {
			if ((strcasecmp (argv[i], "-t") == 0) || (strcasecmp (argv[i], "--target") == 0)) {
				if (argc > (i + 1)) {
					tgt_str = argv[i + 1];
					if (strcasecmp (argv[i + 1], "ram") == 0)
						tgt = FW_TARGET_RAM;
					if (strcasecmp (argv[i + 1], "i2c") == 0)
						tgt = FW_TARGET_I2C;
					if (strcasecmp (argv[i + 1], "spi") == 0)
						tgt = FW_TARGET_SPI;
					if (tgt == FW_TARGET_NONE) {
						fprintf (stderr, "Error: Unknown target %s\n", argv[i + 1]);
						print_usage_info (argv[0]);
						return -EINVAL;
					}
				}
				i++;
			} else {
				if ((strcmp (argv[i], "-i") == 0) || (strcmp (argv[i], "--image") == 0)) {
					if (argc > (i + 1))
						filename = argv[i + 1];
					i++;
				} else {
					fprintf (stderr, "Error: Unknown parameter %s\n", argv[i]);
					print_usage_info (argv[0]);
					return -EINVAL;
				}
			}
		}
	}

	if ((filename == NULL) || (tgt == FW_TARGET_NONE)) {
		fprintf (stderr, "Error: Firmware binary or target not specified\n");
		print_usage_info (argv[0]);
		return -EINVAL;
	}


	libusb_device **devs;          //pointer to pointer of device, used to retrieve a list of devices
	libusb_context *ctx = NULL;    //a libusb session
	//int r;                         //for return values
	ssize_t cnt;                   //holding number of devices in list
	r = libusb_init(&ctx);         //initialize a library session
	if(r < 0) 
	{
		printf("Init Error\n");
		return 1;
	}
	libusb_set_debug(ctx, 3);					//set verbosity level to 3, as suggested in the documentation
	cnt = libusb_get_device_list(ctx, &devs);	//get the list of devices
	if(cnt < 0) 
	{
		printf("Get Device Error\n");
	}

	libusb_device_handle *dev_handle;			//a device handle
	dev_handle = libusb_open_device_with_vid_pid(ctx, 0x04b4, 0x00f3); //open device
	if(dev_handle == NULL) 
	{
		printf("Cannot open usb device,cannot download image to device\n");
		printf("if you want to redownload your image,maybe you should restart your device\n");
		libusb_free_device_list(devs, 1); //free the list, unref the devices in it
		libusb_exit(ctx);                 //close the session
		return 0;
	} 
	else 
	{
		printf("Device Opened\n");
		libusb_free_device_list(devs, 1);						//free the list, unref the devices in it
		if(libusb_kernel_driver_active(dev_handle, 0) == 1)		//find out if kernel driver is attached 
		{
			printf("Kernel Driver Active\n");
			if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
				printf("Kernel Driver Detached\n");
		}
		r = libusb_claim_interface(dev_handle, 0);				//claim interface 0 (the first) of device (mine had jsut 1)
		if(r < 0) 
		{
			printf("Cannot Claim Interface\n");
			return 1;
		}
		printf("Claimed Interface\n");
	}

	/*	
		r = cyusb_open ();
		if (r < 0) {
		fprintf (stderr, "Error opening library\n");
		return -ENODEV;
		}
		else if (r == 0) {
		fprintf (stderr, "Error: No FX3 device found\n");
		return -ENODEV;
		}
		else if (r > 1) {
		fprintf (stderr, "Error: More than one Cypress device found\n");
		return -EINVAL;
		}

		h = cyusb_gethandle (0);
		*/
	switch (tgt) {
		case FW_TARGET_RAM:
			r = fx3_usbboot_download (dev_handle, filename);
			break;
		default:
			break;
	}
	if (r != 0) {
		fprintf (stderr, "Error: FX3 firmware programming failed\n");
	} else {
		printf ("FX3 firmware programming to %s completed\n", tgt_str);
	}
	return r;
}

int main(int argc, char **argv)
{
	if(download_fx3(argc, argv) != 0)
		return 0;
	printf("Waiting to enumerate...\n");
	sleep(2);

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("failed to init SDL\n");
	}
	SDL_CreateWindowAndRenderer(640, 480, SDL_WINDOW_SHOWN, &sdlWindow, &sdlRenderer);
	sdlTexture = SDL_CreateTexture(sdlRenderer, 
			SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);

	init_signal_handle();
//	init_param(argc, argv);    

	open_device ();
	init_device ();

//	pthread_t thread_id;
//	pthread_create(&thread_id, NULL, &callback, NULL);
	//	start_capturing ();

	//  init_common_camera_lib();

	process_image = process;

	// config_common_camera_lib(intel_cam.width, intel_cam.height, intel_cam.width, intel_cam.height, 30, 4);
	// start_common_camera_lib();

	mainloop ();

	//  destroy_common_camera_lib();

	stop_capturing ();
	uninit_device ();
	close_device ();

	exit (EXIT_SUCCESS);
	return 0;
}
