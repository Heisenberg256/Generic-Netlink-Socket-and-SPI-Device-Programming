#include <linux/netlink.h>

#ifndef __KERNEL__
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#endif

#define GENL_GROUP_NAME	        "genl_group"
#define GENL_FAMILY_NAME        "my_genl_family"
#define GENL_MAX_MSG_LENGTH     256

//Netlink attributes
enum {
    A_UNSPEC,
    A_MSG,
    __A_MAX,
};
#define A_MAX (__A_MAX - 1)

//Netlink policy
static struct nla_policy genl_policy[A_MAX + 1] = {
    [A_MSG] = 
    { 
        .type = NLA_U8
    },
};

//Netlink commands
enum {
    CMD_UNSPEC,
    CMD_PATTERN,
    CMD_PIN_CONFIG,
    CMD_MEASURE,
    __CMD_MAX,
};
#define CMD_MAX (__CMD_MAX - 1)

//Netlink groups
enum genl_multicast_groups {
	GENL_GROUP,
};
#define GENL_GROUP_MAX		1