#ifndef MODULE 
#define MODULE
#endif

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <rtai.h>
#include <rtai_sched.h>

#define FIFO_ID 1 ///dev/rtf/1

/* TODO:
   Add software handler to change frequency of speaker.
   Software handler interrupt is triggered by write from user space program.
   Communicate with user space program through rtfifo i.e. to get the value of the note (@A-@E)
   Change the frequency accordingly
*/


MODULE_LICENSE("GPL");

#define PORTB_NUM_BUTTONS 6 // 0-5
const int hw_irq = 59;
const int sw_irq = 63;

RTIME period;
RT_TASK t1;
volatile unsigned char *BasePtr, *PBDR, *PBDDR;	// pointers for port B DR/DDR
volatile unsigned char *PFDR, *PFDDR;
volatile unsigned char *GPIOBIntEn, *GPIOBIntType1, *GPIOBIntType2;
volatile unsigned char *GPIOBEOI, *GPIOBDB, *IntStsB, *RawIntStsB;
volatile unsigned char *VIC2IntEnable, *VIC2SoftInt, *VIC2SoftIntClear;

static void read_fifo(unsigned int irq_num, void *cookie) {
	// Disable interrupt
	rt_disable_irq(sw_irq);

	// Clear interrupt
	*VIC2SoftIntClear |= 0x80; // Set MSB to clear interrupt
	RTIME task_period;
	int i=0;
	int ret = rtf_get(FIFO_ID, &i, sizeof(i));
	if(ret < 0) {
		printk("Error reading from FIFO\n");
	}
	task_period = (1+i)*period;
	rt_task_make_periodic(&t1, 0*period, task_period);

	rt_enable_irq(sw_irq);
}


static void play_speaker(void) {
	// Set PF1 as output
	*PFDDR |= 0x02;
	static int pin_val = 0;
	while(1)
	{
		if(1 == pin_val)
		{
			*PFDR |= 0x02;
			pin_val = 0;
		}
		else if(0 == pin_val) 
		{
			*PFDR &= ~(0x02);
			pin_val = 1;
		}
		rt_task_wait_period();
	}
}

// Run when button interrupt triggered
static void button_handler(unsigned int irq_num, void *cookie) {
	static RTIME task_period;
	// Disable interrupts 
	rt_disable_irq(hw_irq);
	// Check which button pressed
	// If RawIntSts == 1 then that button was pressed
	int i=0;
	for(i=0; i<PORTB_NUM_BUTTONS; i++)
		if( (*RawIntStsB & (1 << i)) != 0)
		{

			printk("Button %d pressed\n", i);
			task_period = (1+i)*period;
			rt_task_make_periodic(&t1, 0*period, task_period);
			break;
		}
	// Clear EOI register by *setting* the bit.
	*GPIOBEOI |= (0x1F);
	// Re-enable interrupts
	rt_enable_irq(hw_irq);
}

int init_module(void) {
	int i=0;
	// Attempt to map file descriptor
	BasePtr = (unsigned char *) __ioremap(0x80840000, 4096, 0);
	PBDR = (unsigned char *) __ioremap(0x80840004, 4096, 0);
	PBDDR = (unsigned char *) __ioremap(0x80840014, 4096, 0);
	PFDR  = (unsigned char *) __ioremap(0x80840030, 4096, 0);
	PFDDR = (unsigned char *) __ioremap(0x80840034, 4096, 0);
	GPIOBIntType1 = (unsigned char *) __ioremap(0x808400AC, 4096, 0);
	GPIOBIntType2 = (unsigned char *) __ioremap(0x808400B0, 4096, 0);
	GPIOBEOI = (unsigned char *) __ioremap(0x808400B4, 4096, 0); 
	GPIOBIntEn = (unsigned char *) __ioremap(0x808400B8, 4096, 0);
	IntStsB = (unsigned char *) __ioremap(0x808400BC, 4096, 0);
	RawIntStsB = (unsigned char *) __ioremap(0x808400C0, 4096, 0);
	GPIOBDB = (unsigned char *) __ioremap(0x808400C4, 4096, 0);
	VIC2IntEnable = (unsigned char *) __ioremap(0x800C0010, 4096, 0);
	VIC2SoftInt = (unsigned char *) __ioremap(0x800C0018, 4096, 0);
	VIC2SoftIntClear = (unsigned char *) __ioremap(0x800C001C, 4096, 0);

	
	// Enable rt_task to play speaker
	rt_set_periodic_mode();
	period = start_rt_timer(nano2count(1000000));
	rt_task_init(&t1, (void *)play_speaker, 0, 256, 0, 0, 0);
	rt_task_make_periodic(&t1, 0*period, 1*period);

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
	if(rt_request_irq(hw_irq, button_handler, NULL, 1) < 0)
	{
		printk("Unable to request IRQ\n");
		return -1;
	}
	// Enable interrupt
	rt_enable_irq(hw_irq);

	*VIC2IntEnable |= 0x80; // msb 1 set, msb 1 clear
	if(rtf_create(FIFO_ID, sizeof(int)) < 0) {
		printk("Unable to create fifo\n");
		return -1;
	}
	//*VIC2SoftInt // userspace 
	//*VIC2SoftIntClear  

	if(rt_request_irq(sw_irq, read_fifo, NULL, 1) < 0)
	{
		printk("Unable to request SW IRQ\n");
		return -1;
	}

	rt_enable_irq(sw_irq);

	printk("MODULE INSTALLED\n");
	return 0;
}

void cleanup_module(void) {
	rt_task_delete(&t1);
	stop_rt_timer();
	rt_disable_irq(hw_irq);
	rt_release_irq(hw_irq);
	rt_disable_irq(sw_irq);
	rt_release_irq(sw_irq);
	printk("MODULE REMOVED\n");
	return;
}
