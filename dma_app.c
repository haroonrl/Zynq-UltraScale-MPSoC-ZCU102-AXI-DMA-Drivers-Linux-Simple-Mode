

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/mman.h>
#include<stdlib.h>
#include<stdint.h>
#include<inttypes.h>

#define LENGTH   8

//Declare users-space buffers
static  int recv_buffer [1024]={0};		//Initialize Receive Buffer
static  int send_buffer [1024]={0};	       //Initialize Send Buffer

int main() {
	int i,ret;
	//Open AXI DMA Device which represent the whole Physical Memory
	int dh = open("/dev/AXI_DMA_DEVICE", O_RDWR|O_SYNC); 

	if(dh<0){
		printf("Failed to Open device");} 

	//fill the userspasce buffer (with some random) data to be sent
	for(i=0;i<LENGTH;i++){
	send_buffer[i+1]=send_buffer[i]+1;
	printf("%d\n",send_buffer[i]);
}
		
	//write the userspace buffer to AXI-DMA Device
	ret = write(dh,send_buffer, LENGTH*sizeof(int));
	printf("data written %d\n",ret);

	if (ret < 0){
      perror("Failed to write the message to the device.");
	
      
   }
	//Read back the data form AXI-DMA Device in the userspace receive buffer
	ret = read(dh,recv_buffer,LENGTH*sizeof(int));

	if (ret < 0){
     	 printf("Failed to read the message from the device.");
    
   }
	
	printf("data read %d\n", ret);

	//Verify the Data received 
	for(i=0;i<LENGTH;i++){

	printf("The received message is: [%d]\n",recv_buffer[i]);
	
}

return 0;

}
