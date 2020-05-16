#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/timer.h>
#include <linux/export.h>
#include <net/genetlink.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include "gen_netlink.h"

#define DRIVER_NAME 	"spi_netlink"
#define CLASS_NAME 		"spi_netlink"
#define DEVICE_NAME		"spi_netlink"

#define MAJOR_NUMBER    175 

//Structure for SPI device
struct spi_led_dev {
	struct spi_device	*spi;
	unsigned char frames[10][8];
	int cs_pin_gpio;
	int cs_pin_shield;
};

//Structure for HCSR device
struct hcsr_dev {
    int trigger_pin_gpio;
    int trigger_pin_shield;
    int echo_pin_gpio;
    int echo_pin_shield;
    int irq_line;
    int is_measuring;
    unsigned long long begin;
    unsigned long long end;
};

static unsigned char ch_tx[2]={0};
static unsigned char ch_rx[2]={0};

static struct spi_transfer transfer = 
{
	.tx_buf = &ch_tx[0],
	.rx_buf = &ch_rx[0],
	.len = 2,
	.cs_change = 1,
	.bits_per_word = 8,
	.speed_hz = 500000,
};

static struct genl_family gnl_family;
static struct hcsr_dev *hcsr_struct;
static struct spi_message msg;
static struct spi_led_dev *spi_dev=NULL;
static struct class *spi_led_class;
static int configured=0;

//function prototypes
int spi_render_frame(void *data);
void free_gpio(void);
int led_gpio_config(int cs_pin);
static int init_led_display(void);
static void send_to_spi(unsigned char ch1, unsigned char ch2);
int hcsr_pin_config(int arr[]);
int perform_measurement(void *data);
void free_pin(int t);
int check_and_set_trigger(int t);
int check_and_set_echo(int e);
void free_echo(int e);

static const struct genl_multicast_group genl_groups[] = {
    [GENL_GROUP] = { .name = GENL_GROUP_NAME, },
};

static struct spi_board_info spi_led_info = {
	.modalias = DEVICE_NAME,
	.max_speed_hz = 500000,
	.bus_num = 1,
	.chip_select = 1,
	.mode = 0,
};

//Handler for CMD_PATTERN message
int pattern_handler(struct sk_buff *skb, struct genl_info *info)
{
	int i=0;
	int j=0;
	unsigned char *temp;
	struct task_struct *worker;
	if (!info->attrs[A_MSG]) {
        printk(KERN_ERR "Message empty from %d!!\n",
            info->snd_portid);
        return -EINVAL;
    }

    temp = (unsigned char*)nla_data(info->attrs[A_MSG]);
    printk("INFO: PATTERN message received\n");
    for(i=0;i<10;i++)
    {
    	for(j=0;j<8;j++)
    	{
    		spi_dev->frames[i][j] = *(temp + i * 8 + j);
    	}	
    }

    //For Async behaviour.
    //Creating thread for displaying the animation.
    worker = kthread_run(&spi_render_frame, NULL,"kthread_spi_render");
    return 0;
}

//Handler for CMD_PATTERN message
int config_handler(struct sk_buff *skb, struct genl_info *info)
{
	unsigned char *config;
	int cs_pin;
	int arr[2];
	if (!info->attrs[A_MSG]) {
        printk(KERN_ERR "Message empty from %d!!\n",
            info->snd_portid);
        return -EINVAL;
    }

    config = (unsigned char*)nla_data(info->attrs[A_MSG]);
    printk("INFO: CONFIG message received: %s \n", config);

    //Trigger pin
    arr[0] = (int)config[1];
    //Echo pin
    arr[1] = (int)config[2];
    //Config HCSR gpio pins
    hcsr_pin_config(arr);
    //Chip select pin
    cs_pin = (int)config[0];

    if(cs_pin<0 || cs_pin>19){
		printk("ERROR: Invalid CS pin!\n");
		return -EINVAL;
	}
	if(spi_dev->cs_pin_shield != -1 && spi_dev->cs_pin_shield != cs_pin)
	{	
		//freeing previously configured CS pin(if any)
		free_pin(spi_dev->cs_pin_shield);
	}

	//GPIO config for SPI device
	led_gpio_config(cs_pin);
	//Initiazling LED matrix display
	init_led_display();
	configured=1;
    return 0;
}

