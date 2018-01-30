#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/types.h>

#define DEVICE_NAME   			"AXI_DMA_DEVICE"
#define CLASS_NAME    			"axi_dma"
#define DMA_BASE_ADDRESS 	 	 0xa0000000
#define MM2S_CONTROL_REGISTER 	 	 0x00
#define MM2S_STATUS_REGISTER    	 0x04
#define MM2S_SOURCE_ADDRESS     	 0x18
#define MM2S_LENGTH 	        	 0x28
#define S2MM_CONTROL_REGISTER 	 	 0x30
#define S2MM_STATUS_REGISTER 	 	 0x34
#define S2MM_DESTINATION_ADDRESS 	 0x48
#define S2MM_LENGTH 		    	 0x58
#define SOURCE_ADDRESS		   	 0xFFFC0000		//Points to OCM-RAM of Zynq-UltraScale+ MPSoc (ZCU102)
#define DESTINATION_ADDRESS      	 0xFFFC2000		
#define LENGTH			  	 1024

MODULE_LICENSE("GPL");              
MODULE_AUTHOR("Harry-SJTU");      
MODULE_DESCRIPTION("A simple Linux driver for the AXI DMA."); 
MODULE_VERSION("1.0");



static void __iomem *dma_virt_addr;
static void __iomem *virt_source_buf_address;
static void __iomem *virt_destination_buf_address;  
unsigned int  length;

static int    message[1024] = {0};
static int    majorNumber;                             
static struct class*  axidmaclass  = NULL;
static struct device* axidmaDevice = NULL;		
static int     axi_dma_open(struct inode *, struct file *);
static int     axi_dma_release(struct inode *, struct file *);
static ssize_t axi_dma_read(struct file *, char *, size_t, loff_t *);
static ssize_t axi_dma_write(struct file *, const char *, size_t, loff_t *);
 
 static int axi_dma_open(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "AXI DMA: Device has been opened time(s)\n");
   return 0;
}

static struct file_operations axi_dma={

.owner=THIS_MODULE,
.open=axi_dma_open,
.release=axi_dma_release,
.read=axi_dma_read,
.write=axi_dma_write,
};

