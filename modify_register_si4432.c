#include <stdio.h>
#include <libusb-1.0/libusb.h>


//send setup command
int send_ID_CHECK(libusb_device_handle *dev_handle)
{
	unsigned char byte;

	byte = libusb_control_transfer(dev_handle, //a handle for the device to communicate
			0xC0,	//the request type field for the setup packet
			0xB0,	//the request field
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

//send setup command
int send_FLASH_WRITE(libusb_device_handle *dev_handle)
{
	unsigned char byte;
	unsigned char buffer[20] = "hello world";

	byte = libusb_control_transfer(dev_handle, //a handle for the device to communicate
			0x40,	//the request type field for the setup packet
			0xC2,	//the request field
			0x00,	//the value field
			0,		//the index field	//endpoint to use
			buffer,	//a suitable-sized data buffer for either input or output
			0x14,	//the length field for the setup packet
			1000);	//timeout

	if(byte == 0)
		printf("Send setup command done.\n");
	else
		printf("Send setup command fail.\n");
}

int main() 
{
	libusb_device **devs;          //pointer to pointer of device, used to retrieve a list of devices
	libusb_context *ctx = NULL;    //a libusb session
	int r;                         //for return values
	ssize_t cnt;                   //holding number of devices in list
    r = libusb_init(&ctx);         //initialize a library session
    if(r < 0) 
	{
        printf("Init Error \n"); //there was an error
        return 1;
    }
    libusb_set_debug(ctx, 3);					//set verbosity level to 3, as suggested in the documentation
    cnt = libusb_get_device_list(ctx, &devs);	//get the list of devices
	if(cnt < 0) 
	{
		printf("Get Device Error\n");			//there was an error
	}

	libusb_device_handle *dev_handle;			//a device handle
	dev_handle = libusb_open_device_with_vid_pid(ctx, 0x04b4, 0x00f0); //open device
	if(dev_handle == NULL) 
	{
	        printf("Cannot open device\n");
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
				printf("Kernel Driver Detached!\n");
		}
		r = libusb_claim_interface(dev_handle, 0);				//claim interface 0 (the first) of device (mine had jsut 1)
		if(r < 0) 
		{
			printf("Cannot Claim Interface\n");
			return 1;
		}
		printf("Claimed Interface\n");

		send_ID_CHECK(dev_handle);				//send setup command			
		send_FLASH_WRITE(dev_handle);

		r = libusb_release_interface(dev_handle, 0); //release the claimed interface
		if(r!=0) 
		{
			printf("Cannot Release Interface\n");
			return 1;
		}
		printf("Released Interface\n");

		libusb_attach_kernel_driver(dev_handle, 0);
		libusb_close(dev_handle);
		libusb_exit(ctx); //close the session
		return 0; 
	}
}