//Handler for CMD_MEASURE request
int measurement_handler(struct sk_buff *skb, struct genl_info *info)
{
    struct task_struct *worker;
    printk("INFO: MEASUREMENT message received\n");
    //Creating worker kthread for Async behaviour.
    worker = kthread_run(&perform_measurement, NULL,"kthread_measure");
	return 0;
}

//Netlink OPS
struct genl_ops gnl_ops_custom[] = {
	{
		.cmd = CMD_PATTERN,
    	.flags = 0,
    	.policy = genl_policy,
		.doit = pattern_handler,
		.dumpit = NULL,
	},
	{
		.cmd = CMD_PIN_CONFIG,
		.flags = 0,
		.policy = genl_policy,
		.doit = config_handler,
		.dumpit = NULL,
	},
	{
		.cmd = CMD_MEASURE,
		.flags = 0,
		.policy = genl_policy,
		.doit = measurement_handler,
		.dumpit = NULL,
	}
};

//Generic NETLINK family definition
static struct genl_family gnl_family = {
    .id = GENL_ID_GENERATE,
    .hdrsize = 0,
    .name = GENL_FAMILY_NAME,
    .version = 1,
    .maxattr = A_MAX,
    .ops = gnl_ops_custom,
    .n_ops = ARRAY_SIZE(gnl_ops_custom),
    .mcgrps = genl_groups,
    .n_mcgrps = ARRAY_SIZE(genl_groups),
};

static int spi_probe(struct spi_device *spi)
{
	struct device *dev;
	spi_dev = kzalloc(sizeof(*spi_dev), GFP_KERNEL);

	spi_dev->spi = spi;
	spi_dev->cs_pin_shield = -1;
	spi_dev->cs_pin_gpio = -1;

    dev = device_create(spi_led_class, &spi->dev, MKDEV(MAJOR_NUMBER, 0), spi_dev, DEVICE_NAME);

    if(dev == NULL)
    {
		printk("ERROR: SPI device creation failed\n");
		kfree(spi_dev);
		return -1;
	}
	return 0;
}

static int spi_remove(struct spi_device *spi)
{	
	device_destroy(spi_led_class, MKDEV(MAJOR_NUMBER, 0));
	kfree(spi_dev);
	printk("INFO: SPI LED device removed\n");
	return 0;
}

static struct spi_driver spi_led_matrix = {
	.driver = {
		.name =         DRIVER_NAME,
		.owner =        THIS_MODULE,
	},
	.probe =        spi_probe,
	.remove =       spi_remove,
};

static int __init genl_init(void)
{
    int status;
    struct spi_device *spi_core_dev;
	struct spi_master *spim;

	//Netlink init
	printk("INFO: Initializing generic netlink family: %s \n",GENL_FAMILY_NAME);
    status = genl_register_family(&gnl_family);
    if(status)
        goto failure;

    //HCSR init
    printk("INFO: Initializing HCSR device\n");
    hcsr_struct = (struct hcsr_dev*)kmalloc(sizeof(struct hcsr_dev), GFP_KERNEL);
    memset(hcsr_struct, 0, sizeof(struct hcsr_dev));
    hcsr_struct->trigger_pin_gpio=-1;
    hcsr_struct->trigger_pin_shield=-1;
    hcsr_struct->echo_pin_gpio=-1;
    hcsr_struct->echo_pin_shield=-1;
    hcsr_struct->irq_line=-1;

    //SPI init
    printk("INFO: Initializing SPI device\n");
	spi_led_class = class_create(THIS_MODULE, CLASS_NAME);
	spim = spi_busnum_to_master(spi_led_info.bus_num);
	spi_core_dev = spi_new_device(spim, &spi_led_info);
	spi_core_dev->bits_per_word=8;
	status = spi_setup(spi_core_dev);
	status = spi_register_driver(&spi_led_matrix);
	if(status < 0)
	{
		printk("ERROR: LED device registraion failed\n");
		class_destroy(spi_led_class);
		return -1;
	}
	printk("INFO: SPI device registered!\n");
    return 0;

	failure:
	    printk("ERROR: while genl_register_family \n");
	    return -EINVAL;
}
module_init(genl_init);

