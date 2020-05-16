Name: SAHIL PATIL
ASU ID: 1217436155

EOSI assignment 3

Generic Netlink Socket and SPI Device Programming

Steps to use the software:

0. Configure the pin numbers in main.c according to your own setup

1. Make/build/compile the the source code using the given Makefile

	run the following command in the source directory:

	make 

2. To install kernel module

	insmod spi_hcsr_netlink.ko

3. To test driver functions using user program, run
	
	./main

4. After testing remove the kernel module using following command

	rmmod spi_hcsr_netlink.ko
	
5. To clean source directory use:
	
	make clean

Note: 
1. In the main program, I have written the code to test operations using two threads for measurement and display concurrently.
2. I have only allowed configuration of those pins which support both-edge interrupt triggering. Please use only those pins for echo.
3. I have used a simple box animation for showing that the object is idle.
4. When you move the object the dog will walk/run (depending on movement speed) towards left/right (depending on movement - far/close)
5. When object stops moving (or moves negligible distance < 3) the animation is switched to default.
6. Both measurement and pattern display is asynchronous (used kthreads)

SAMPLE OUTPUT:

1. ON STDOUT:

	root@quark:/home/sahil/nl# insmod spi_hcsr_netlink.ko 
	root@quark:/home/sahil/nl# ./main 
	Preparing netlink socket
	Sending pin config request to kernel
	Starting measurement request thread
	M_THREAD: Requesting measurement
	Starting animation request thread
	Distance : 98 cm 
	M_THREAD: Requesting measurement
	Distance : 98 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 98 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 98 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 98 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 21 cm 
	Object moved closer, fast
	M_THREAD: Requesting measurement
	Distance : 37 cm 
	Object moved farther, slowly
	M_THREAD: Requesting measurement
	Distance : 15 cm 
	Object moved closer, fast
	M_THREAD: Requesting measurement
	Distance : 14 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 12 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 27 cm 
	Object moved farther, slowly
	M_THREAD: Requesting measurement
	Distance : 10 cm 
	Object moved closer, slowly
	M_THREAD: Requesting measurement
	Distance : 16 cm 
	Object moved farther, slowly
	M_THREAD: Requesting measurement
	Distance : 23 cm 
	Object moved farther, slowly
	M_THREAD: Requesting measurement
	Distance : 30 cm 
	Object moved farther, slowly
	M_THREAD: Requesting measurement
	Distance : 40 cm 
	Object moved farther, slowly
	M_THREAD: Requesting measurement
	Distance : 37 cm 
	Object moved closer, slowly
	M_THREAD: Requesting measurement
	Distance : 15 cm 
	Object moved closer, fast
	M_THREAD: Requesting measurement
	Distance : 14 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 20 cm 
	Object moved farther, slowly
	M_THREAD: Requesting measurement
	Distance : 25 cm 
	Object moved farther, slowly
	M_THREAD: Requesting measurement
	Distance : 97 cm 
	Object moved farther, fast
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving
	M_THREAD: Requesting measurement
	Distance : 99 cm 
	Object is NOT moving



