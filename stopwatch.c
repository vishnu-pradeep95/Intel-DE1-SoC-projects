#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/init.h>
#include<linux/interrupt.h>
#include<asm/io.h>
#include"../address_map_arm.h"
#include"../interrupt_ID.h"

void * LW_virtual;
volatile int *LEDR_ptr, *KEY_ptr,*SW_ptr;
volatile int *HEX_ptr, *HEX_ptr1;
unsigned char arrt[10]={ 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
// Lightweight bridge base address
// virtual addresses
irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
*LEDR_ptr = *LEDR_ptr + 1;
// Clear the Edgecapture register (clears current interrupt)
*(KEY_ptr + 3) = 0xF;
return (irq_handler_t) IRQ_HANDLED;
}

static int __init initialize_pushbutton_handler(void)
{
int value1,value2;
// generate a virtual address for the FPGA lightweight bridge
LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
SW_ptr = LW_virtual + SW_BASE;
KEY_ptr = LW_virtual + KEY_BASE;

HEX_ptr1=LW_virtual+HEX5_HEX4_BASE;
HEX_ptr=LW_virtual+HEX3_HEX0_BASE;




*(KEY_ptr + 3) = 0xF; // Clear the Edgecapture register
*(KEY_ptr + 2) = 0xF; // Enable IRQ generation for the 4 buttons
// Register the interrupt handler.
value1 = request_irq (KEYS_IRQ, (irq_handler_t) irq_handler, IRQF_SHARED,
"pushbutton_irq_handler", (void *) (irq_handler));
value2 = request_irq (72, (irq_handler_t) irq_handler, IRQF_SHARED,
"clck_irq_handler", (void *) (irq_handler));
return value;
}


static void __exit cleanup_pushbutton_handler(void)
{
*LEDR_ptr = 0; // Turn off LEDs and de-register irq handler
iounmap (LW_virtual);
free_irq (KEYS_IRQ, (void *) irq_handler);
}

static void __exit cleanup_clck_handler(void)
{
*LEDR_ptr = 0; // Turn off LEDs and de-register irq handler
iounmap (LW_virtual);
free_irq (72, (void *) irq_handler);
}

module_init(initialize_pushbutton_handler);
module_init(initialize_clck_handler);

module_exit(cleanup_pushbutton_handler);
module_exit(cleanup_clck_handler);