static void genl_exit(void)
{
    unsigned char i=0x00;

    //netlink exit
    genl_unregister_family(&gnl_family);
	printk("INFO: generic netlink family unregistered\n");
    
    //resetting display if configured
    if(configured==1)
    {
    	for(i=0x01; i < 0x09; i++)
		{
		send_to_spi(i, 0x00);
		}
    }
    //SPI exit
    free_gpio();
    spi_unregister_device(spi_dev->spi);
    spi_unregister_driver(&spi_led_matrix);
	class_destroy(spi_led_class);
	printk("INFO: SPI device driver unregistered\n");

	//HCSR exit
	if(hcsr_struct->irq_line!=-1)
	{
		free_irq(hcsr_struct->irq_line,NULL);
	}
	free_echo(hcsr_struct->echo_pin_shield);
	free_pin(hcsr_struct->trigger_pin_shield);
	kfree(hcsr_struct);
	printk("INFO: all gpios and irq freed.\n");
	printk("INFO: HCSR device removed\n");
}
module_exit(genl_exit);

MODULE_AUTHOR("Sahil Patil <spatil23@asu.edu>");
MODULE_LICENSE("GPL");

//free_SPI_gpio
void free_gpio()
{
	//SPI MOSI
	//Shield pin 11
	gpio_free(24);
	gpio_free(44);
	gpio_free(72);

	//SPI MISO
	//Shield pin 12
	gpio_free(42);

	//SPI SCK
	//Shield pin 13
	gpio_free(30);
	gpio_free(46);
	
	//SPI CS
	free_pin(spi_dev->cs_pin_shield);
	return;
}

//configure LED gpio pins
int led_gpio_config(int cs_pin)
{
	int cs;
	free_gpio();

	//SPI MOSI
	gpio_request_one(24, GPIOF_DIR_OUT, "MOSI_SHIFT");
	gpio_request_one(44, GPIOF_DIR_OUT, "MOSI_MUX1");
	gpio_request(72, "MOSI_MUX2");

	//SPI MISO
	gpio_request_one(42, GPIOF_DIR_IN, "MISO_SHIFT");

	//SPI SCK
	gpio_request_one(30, GPIOF_DIR_OUT, "SCK_SHIFT");
	gpio_request_one(46, GPIOF_DIR_OUT, "SPI_SCK");
	
	//SPI CS
	cs = check_and_set_trigger(cs_pin);

	if(cs==-1)
	{	
		printk("ERROR: ChipSelect pin configuration failed!\n");		
		return -EINVAL;
	}
	else
	{
		//if no errors are encountered, setting trigger pin value to a variable in per-device struct
		spi_dev->cs_pin_shield = cs_pin;
		spi_dev->cs_pin_gpio = cs;
	}
	
	//SPI MOSI
	gpio_set_value_cansleep(24, 0);
	gpio_set_value_cansleep(44, 1);
	gpio_set_value_cansleep(72, 0);
	//SPI MISO
	gpio_set_value_cansleep(42, 0);
	//SPI SCK
	gpio_set_value_cansleep(30, 0);
	gpio_set_value_cansleep(46, 1);

	printk("INFO: Chip select pin is %d\n",cs_pin);
	printk("INFO: SPI pin configuration done!\n");
	return 0;
}

//Initialize LED display
static int init_led_display()
{
	unsigned char i=0x00;
	//Setting config register values for initializing led display	
	//Display test
	send_to_spi(0x0F, 0x01);
	send_to_spi(0x0F, 0x00);
	
	//Decode mode
	send_to_spi(0x09, 0x00);
	
	//Intensity
	send_to_spi(0x0A, 0x0F);
	
	//Scan Limit
	send_to_spi(0x0B, 0x07);
	
	//Shutdown register
	send_to_spi(0x0C, 0x01);

	//Set every pixel to zero
	for(i=0x01; i < 0x09; i++)
	{
		send_to_spi(i, 0x00);
	}
	printk("INFO: LED matrix initialized!\n");
	return 0;
}

