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
#include <rtai_fifos.h>

MODULE_LICENSE("GPL");

#define FIFO_READ 0 // /dev/rtf/0
#define FIFO_WRITE 1 ///dev/rtf/1
#define PORTB_NUM_BUTTONS 6 // 0-5
const int hw_irq = 59;
const int sw_irq = 63;

RTIME period;
RT_TASK t1;
volatile unsigned char *BasePtr, *PBDR, *PBDDR;	// pointers for port B DR/DDR
volatile unsigned char *PFDR, *PFDDR;
volatile unsigned char *GPIOBIntEn, *GPIOBIntType1, *GPIOBIntType2;
volatile unsigned char *GPIOBEOI, *GPIOBDB, *IntStsB, *RawIntStsB;
volatile unsigned long *VIC2IntEnable, *VIC2SoftIntClear;

static void read_fifo(unsigned int irq_num, void *cookie) {
	// Disable interrupt
	rt_disable_irq(irq_num);
	printk("Software handler activated\n");
	// Clear interrupt
	RTIME task_period = period;
	int i=0;
	int ret = rtf_get(FIFO_READ, &i, sizeof(i));
	if(ret < 0) {
		printk("Error reading from FIFO\n");
	}
	else {
		task_period = (1+i)*period;
		rt_task_make_periodic(&t1, rt_get_time(), task_period);
	}

	*VIC2SoftIntClear |= 1 << 31; // Clear interrupt by setting the MSB
	rt_enable_irq(irq_num);
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
	rt_disable_irq(irq_num);
	// Check which button pressed
	// If RawIntSts == 1 then that button was pressed
	int i=0;
	for(i=0; i<PORTB_NUM_BUTTONS; i++)
	{
		if( (*RawIntStsB & (1 << i)) )
		{

			printk("Button %d pressed\n", i);
			task_period = (1+i)*period;
			rtf_put(FIFO_WRITE, &i, sizeof(i)); // Write 'note' to FIFO 
			rt_task_make_periodic(&t1, 0*period, task_period);
			break;
		}
	}
	// Clear EOI register by *setting* the bit.
	*GPIOBEOI |= (0x1F);
	// Re-enable interrupts
	rt_enable_irq(irq_num);
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
	// SW Int
	unsigned long *BasePtrB = (unsigned long *) __ioremap(0x800C0000, 4096, 0);
	VIC2IntEnable = (unsigned long *)((char *)BasePtrB + 0x10);
	VIC2SoftIntClear = (unsigned long *)((char *)BasePtrB + 0x1C);
	
	// Enable rt_task to play speaker
	rt_set_periodic_mode();
	period = start_rt_timer(nano2count(1000000));
	rt_task_init(&t1, (void *)play_speaker, 0, 256, 0, 0, 0);
	rt_task_make_periodic(&t1, 0*period, period);

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
	// Enable SW interrupt IRQ 63
	*VIC2IntEnable |= 1 << 31;

	// Attempt to attach handler
	if(rt_request_irq(hw_irq, button_handler, NULL, 1) < 0)
	{
		printk("Unable to request IRQ\n");
		return -1;
	}
	if(rt_request_irq(sw_irq, read_fifo, NULL, 1) < 0)
	{
		printk("Unable to request SW IRQ\n");
		return -1;
	}

	if(rtf_create(FIFO_READ, sizeof(int)) < 0) {
		printk("Unable to create fifo\n");
		return -1;
	}
	if(rtf_create(FIFO_WRITE, sizeof(int)) < 0) {
		printk("Unable to create fifo\n");
		return -1;
	}
	
	// Enable interrupt
	rt_enable_irq(hw_irq);
	rt_enable_irq(sw_irq);

	printk("MODULE INSTALLED\n");
	return 0;
}

void cleanup_module(void) {
	rt_disable_irq(hw_irq);
	rt_release_irq(hw_irq);
	rt_disable_irq(sw_irq);
	rt_release_irq(sw_irq);
	rtf_destroy(FIFO_READ);
	rtf_destroy(FIFO_WRITE);
	rt_task_delete(&t1);
	stop_rt_timer();
	printk("MODULE REMOVED\n");
}