static int __init axi_dma_init(void){
// Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &axi_dma);
   if (majorNumber<0){
      printk(KERN_ALERT "axidma failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "axidma: registered correctly with major number %d\n", majorNumber);


   // Register the device class
   axidmaclass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(axidmaclass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(axidmaclass);          // Correct way to return an error on a pointer
   }

// Register the device driver
   axidmaDevice= device_create(axidmaclass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(axidmaDevice)){               // Clean up if there is an error
      class_destroy(axidmaclass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(axidmaDevice);
   }

   printk(KERN_INFO "axidma: device class registered correctly\n");

//request memory region

if(request_mem_region(0xFFFC0000,256*1024,"axi_dma_mmeory")==NULL){
printk("failed to request memory region\n");
}


//MAP SOURCE BUFFER (Could be DDR/OCM)
virt_source_buf_address = ioremap_nocache(0xFFFC0000, (0x2000));

       if (!virt_source_buf_address ) {
                printk("Could not map physical memory to virtual\n");
                return -1;
        }
//MAP DESTINATION BUFFER
virt_destination_buf_address=ioremap_nocache(0xFFFC2000, (0x2000));

	if (!virt_destination_buf_address ) {
               printk("Could not map physical memory to virtual\n");
               return -1;
        }
//INITIALIZE DESTINATION BUFFER
memset_io(virt_destination_buf_address,0x0,LENGTH);

printk("Destination buffer initialized");

//MAP AXI DMA BASE ADDRESS
dma_virt_addr=ioremap_nocache(DMA_BASE_ADDRESS,65535);
	
if (!dma_virt_addr) {
               printk("Could not map AXI DMA\n");
               return -1;
        }

	//START MM2S DMA OPERATION
	iowrite32(0x01,(void __iomem *)(dma_virt_addr+MM2S_CONTROL_REGISTER));
	printk("MM2S CONTROL REGISTER %d\n", ioread32((void __iomem *)(dma_virt_addr+MM2S_CONTROL_REGISTER)));

	//START S2MM DMA OPERATION	
	iowrite32(0x01,(void __iomem *)(dma_virt_addr+S2MM_CONTROL_REGISTER));	
	printk("S2MM CONTROL REGISTER %d\n", ioread32((void __iomem *)(dma_virt_addr+S2MM_CONTROL_REGISTER)));

	//WRITE ADDRESS OF SOURCE BUFFER 
	iowrite32(SOURCE_ADDRESS,(void __iomem *)(dma_virt_addr+MM2S_SOURCE_ADDRESS));
	printk("MM2S_SOURCE_ADDRESS 0x%x\n", ioread32((void __iomem *)(dma_virt_addr+MM2S_SOURCE_ADDRESS)));

	//WRITE ADDRESS OF DESTINATION BUFFER		
	iowrite32(DESTINATION_ADDRESS,(void __iomem *)(dma_virt_addr+S2MM_DESTINATION_ADDRESS));
	printk("S2MM_DESTINATION_ADDRESS 0x%x\n", ioread32((void __iomem *)(dma_virt_addr+S2MM_DESTINATION_ADDRESS)));

	//PRINT THE STATUS REGISTERS 
	printk("MM2S STATUS REGISTER %d\n", ioread32((void __iomem *)(dma_virt_addr+MM2S_STATUS_REGISTER)));
	printk("S2MM STATUS REGISTER %d\n", ioread32((void __iomem *)(dma_virt_addr+S2MM_STATUS_REGISTER)));

return 0;

}

static ssize_t axi_dma_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){

length=len; // GET THE NO BYTES TO WRITE TO MM2S LENGTH REGISTER 

//COPY DATA BUFFER FROM USER-SPACE
len=copy_from_user(message,buffer,len);

//STORE USER BUFFER DATA TO OCM-RAM
memcpy_toio(virt_source_buf_address,message,length );

//WRITE MM2S LENGTH REGISTER TO START MM2S TRANSFER (FROM ARM HOST TO PL)
iowrite32(length,(void __iomem *)(dma_virt_addr+MM2S_LENGTH));

//WAIT FOR MM2S DMA TRANSFER COMPLETION
while(!(ioread32((void __iomem *)(dma_virt_addr+MM2S_STATUS_REGISTER)) & 0x1002));

//PRINT THE STATUS OF MM2S TRANSFER
printk("MM2S STATUS REGISTER %d\n", ioread32((void __iomem *)(dma_virt_addr+MM2S_STATUS_REGISTER)));

return len;

}

static ssize_t axi_dma_read(struct file *filep, char *buffer, size_t len, loff_t *offset){

//START TRANSFER S2MM (FROM PL TO ARM HOST) BY WRITING NO BYTES TO BE READ
iowrite32(len,(void __iomem *)(dma_virt_addr+S2MM_LENGTH));

//WAIT FOR S2MM DMA TRANSFER COMPLETION
while(!(ioread32((void __iomem *)(dma_virt_addr+S2MM_STATUS_REGISTER)) & 0x1002));

//COPY DATA WRITTEN AT DESTINATION BUFFER (ADDRESS WRITTEN  IN DESTINATION ADDRESS REGISTER) TO LOCAL BUFFER (MESSAGE) 
memcpy_fromio(message,virt_destination_buf_address,len);

//SEND DATA BACK TO USERSPACE RECEIVED FROM S2MM CHANNEL
len=copy_to_user(buffer,message,len);

return len;

}


static int axi_dma_release(struct inode *inodep, struct file *filep){
  printk(KERN_INFO "AXI DMA: Device successfully closed\n");
  return 0;
}
 static void __exit axi_dma_exit(void){
	device_destroy(axidmaclass, MKDEV(majorNumber, 0));     // remove the device
   	class_unregister(axidmaclass);                          // unregister the device class
   	class_destroy(axidmaclass);                             // remove the device class
   	unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major num
	release_mem_region(0xFFFC0000 ,(256*1024));
        printk(KERN_INFO "AXI DMA Exit\n");
}
module_init(axi_dma_init);
module_exit(axi_dma_exit);

	


	