//to set led registers
static void send_to_spi(unsigned char ch1, unsigned char ch2)
{
    int ret=0;
    ch_tx[0] = ch1;
    ch_tx[1] = ch2;
 	gpio_set_value_cansleep(spi_dev->cs_pin_gpio, 0);	
	spi_message_init(&msg);
	spi_message_add_tail(&transfer, &msg);
	ret = spi_sync(spi_dev->spi, &msg);
	gpio_set_value_cansleep(spi_dev->cs_pin_gpio, 1);
	return;
}

//to render animation
int spi_render_frame(void* data)
{
	int i=0;
	for(i=0;i<10;i++)
	{
		send_to_spi(0x01, spi_dev->frames[i][0]);
		send_to_spi(0x02, spi_dev->frames[i][1]);
		send_to_spi(0x03, spi_dev->frames[i][2]);
		send_to_spi(0x04, spi_dev->frames[i][3]);
		send_to_spi(0x05, spi_dev->frames[i][4]);
		send_to_spi(0x06, spi_dev->frames[i][5]);
		send_to_spi(0x07, spi_dev->frames[i][6]);
		send_to_spi(0x08, spi_dev->frames[i][7]);
		msleep(300);
	}
	return 0;
}

//IRQ handler
static irq_handler_t custom_irq_handler(unsigned int irq, void *device_data)
{
	//Checking echo pin signal value
	if(gpio_get_value(hcsr_struct->echo_pin_gpio))
	{
		hcsr_struct->begin = native_read_tsc();
	}
	else
	{
		hcsr_struct->end = native_read_tsc();
	}
	return (irq_handler_t) IRQ_HANDLED;
}


int hcsr_pin_config(int arr[])
{
	int t=-1;
	int e=-1;
	printk("INFO: IOCTL operation: CONFIG_PINS\n");
	printk("INFO: INPUT: Trigger pin: %d\n",arr[0]);
	printk("INFO: INPUT: Echo pin: %d\n",arr[1]);

	if(arr[0]<0 || arr[1]<0 || arr[0]>19 || arr[1]>19){
		printk("ERROR: Invalid pin numbers!\n");
		return -EINVAL;
	}
	//Checking if input trigger pin matches with previously configured trigger pin 
	if(hcsr_struct->trigger_pin_shield == arr[0])
	{	
		t = hcsr_struct->trigger_pin_gpio;
	}
	else
	{	
		//else freeing previously configured trigger pin(if any)
		free_pin(hcsr_struct->trigger_pin_shield);
		//checking and configuring the trigger pin with proper pin multiplexing
		t = check_and_set_trigger(arr[0]);
	}
	if(t==-1)
	{	
		printk("ERROR: Trigger pin configuration failed!\n");		
		return -EINVAL;
	}
	else
	{
		//if no errors are encountered, setting trigger pin value to a variable in per-device struct
		hcsr_struct->trigger_pin_shield = arr[0];
		hcsr_struct->trigger_pin_gpio = t;
	}
	//ECHO PIN CONFIG
	//Checking if input echo pin matches with previously configured echo pin 
	if(hcsr_struct->echo_pin_shield == arr[1])
	{	
		e = hcsr_struct->echo_pin_gpio;
	}
	else
	{
		//else freeing previously configured echo pin(if any)
		free_echo(hcsr_struct->echo_pin_shield);
		//checking and configuring the echo pin with proper pin multiplexing
		e = check_and_set_echo(arr[1]);
	}
	if(e==-1)
	{	
		//if error is encountered while configuring echo pin, then freeing the trigger pin
		free_pin(t);	
		printk("ERROR: Echo pin configuration failed!\n");		
		return -EINVAL;
	}
	else
	{
		//else, setting per-device variables corresponding to echo pins
		hcsr_struct->echo_pin_shield = arr[1];
		hcsr_struct->echo_pin_gpio = e;
	}
    return 0;
}

