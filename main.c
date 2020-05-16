#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <pthread.h>
#include "gen_netlink.h"

#ifndef GRADING
#define MAX7219_CS_PIN 9
#define HCSR04_TRIGGER_PIN 10
#define HCSR04_ECHO_PIN 4
#endif

//Types of animations
enum patterns{
	ANIMATION,
	LEFT,
	RIGHT,
	LEFT_FAST,
	RIGHT_FAST
};

static char message[GENL_MAX_MSG_LENGTH];
static unsigned int groups;
static unsigned char (*pin_numbers)[3];
int prev_distance=-1;
int distance;
struct nl_sock* nlsock_g;
struct nl_cb *cb;

unsigned char anim[10][8] = 
	{{0x00,0x00,0x18,0x3C,0x18,0x00,0x00,0x00},
	{0x00,0x18,0x3C,0x7E,0x3C,0x18,0x00,0x00},
	{0x18,0x3C,0x7E,0xFF,0x7E,0x3C,0x18,0x00},
	{0x00,0x18,0x3C,0x7E,0x3C,0x18,0x00,0x00},
	{0x00,0x00,0x18,0x3C,0x18,0x00,0x00,0x00},
	{0x00,0x00,0x18,0x3C,0x18,0x00,0x00,0x00},
	{0x00,0x18,0x3C,0x7E,0x3C,0x18,0x00,0x00},
	{0x18,0x3C,0x7E,0xFF,0x7E,0x3C,0x18,0x00},
	{0x00,0x18,0x3C,0x7E,0x3C,0x18,0x00,0x00},
	{0x00,0x00,0x18,0x3C,0x18,0x00,0x00,0x00}
};

unsigned char dog_left[10][8] = 
	{{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06}
};

unsigned char dog_right[10][8] = 
	{{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60}
};

unsigned char dog_left_fast[10][8] = 
	{{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06},
	{0x63,0x22,0x24,0x7F,0x83,0x06,0x06,0x06},
	{0x16,0x32,0xA2,0x7F,0x03,0x06,0x06,0x06}
};

unsigned char dog_right_fast[10][8] = 
	{{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60},
	{0xC6,0x44,0x24,0xFE,0xC1,0x60,0x60,0x60},
	{0x68,0x4C,0x45,0xFE,0xC0,0x60,0x60,0x60}
};

static void add_group(char* group)
{
	unsigned int grp = strtoul(group, NULL, 10);
	if (grp > GENL_GROUP_MAX-1) {
		fprintf(stderr, "Invalid group number %u. Values allowed 0:%u\n",
			grp, GENL_GROUP_MAX-1);
		exit(EXIT_FAILURE);
	}
	groups |= 1 << (grp);
}

//funtion for CMD_PIN_CONFIG message
static int send_config_to_kernel(struct nl_sock *sock)
{
	struct nl_msg* msg;
	int family_id, err = 0;

	family_id = genl_ctrl_resolve(sock, GENL_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		exit(EXIT_FAILURE);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message\n");
		exit(EXIT_FAILURE);
	}

	if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, CMD_PIN_CONFIG, 0)) {
		fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	}

	err = nla_put_string(msg, A_MSG, (char*)pin_numbers);
	if (err) {
		fprintf(stderr, "failed to put nl string!\n");
		goto out;
	}

	err = nl_send_auto(sock, msg);
	if (err < 0) {
		fprintf(stderr, "failed to send nl message!\n");
	}

out:
	nlmsg_free(msg);
	return err;
}

//funtion for CMD_PATTERN message
static int send_pattern_to_kernel(struct nl_sock *sock,int a)
{
	struct nl_msg* msg;
	int family_id, err = 0;

	family_id = genl_ctrl_resolve(sock, GENL_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		exit(EXIT_FAILURE);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message\n");
		exit(EXIT_FAILURE);
	}

	if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, CMD_PATTERN, 0)) {
		fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	}
	switch(a)
	{
		case ANIMATION:
			err = nla_put(msg, A_MSG,sizeof(anim), &anim);
		break;
		case LEFT:
			err = nla_put(msg, A_MSG,sizeof(dog_left), &dog_left);
		break;
		case LEFT_FAST:
			err = nla_put(msg, A_MSG,sizeof(dog_left_fast), &dog_left_fast);
		break;
		case RIGHT:
			err = nla_put(msg, A_MSG,sizeof(dog_right), &dog_right);
		break;
		case RIGHT_FAST:
			err = nla_put(msg, A_MSG,sizeof(dog_right_fast), &dog_right_fast);
		break;
		default:
			printf("INVALID ANIMATION\n");
			return -1;
		break;
	}
	if (err) {
		fprintf(stderr, "failed to put nl string!\n");
		goto out;
	}

	err = nl_send_auto(sock, msg);
	if (err < 0) {
		fprintf(stderr, "failed to send nl message!\n");
	}

out:
	nlmsg_free(msg);
	return err;
}

//funtion for CMD_MEASURE message
static int request_measurement_from_kernel(struct nl_sock *sock)
{
	struct nl_msg* msg;
	int family_id, err = 0;

	family_id = genl_ctrl_resolve(sock, GENL_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		exit(EXIT_FAILURE);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message\n");
		exit(EXIT_FAILURE);
	}

	if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, CMD_MEASURE, 0)) {
		fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	}

	err = nla_put_string(msg, A_MSG, (char*)message);
	if (err) {
		fprintf(stderr, "failed to put nl string!\n");
		goto out;
	}

	err = nl_send_auto(sock, msg);
	if (err < 0) {
		fprintf(stderr, "failed to send nl message!\n");
	}