2. On dmesg:

	root@quark:/home/sahil# dmesg
	[  289.235890] INFO: Initializing generic netlink family: my_genl_family 
	[  289.246549] INFO: Initializing HCSR device
	[  289.252873] INFO: Initializing SPI device
	[  289.272523] INFO: SPI device registered!
	[  292.191724] INFO: CONFIG message received: 	
	[  292.191724]  
	[  292.197703] INFO: IOCTL operation: CONFIG_PINS
	[  292.202303] INFO: INPUT: Trigger pin: 10
	[  292.206262] INFO: INPUT: Echo pin: 4
	[  292.213720] INFO: IRQ number: 60
	[  292.217062] INFO: IRQ Success!
	[  292.231631] INFO: Chip select pin is 9
	[  292.235421] INFO: SPI pin configuration done!
	[  292.245356] INFO: LED matrix initialized!
	[  292.256200] INFO: MEASUREMENT message received
	[  292.263901] INFO: PATTERN message received
	[  293.030150] INFO: Distance is 98 cm
	[  293.034699] INFO: PATTERN message received
	[  294.269103] INFO: MEASUREMENT message received
	[  295.040141] INFO: Distance is 98 cm
	[  295.045463] INFO: PATTERN message received
	[  296.275484] INFO: MEASUREMENT message received
	[  297.050190] INFO: Distance is 98 cm
	[  297.054604] INFO: PATTERN message received
	[  298.282970] INFO: MEASUREMENT message received
	[  299.050143] INFO: Distance is 98 cm
	[  299.065518] INFO: PATTERN message received
	[  300.289233] INFO: MEASUREMENT message received
	[  301.060146] INFO: Distance is 99 cm
	[  301.073054] INFO: PATTERN message received
	[  302.295626] INFO: MEASUREMENT message received
	[  303.070141] INFO: Distance is 99 cm
	[  303.079155] INFO: PATTERN message received
	[  304.302025] INFO: MEASUREMENT message received
	[  305.070147] INFO: Distance is 98 cm
	[  305.087480] INFO: PATTERN message received
	[  306.308298] INFO: MEASUREMENT message received
	[  307.080147] INFO: Distance is 99 cm
	[  307.093725] INFO: PATTERN message received
	[  308.314678] INFO: MEASUREMENT message received
	[  309.090148] INFO: Distance is 99 cm
	[  309.099816] INFO: PATTERN message received
	[  310.323668] INFO: MEASUREMENT message received
	[  311.100144] INFO: Distance is 21 cm
	[  311.108141] INFO: PATTERN message received
	[  312.336613] INFO: MEASUREMENT message received
	[  313.110135] INFO: Distance is 37 cm
	[  313.116505] INFO: PATTERN message received
	[  314.343014] INFO: MEASUREMENT message received
	[  315.110146] INFO: Distance is 15 cm
	[  315.124870] INFO: PATTERN message received
	[  316.349287] INFO: MEASUREMENT message received
	[  317.120142] INFO: Distance is 14 cm
	[  317.134433] INFO: PATTERN message received
	[  318.355665] INFO: MEASUREMENT message received
	[  319.130138] INFO: Distance is 12 cm
	[  319.140662] INFO: PATTERN message received
	[  320.362056] INFO: MEASUREMENT message received
	[  321.130143] INFO: Distance is 27 cm
	[  321.148875] INFO: PATTERN message received
	[  322.368339] INFO: MEASUREMENT message received
	[  323.140141] INFO: Distance is 10 cm
	[  323.155113] INFO: PATTERN message received
	[  324.374704] INFO: MEASUREMENT message received
	[  325.150143] INFO: Distance is 16 cm
	[  325.162806] INFO: PATTERN message received
	[  326.383743] INFO: MEASUREMENT message received
	[  327.150142] INFO: Distance is 23 cm
	[  327.168929] INFO: PATTERN message received
	[  328.390087] INFO: MEASUREMENT message received
	[  329.160142] INFO: Distance is 30 cm
	[  329.175188] INFO: PATTERN message received
	[  330.396359] INFO: MEASUREMENT message received
	[  331.170164] INFO: Distance is 40 cm
	[  331.182842] INFO: PATTERN message received
	[  332.402757] INFO: MEASUREMENT message received
	[  333.170144] INFO: Distance is 37 cm
	[  333.188983] INFO: PATTERN message received
	[  334.409032] INFO: MEASUREMENT message received
	[  335.180141] INFO: Distance is 15 cm
	[  335.195234] INFO: PATTERN message received
	[  336.415401] INFO: MEASUREMENT message received
	[  337.190143] INFO: Distance is 14 cm
	[  337.202814] INFO: PATTERN message received
	[  338.422892] INFO: MEASUREMENT message received
	[  339.190142] INFO: Distance is 20 cm
	[  339.208941] INFO: PATTERN message received
	[  340.429162] INFO: MEASUREMENT message received
	[  341.200143] INFO: Distance is 25 cm
	[  341.215192] INFO: PATTERN message received
	[  342.435534] INFO: MEASUREMENT message received
	[  343.210142] INFO: Distance is 97 cm
	[  343.222768] INFO: PATTERN message received
	[  344.441924] INFO: MEASUREMENT message received
	[  345.210145] INFO: Distance is 99 cm
	[  345.228885] INFO: PATTERN message received
	[  346.448200] INFO: MEASUREMENT message received
	[  347.220142] INFO: Distance is 99 cm
	[  347.235967] INFO: PATTERN message received
	[  348.454586] INFO: MEASUREMENT message received
	[  349.230143] INFO: Distance is 99 cm
	[  349.242203] INFO: PATTERN message received
	[  350.462279] INFO: MEASUREMENT message received
	[  351.230145] INFO: Distance is 99 cm
	[  351.248316] INFO: PATTERN message received
	[  352.468553] INFO: MEASUREMENT message received
	[  353.240142] INFO: Distance is 99 cm
	[  353.254562] INFO: PATTERN message received
	[  356.727642] INFO: generic netlink family unregistered
	[  356.739183] INFO: SPI LED device removed
	[  356.754316] INFO: SPI device driver unregistered
	[  356.772522] INFO: all gpios and irq freed.
	[  356.776664] INFO: HCSR device removed