int perform_measurement(void *data)
{
    void *hdr;
    int res, flags=GFP_ATOMIC;
    char msg[GENL_MAX_MSG_LENGTH];
    struct sk_buff* skb_send;
	int i=0;
	int m=5; //Number of samples
	int sampling_period=100;
	unsigned long long int sum=0;
	unsigned long long int largest_value=0;
	unsigned long long int smallest_value=ULLONG_MAX;
	unsigned long long int t=0;
	if(hcsr_struct->trigger_pin_gpio==-1 || hcsr_struct->echo_pin_gpio==-1)
	{
		printk("ERROR: Trigger/Echo pin not configured!\n");
		return -1;
	}
	for (i=0;i<m+2;i++)
	{
		//Setting trigger pin to 1 for triggering the HCSR sensor for getting a sample 
		gpio_set_value_cansleep(hcsr_struct->trigger_pin_gpio,1);
		udelay(12);
		gpio_set_value_cansleep(hcsr_struct->trigger_pin_gpio,0);
		//Sleeping for sampling period
		msleep(sampling_period);
		//Calculating the time difference between RISING and FALLING edges
	 	t = hcsr_struct->end - hcsr_struct->begin;
	 	t=div_u64(t,400); //to get time in micro seconds
	 	t=div_u64(t,58); //to get distance in cm
	 	sum=sum +t;
	 	//Calulating the two outliers in the measurement
	 	if(t > largest_value)
	 	{
	 		largest_value=t;
	 	}
	 	if(t < smallest_value)
	 	{
	 		smallest_value=t;
		}
	}	
 	//Subtracting the two outliers from sum
	sum = sum - largest_value;
	sum = sum - smallest_value;
	//Taking average of 5 samples
	sum = div_u64(sum,m);
	printk("INFO: Distance is %llu cm\n",sum);

	//Broadcasting distance value via netlink socket message 
	skb_send = genlmsg_new(NLMSG_DEFAULT_SIZE, flags);

    if (!skb_send) {
        printk(KERN_ERR "%d: OOM!!", __LINE__);
        return -1;
    }

    hdr = genlmsg_put(skb_send, 0, 0, &gnl_family, flags, CMD_MEASURE);
    if (!hdr) {
        printk(KERN_ERR "%d: Unknown err !", __LINE__);
        goto nlmsg_fail;
    }

    snprintf(msg, GENL_MAX_MSG_LENGTH, "%lld", sum);

    res = nla_put_string(skb_send, A_MSG, msg);
    if (res) {
        printk(KERN_ERR "%d: err %d ", __LINE__, res);
        goto nlmsg_fail;
    }

    genlmsg_end(skb_send, hdr);
    genlmsg_multicast(&gnl_family, skb_send, 0, GENL_GROUP, flags);
    return 0;

	nlmsg_fail:
	    genlmsg_cancel(skb_send, hdr);
	    nlmsg_free(skb_send);
	    return -1;
	return sum;
}

void free_pin(int t)
{
	//Getting the pins required to be freed based on given shield pin
	int linux_gpio=-1;
	int level_shifter=-1;
	int mux_1=-1;
	int mux_2=-1;
	switch(t)
	{
		case 0:
		    linux_gpio = 11;
		    level_shifter = 32;
		    break;
		case 1:
		    linux_gpio = 12;
		    level_shifter = 28;
		    mux_1 = 45;
		    break;
		case 2:
		    linux_gpio = 13;
		    level_shifter = 34;
		    mux_1 = 77;
		    break;
		case 3:
		    linux_gpio = 14;
		    level_shifter = 16;
		    mux_1 = 76;
		    mux_2 = 64;
		    break;
		case 4:
		    linux_gpio = 6;
		    level_shifter = 36;
		    break;
		case 5:
		    linux_gpio = 0;
		    level_shifter = 18;
		    mux_1 = 66;
		    break;
		case 6:
		    linux_gpio = 1;
		    level_shifter = 20;
		    mux_1 = 68;
		    break;
		case 7:
		    linux_gpio = 38;
		    break;
		case 8:
		    linux_gpio = 40;
		    break;
		case 9:
		    linux_gpio = 4;
		    level_shifter = 22;
		    mux_1 = 70;
		    break;
		case 10:
		    linux_gpio = 10;
		    level_shifter = 26;
		    mux_1 = 74;
		    break;
		case 11:
		    linux_gpio = 5;
		    level_shifter = 24;
		    mux_1 = 44;
		    mux_2 = 72;
		    break;
		case 12:
		    linux_gpio = 15;
		    level_shifter = 42;
		    break;
		case 13:
		    linux_gpio = 7;
		    level_shifter = 30;
		    mux_1 = 46;
		case 14:
		    linux_gpio = 48;
		    break;
		case 15:
		    linux_gpio = 50;
		    break;
		case 16:
		    linux_gpio = 52;
		    break;
		case 17:
		    linux_gpio = 54;
		    break;
		case 18:
		    linux_gpio = 56;
		    mux_1 = 60;
		    mux_2 = 78;
		    break;
		case 19:
		    linux_gpio = 58;
		    mux_1 = 60;
		    mux_2 = 79;
		    break;
		default:
		    return;
		    break;
	}
	//freeing pins which were configured 
	if(linux_gpio!=-1)
	{
		gpio_free(linux_gpio);
	}
	if(level_shifter!=-1)
	{
		gpio_free(level_shifter);
	}
	if(mux_1!=-1)
	{
		gpio_free(mux_1);
	}
	if(mux_2!=-1)
	{
		gpio_free(mux_2);
	}
}

