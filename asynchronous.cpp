#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <libusb.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <SDL2/SDL.h>
using namespace std;

#define width 752
#define height 480

// create SDL window and render
SDL_Window *sdlWindow;
SDL_Renderer *sdlRenderer;
SDL_Texture *sdlTexture;  

//send setup command
int send_setup_cmd(libusb_device_handle *dev_handle)
{
	unsigned char byte;

	byte = libusb_control_transfer(dev_handle, //a handle for the device to communicate
			0x40,	//the request type field for the setup packet
			0xa8,	//the request field
			0x00,	//the value field
			0,		//the index field	//endpoint to use
			NULL,	//a suitable-sized data buffer for either input or output
			0x00,	//the length field for the setup packet
			1000);	//timeout

	if(byte == 0)
		printf("Send setup command done.\n");
	else
		printf("Send setup command fail.\n");
}

int send_close_cmd(libusb_device_handle *dev_handle)
{
	unsigned char byte;

	byte = libusb_control_transfer(dev_handle, //a handle for the device to communicate
			0x40,	//the request type field for the setup packet
			0xa9,	//the request field
			0x00,	//the value field
			0,		//the index field	//endpoint to use
			NULL,	//a suitable-sized data buffer for either input or output
			0x00,	//the length field for the setup packet
			1000);	//timeout

	if(byte == 0)
		printf("Send close command done.\n");
	else
		printf("Send close command fail.\n");
}

void convert(unsigned char *buffer, unsigned char *dataout)
{
	for(int y = 0; y < height; ++y)
		for(int x = 0; x < width; ++x)
		{
			buffer[width*y*4 + 4*x] = 0;
			buffer[width*y*4 + 4*x + 1] = dataout[width*y + x];
			buffer[width*y*4 + 4*x + 2] = dataout[width*y + x];
			buffer[width*y*4 + 4*x + 3] = dataout[width*y + x];
		}
}

void display(void *args)
{
	int size;
	unsigned char buffer[width *height *4];
	
	convert(buffer, (unsigned char *)args);
	SDL_UpdateTexture(sdlTexture, NULL, buffer, width * sizeof(unsigned char) * 4);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);
	SDL_Delay(0); 
}

void save_as_picture(unsigned char *data)
{
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, NULL);

	char name[100];
	memset(name, 0, 100);
	sprintf(name,"/home/coco/pic/%ld_%ld.pic", tv.tv_sec, tv.tv_usec);
	FILE *file = fopen(name, "wb+");
	fwrite(data, 1, 360960, file);
	fclose(file);
}

static int loop = 0;

void callbackfnc(struct libusb_transfer *transfer)
{
//	save as picture
//	save_as_picture(transfer->buffer);

//	display use SDL2
//	struct timeval tv1;
//	struct timeval tv;
//	gettimeofday(&tv1, NULL);
//	printf("%ld_%ld ", tv1.tv_sec, tv1.tv_usec);
	display(transfer->buffer);
//	gettimeofday(&tv, NULL);
//	printf("%ld_%ld\n", tv.tv_sec, tv.tv_usec);
//	printf("%f\n", ((tv.tv_sec - tv1.tv_sec) * 1000000 + (tv.tv_usec - tv1.tv_usec)) / 1000.0);
//
	libusb_submit_transfer(transfer);
}
void callbackfnc1(struct libusb_transfer *transfer)
{
//	save as picture
//	save_as_picture(transfer->buffer);

//	display use SDL2
	display(transfer->buffer);

	libusb_submit_transfer(transfer);
}
void callbackfnc2(struct libusb_transfer *transfer)
{
//save as picture
//	save_as_picture(transfer->buffer);

//	display use SDL2
	display(transfer->buffer);

	libusb_submit_transfer(transfer);
}

//read data from sensor
void get_data_from_sensor(libusb_device_handle *dev_handle, libusb_context *ctx)
{
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char buf[width*height];
	libusb_fill_bulk_transfer(transfer, dev_handle, 0x82, buf, sizeof(buf), callbackfnc, NULL, 1000);

	struct libusb_transfer *transfer1 = libusb_alloc_transfer(0);
	unsigned char buf1[width*height];
	libusb_fill_bulk_transfer(transfer1, dev_handle, 0x82, buf1, sizeof(buf1), callbackfnc1, NULL, 1000);

	struct libusb_transfer *transfer2 = libusb_alloc_transfer(0);
	unsigned char buf2[width*height];
	libusb_fill_bulk_transfer(transfer2, dev_handle, 0x82, buf2, sizeof(buf), callbackfnc2, NULL, 1000);

	libusb_submit_transfer(transfer);
	libusb_submit_transfer(transfer1);
	libusb_submit_transfer(transfer2);

	while(1)
	{
		libusb_handle_events(ctx);
	}

    // quit SDL
    SDL_Quit();
}

int main() 
{
	// init SDL
    if( SDL_Init( SDL_INIT_EVERYTHING ) != 0 )
    {
        cout<<"failed to init SDL"<<endl;
    }

    SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_SHOWN, &sdlWindow, &sdlRenderer);    
	sdlTexture = SDL_CreateTexture(sdlRenderer, 
			SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);

	libusb_device **devs;          //pointer to pointer of device, used to retrieve a list of devices
	libusb_context *ctx = NULL;    //a libusb session
	int r;                         //for return values
	ssize_t cnt;                   //holding number of devices in list
    r = libusb_init(&ctx);         //initialize a library session
    if(r < 0) 
	{
        cout<<"Init Error "<<r<<endl; //there was an error
        return 1;
    }
    libusb_set_debug(ctx, 3);					//set verbosity level to 3, as suggested in the documentation
    cnt = libusb_get_device_list(ctx, &devs);	//get the list of devices
	if(cnt < 0) 
	{
		cout<<"Get Device Error"<<endl;			//there was an error
	}

	libusb_device_handle *dev_handle;			//a device handle
	dev_handle = libusb_open_device_with_vid_pid(ctx, 0x52cb, 0x52cb); //open device
	if(dev_handle == NULL) 
	{
	        cout<<"Cannot open device"<<endl;
	    	libusb_free_device_list(devs, 1); //free the list, unref the devices in it
	    	libusb_exit(ctx);                 //close the session
	    	return 0;
	} 
	else 
	{
		cout<<"Device Opened"<<endl;
		libusb_free_device_list(devs, 1);						//free the list, unref the devices in it
		if(libusb_kernel_driver_active(dev_handle, 0) == 1)		//find out if kernel driver is attached 
		{
			cout<<"Kernel Driver Active"<<endl;
			if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
				cout<<"Kernel Driver Detached!"<<endl;
		}
		r = libusb_claim_interface(dev_handle, 0);				//claim interface 0 (the first) of device (mine had jsut 1)
		if(r < 0) 
		{
			cout<<"Cannot Claim Interface"<<endl;
			return 1;
		}
		cout<<"Claimed Interface"<<endl;

		send_setup_cmd(dev_handle);				//send setup command			
		get_data_from_sensor(dev_handle, ctx);		//read data from sensor
		send_close_cmd(dev_handle);				//send close command


		r = libusb_release_interface(dev_handle, 0); //release the claimed interface
		if(r!=0) 
		{
			cout<<"Cannot Release Interface"<<endl;
			return 1;
		}
		cout<<"Released Interface"<<endl;

		libusb_attach_kernel_driver(dev_handle, 0);
		libusb_close(dev_handle);
		libusb_exit(ctx); //close the session
		return 0; 
	}
}
