#ifndef MODULE 
#define MODULE
#endif

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <unistd.h>
#include <rtai.h>

MODULE_LICENSE("GPL");

#define PORTB_NUM_BUTTONS 6 // 0-5
#define HW_IRQ 59
//software irq 63

unsigned long *BasePtr, *PBDR, *PBDDR;	// pointers for port B DR/DDR
unsigned long *PFDR, *PFDDR;
unsigned long *GPIOBIntEn, *GPIOBIntType1, *GPIOBIntType2;
unsigned long *GPIOBEOI, *GPIOBDB, *IntStsB, *RawIntStsB;

// Hardware interrupt IRQ = 59
// rt_task <plays speaker>
// Change period of rt_task to change speaker freq.

// Run when button interrupt triggered
static void button_handler(unsigned int irq_num, void *cookie) {
	// Disable interrupts 
	rt_disable_irq(HW_IRQ);
	// Check which button pressed
	int i=0;
	//for(i=0; i<PORTB_NUM_BUTTONS; i++)
	//{
	//	if( (*RawIntStsB & (1 << i)) != 0)
	//	{
	//		printk(KERN_INFO, "Button %d pressed\n", i);
	//		break;
	//	}
	//}
	printf("Button pressed\n");
	
	// Clear EOI register by *setting* the bit.
	*GPIOBEOI |= (0x1F);
	// Re-enable interrupts
	rt_enable_irq(HW_IRQ);
}

int init_module(void) {
	int i=0;
	// Attempt to map file descriptor
	BasePtr = (unsigned long *) __ioremap(0x80840000, 4096, 0);
	if(NULL == BasePtr) 
	{
		printk(KERN_INFO "Unable to map memory space\n");
		return -1;
	}
	// Map registers
	PBDR = BasePtr + 1;
	PBDDR = BasePtr + 5;
	PFDR = BasePtr + 11;
	PFDDR = BasePtr + 12;
	GPIOBIntType1 = BasePtr + 32;
	GPIOBIntType2 = BasePtr + 33;
	GPIOBEOI = BasePtr + 34;
	GPIOBIntEn = BasePtr + 35;
	IntStsB = BasePtr + 36;
	RawIntStsB = BasePtr + 37;
	GPIOBDB = BasePtr + 38;
	
	// Set push buttons as inputs
	for(i=0; i<PORTB_NUM_BUTTONS; i++)
	{
		*PBDDR &= ~(1 << i);
	}
	
	*GPIOBIntEn &= 0x00; // Disable all interrupts on PORTB
	*GPIOBIntType1 |= (0x1F); // Set interrupts to edge sensitive 
	*GPIOBIntType2 |= (0x1F); // Set interrupts to rising edge 
	*GPIOBDB |= (0x1F);	// Enable debounce
	*GPIOBEOI |= 0xFF;	// Set all the bits to clear all the interrupts
	*GPIOBIntEn |= (0x1F); // Enable interrupt on B0-B4
	
	// Attempt to attach handler
	if(rt_request_irq(HW_IRQ, button_handler, NULL, 1) < 0)
	{
		printk(KERN_INFO, "Unable to request IRQ\n");
		return -1;
	}
	
	// Enable interrupt
	rt_enable_irq(HW_IRQ);

	printk(KERN_INFO, "MODULE INSTALLED\n");
	return 0;
}

void cleanup_module(void) {
	rt_release_irq(HW_IRQ);
	printk(KERN_INFO "MODULE REMOVED\n");
	return;
}