int check_and_set_trigger(int t)
{
	//Initializing gpio pin values
	int linux_gpio=-1;
	int level_shifter=-1;
	int mux_1=-1;
	int mux_2=-1;
	//Depending on user input, configuring the necessary pins accordingly
	switch(t)
	{
		case 0:
		    linux_gpio = 11;
		    level_shifter = 32;
		    break;
		case 1:
		    linux_gpio = 12;
		    level_shifter = 28;
		    mux_1 = 45;
		    break;
		case 2:
		    linux_gpio = 13;
		    level_shifter = 34;
		    mux_1 = 77;
		    break;
		case 3:
		    linux_gpio = 14;
		    level_shifter = 16;
		    mux_1 = 76;
		    mux_2 = 64;
		    break;
		case 4:
		    linux_gpio = 6;
		    level_shifter = 36;
		    break;
		case 5:
		    linux_gpio = 0;
		    level_shifter = 18;
		    mux_1 = 66;
		    break;
		case 6:
		    linux_gpio = 1;
		    level_shifter = 20;
		    mux_1 = 68;
		    break;
		case 7:
		    linux_gpio = 38;
		    break;
		case 8:
		    linux_gpio = 40;
		    break;
		case 9:
		    linux_gpio = 4;
		    level_shifter = 22;
		    mux_1 = 70;
		    break;
		case 10:
		    linux_gpio = 10;
		    level_shifter = 26;
		    mux_1 = 74;
		    break;
		case 11:
		    linux_gpio = 5;
		    level_shifter = 24;
		    mux_1 = 44;
		    mux_2 = 72;
		    break;
		case 12:
		    linux_gpio = 15;
		    level_shifter = 42;
		    break;
		case 13:
		    linux_gpio = 7;
		    level_shifter = 30;
		    mux_1 = 46;
		case 14:
		    linux_gpio = 48;
		    break;
		case 15:
		    linux_gpio = 50;
		    break;
		case 16:
		    linux_gpio = 52;
		    break;
		case 17:
		    linux_gpio = 54;
		    break;
		case 18:
		    linux_gpio = 56;
		    mux_1 = 60;
		    mux_2 = 78;
		    break;
		case 19:
		    linux_gpio = 58;
		    mux_1 = 60;
		    mux_2 = 79;
		    break;
		default:
		    printk("ERROR: Invalid pin selected for trigger\n");
		    return -1;
	}
	//Requesting all the pins required
	//If any pin configuration fails then freeing all the previously requested pins
	if(gpio_request(linux_gpio, "gpio_out") != 0 )
	{
		printk("ERROR: linux_gpio error!\n");
		return -1;
	}
	if(level_shifter!=-1)
	{
		if( gpio_request(level_shifter, "dir_out") != 0 )
		{
			printk("ERROR: level_shifter error!\n");
			gpio_free(linux_gpio);
			return -1;
		}
	}
	if(mux_1!=-1)
	{
		if( gpio_request(mux_1, "pin_mux_1") != 0 )
		{
			printk("ERROR: pin_mux_1 error!\n");
			gpio_free(linux_gpio);
			if(level_shifter!=-1)
			{
				gpio_free(level_shifter);
			}
			return -1;
		}
	}
	if(mux_2!=-1)
	{
		if( gpio_request(mux_2, "pin_mux_2") != 0 )
		{
			printk("ERROR: pin_mux_2 error!\n");
			gpio_free(linux_gpio);
			if(level_shifter!=-1)
			{
				gpio_free(level_shifter);
			}
			gpio_free(mux_1);
			return -1;
		}
	}
	//Setting values to all requested pins as required
	if(level_shifter!=-1)
	{
		gpio_direction_output(level_shifter, 0);
	}
	if(mux_1!=-1)
	{
		if(mux_1==60)
		{
			gpio_direction_output(mux_1, 1);
		}
		else
		{
			if(mux_1>63)
			{
				gpio_set_value_cansleep(mux_1, 0);
			}
			else
			{
				gpio_direction_output(mux_1, 0);
			}
		}
	}
	if(mux_2!=-1)
	{
		if(mux_2==78 || mux_2==79)
		{
			gpio_set_value_cansleep(mux_2, 1);
		}
		else
		{
			if(mux_2>63)
			{
				gpio_set_value_cansleep(mux_2, 0);
			}
			else
			{
				gpio_direction_output(mux_2, 0);
			}
		}
	}
	gpio_direction_output(linux_gpio,0);
	//returning gpio pin for echo
	return linux_gpio;
}