out:
	nlmsg_free(msg);
	return err;
}

static int skip_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static int get_distance(struct nl_msg *msg, void* arg)
{
	struct nlattr *attr[A_MAX+1];
	genlmsg_parse(nlmsg_hdr(msg), 0, attr, 
			A_MAX, genl_policy);

	if (!attr[A_MSG]) {
		fprintf(stdout, "Kernel sent empty message!!\n");
		return NL_OK;
	}

	distance = atoi(nla_get_string(attr[A_MSG]));
	fprintf(stdout, "Distance : %d cm \n", distance);
	return NL_OK;
}

static void prep_nl_sock(struct nl_sock** nlsock)
{
	int family_id, grp_id;
	
	*nlsock = nl_socket_alloc();
	if(!*nlsock) {
		fprintf(stderr, "Unable to alloc nl socket!\n");
		exit(EXIT_FAILURE);
	}

	nl_socket_disable_seq_check(*nlsock);
	nl_socket_disable_auto_ack(*nlsock);

	if (genl_connect(*nlsock)) {
		fprintf(stderr, "Unable to connect to genl!\n");
		goto exit_err;
	}

	family_id = genl_ctrl_resolve(*nlsock, GENL_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		goto exit_err;
	}

	if (!groups)
		return;

	grp_id = genl_ctrl_resolve_grp(*nlsock, GENL_FAMILY_NAME,
			GENL_GROUP_NAME);

	if (grp_id < 0)	{
		fprintf(stderr, "Unable to resolve group name!\n");
        goto exit_err;
	}
	if (nl_socket_add_membership(*nlsock, grp_id)) {
		fprintf(stderr, "Unable to join group!\n");
        goto exit_err;
	}
    return;

	exit_err:
	    nl_socket_free(*nlsock);
	    exit(EXIT_FAILURE);
}

//Thread for measurement requests
void * measurement_requester()
{	
	printf("Starting measurement request thread\n");
	strncpy(message, "Dummy message", GENL_MAX_MSG_LENGTH);
	message[GENL_MAX_MSG_LENGTH - 1] = '\0';
	while(1)
	{	
		printf("M_THREAD: Requesting measurement\n");
		request_measurement_from_kernel(nlsock_g);
		//requesting measurement after every 2 seconds
		sleep(2);
	}	
}


//Thread for animation requests
void * animation_controller()
{
	int diff=0;
	printf("Starting animation request thread\n");
	//initialize display with default animation
	send_pattern_to_kernel(nlsock_g,ANIMATION);
	while(1)
	{
		//check for message from kernel
		nl_recvmsgs(nlsock_g, cb);
		if(prev_distance!=-1)
		{	
			if(distance>prev_distance)
			{
				diff = distance-prev_distance;
			}
			else
			{
				diff = prev_distance-distance;
			}
			if(distance>prev_distance && diff>2)
			{
				prev_distance = distance;
				//if object moves farther, dog goes left
				if(diff < 20)
				{
					printf("Object moved farther, slowly\n");
					//dog walks, less displacement
					send_pattern_to_kernel(nlsock_g,LEFT);
				}
				else
				{	
					printf("Object moved farther, fast\n");
					//if displacement is greater than 10, dog runs faster
					send_pattern_to_kernel(nlsock_g,LEFT_FAST);
				}
			}
			else if(distance<prev_distance && diff>2)
			{
				prev_distance = distance;
				//if object moves closer, dog goes right
				if(diff < 20)
				{
					printf("Object moved closer, slowly\n");
					//dog walks, less displacement
					send_pattern_to_kernel(nlsock_g,RIGHT);
				}
				else
				{
					printf("Object moved closer, fast\n");
					//if displacement is greater than 10, dog runs faster
					send_pattern_to_kernel(nlsock_g,RIGHT_FAST);
				}
			}
			if(diff<=2) 
			{	
				prev_distance = distance;
				printf("Object is NOT moving\n");
				//if object is not moving, display default animation
				send_pattern_to_kernel(nlsock_g,ANIMATION);
			}
		}
		else
		{
			//for first measurement display default animation
			prev_distance = distance;
			send_pattern_to_kernel(nlsock_g,ANIMATION);
		}
		sleep(2);
		//repeat after every two seconds
	}
}

int main(int argc, char** argv)
{
	pthread_t measure,display;

	unsigned char t[3] = {
		MAX7219_CS_PIN,
		HCSR04_TRIGGER_PIN,
		HCSR04_ECHO_PIN
	};
	pin_numbers = &t;
	
	//add group and init sock
	add_group("0");
	printf("Preparing netlink socket\n");
	prep_nl_sock(&nlsock_g);

	//Setup callback funtion
	cb = nl_cb_alloc(NL_CB_DEFAULT);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, skip_seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, get_distance, NULL);

	//Request kernerl for pin config
	printf("Sending pin config request to kernel\n");
	send_config_to_kernel(nlsock_g);
	//Creating two threads
	pthread_create(&display, NULL, animation_controller, NULL);
	pthread_create(&measure, NULL, measurement_requester, NULL);
	
	pthread_join(measure,NULL);
	pthread_join(display,NULL);
	
	nl_cb_put(cb);
    nl_socket_free(nlsock_g);
	return 0;
}