int check_and_set_echo(int e)
{
	//Initializing gpio pin values
	int linux_gpio=-1;
	int level_shifter=-1;
	int mux_1=-1;
	int mux_2=-1;
	int result=-1;
	int irq_no=-1;
	switch(e)
	{
		case 2:
		    linux_gpio = 61;
		    mux_1 = 77;
		    break;
		case 3:
		    linux_gpio = 62;
		    mux_1 = 76;
		    mux_2 = 64;
		    break;
		case 4:
		    linux_gpio = 6;
		    level_shifter = 36;
		    break;
		case 5:
		    linux_gpio = 0;
		    level_shifter = 18;
		    mux_1 = 66;
		    break;
		case 6:
		    linux_gpio = 1;
		    level_shifter = 20;
		    mux_1 = 68;
		    break;
		case 9:
		    linux_gpio = 4;
		    level_shifter = 22;
		    mux_1 = 70;
		    break;
		case 11:
		    linux_gpio = 5;
		    level_shifter = 24;
		    mux_1 = 44;
		    mux_2 = 72;
		    break;
		case 13:
		    linux_gpio = 7;
		    level_shifter = 30;
		    mux_1 = 46;
		    break;
		case 14:
		    linux_gpio = 48;
		    break;
		case 15:
		    linux_gpio = 50;
		    break;
		case 16:
		    linux_gpio = 52;
		    break;
		case 17:
		    linux_gpio = 54;
		    break;
		case 18:
		    linux_gpio = 56;
		    mux_1 = 60;
		    mux_2 = 78;
		    break;
		case 19:
		    linux_gpio = 58;
		    mux_1 = 60;
		    mux_2 = 79;
		    break;
		default:
			printk("ERROR: Invalid echo pin!\n");
			return -1;
			break;
    }
    //Requesting all necessary pins
    //Freeing all previously configured pins if any pin fails in request call
    if(gpio_request(linux_gpio, "gpio_in") != 0 )
	{
		printk("ERROR: linux_gpio error!\n");
		return -1;
	}
	if(level_shifter!=-1)
	{
		if( gpio_request(level_shifter, "dir_in") != 0 )
		{
			printk("ERROR: level_shifter error!\n");
			gpio_free(linux_gpio);
			return -1;
		}
	}
	if(mux_1!=-1)
	{
		if( gpio_request(mux_1, "pin_mux_1") != 0 )
		{
			printk("ERROR: pin_mux_1 error!\n");
			gpio_free(linux_gpio);
			if(level_shifter!=-1)
			{
				gpio_free(level_shifter);
			}
			return -1;
		}
	}
	if(mux_2!=-1)
	{
		if( gpio_request(mux_2, "pin_mux_2") != 0 )
		{
			printk("ERROR: pin_mux_2 error!\n");
			gpio_free(linux_gpio);
			if(level_shifter!=-1)
			{
				gpio_free(level_shifter);
			}
			gpio_free(mux_1);
			return -1;
		}
	}
	//Setting all the pins to appropriate values
	if(level_shifter!=-1)
	{
		gpio_direction_output(level_shifter, 1);
	}
	if(mux_1!=-1)
	{
		if(mux_1==60)
		{
			gpio_direction_output(mux_1, 1);
		}
		else
		{
			if(mux_1>63)
			{
				gpio_set_value_cansleep(mux_1, 0);
			}
			else
			{
				gpio_direction_output(mux_1, 0);
			}
		}
	}
	if(mux_2!=-1)
	{
		if(mux_2==78 || mux_2==79)
		{
			gpio_set_value_cansleep(mux_2, 1);
		}
		else
		{
			if(mux_2>63)
			{
				gpio_set_value_cansleep(mux_2, 0);
			}
			else
			{
				gpio_direction_output(mux_2, 0);
			}
		}
	}
	gpio_direction_input(linux_gpio);
	//Getting IRQ number based on gpio echo pin
	irq_no = gpio_to_irq(linux_gpio);
	printk("INFO: IRQ number: %d\n",irq_no);
	if(irq_no<0)
	{
		printk("ERROR: IRQ number error\n");
		gpio_free(linux_gpio);
		return -1;
	}

	//If IRQ number matches with previously configured IRQ, return.
	if(hcsr_struct->irq_line==irq_no)
	{
		return linux_gpio;
	}

	//Freeing IRQ if assigned earlier.
	if(hcsr_struct->irq_line!=-1)
	{
		free_irq(hcsr_struct->irq_line,(void *)&hcsr_struct);
	}
	//Requesting IRQ to be configured for both edges (RISING AND FALLING)
	result = request_irq(irq_no,(irq_handler_t) custom_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING , "both_edge",NULL);
	if (result < 0)
	{
		printk("ERROR: in request_irq call\n");
		gpio_free(linux_gpio);
		return -1;
	}
	else
	{
		printk("INFO: IRQ Success!\n");
	}
	//Setting IRQ number in device struct for future reference (needed while freeing IRQ)
	hcsr_struct->irq_line = irq_no;
	return linux_gpio;
}

void free_echo(int e)
{
	//Getting the pins required to be freed based on given shield pin
	int linux_gpio=-1;
	int level_shifter=-1;
	int mux_1=-1;
	int mux_2=-1;
	switch(e)
	{
		case 2:
		    linux_gpio = 61;
		    mux_1 = 77;
		    break;
		case 3:
		    linux_gpio = 62;
		    mux_1 = 76;
		    mux_2 = 64;
		    break;
		case 4:
		    linux_gpio = 6;
		    level_shifter = 36;
		    break;
		case 5:
		    linux_gpio = 0;
		    level_shifter = 18;
		    mux_1 = 66;
		    break;
		case 6:
		    linux_gpio = 1;
		    level_shifter = 20;
		    mux_1 = 68;
		    break;
		case 9:
		    linux_gpio = 4;
		    level_shifter = 22;
		    mux_1 = 70;
		    break;
		case 11:
		    linux_gpio = 5;
		    level_shifter = 24;
		    mux_1 = 44;
		    mux_2 = 72;
		    break;
		case 13:
		    linux_gpio = 7;
		    level_shifter = 30;
		    mux_1 = 46;
		    break;
		case 14:
		    linux_gpio = 48;
		    break;
		case 15:
		    linux_gpio = 50;
		    break;
		case 16:
		    linux_gpio = 52;
		    break;
		case 17:
		    linux_gpio = 54;
		    break;
		case 18:
		    linux_gpio = 56;
		    mux_1 = 60;
		    mux_2 = 78;
		    break;
		case 19:
		    linux_gpio = 58;
		    mux_1 = 60;
		    mux_2 = 79;
		    break;
		default:
		    return;
		    break;
    }
    //Freeing pins which were configured
	if(linux_gpio!=-1)
	{
		gpio_free(linux_gpio);
	}
	if(level_shifter!=-1)
	{
		gpio_free(level_shifter);
	}
	if(mux_1!=-1)
	{
		gpio_free(mux_1);
	}
	if(mux_2!=-1)
	{
		gpio_free(mux_2);
	}
}