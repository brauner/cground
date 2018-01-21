#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>

#include "nl.h"


int netdev_get_mtu(int ifindex)
{
	int answer_len, err, res;
	struct nl_handler nlh;
	struct ifinfomsg *ifi;
	struct nlmsghdr *msg;
	int readmore = 0, recv_len = 0;
	struct nlmsg *answer = NULL, *nlmsg = NULL;

	err = netlink_open(&nlh, NETLINK_ROUTE, 0);
	if (err)
		return err;

	err = -ENOMEM;
	nlmsg = nlmsg_alloc(NLMSG_GOOD_SIZE);
	if (!nlmsg)
		goto out;

	answer = nlmsg_alloc_reserve(NLMSG_GOOD_SIZE);
	if (!answer)
		goto out;

	/* Save the answer buffer length, since it will be overwritten
	 * on the first receive (and we might need to receive more than
	 * once.
	 */
	answer_len = answer->nlmsghdr->nlmsg_len;

	nlmsg->nlmsghdr->nlmsg_flags = NLM_F_REQUEST;
	nlmsg->nlmsghdr->nlmsg_type = RTM_GETLINK;

	ifi = nlmsg_reserve(nlmsg, sizeof(struct ifinfomsg));
	if (!ifi)
		goto out;
	ifi->ifi_family = AF_UNSPEC;
	ifi->ifi_index = 1;

	if (nla_put_u32(nlmsg, IFLA_IF_NETNSID, 3))
		goto out;

	/* Send the request for addresses, which returns all addresses
	 * on all interfaces. */
	err = netlink_send(&nlh, nlmsg);
	if (err < 0)
		goto out;

	do {
		/* Restore the answer buffer length, it might have been
		 * overwritten by a previous receive.
		 */
		answer->nlmsghdr->nlmsg_len = answer_len;

		/* Get the (next) batch of reply messages */
		err = netlink_rcv(&nlh, answer);
		if (err < 0)
			goto out;

		recv_len = err;

		/* Satisfy the typing for the netlink macros */
		msg = answer->nlmsghdr;

		while (NLMSG_OK(msg, recv_len)) {

			/* Stop reading if we see an error message */
			if (msg->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *errmsg =
				    (struct nlmsgerr *)NLMSG_DATA(msg);
				err = errmsg->error;
				goto out;
			}

			/* Stop reading if we see a NLMSG_DONE message */
			if (msg->nlmsg_type == NLMSG_DONE) {
				readmore = 0;
				break;
			}

			ifi = NLMSG_DATA(msg);
			if (ifi->ifi_index == ifindex) {
				struct rtattr *rta = IFLA_RTA(ifi);
				int attr_len =
				    msg->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi));
				res = 0;
				while (RTA_OK(rta, attr_len)) {
					if (rta->rta_type == IFLA_IFNAME) {
						char name[4096] = {0};
						memcpy(name, RTA_DATA(rta), sizeof(name));
						printf("AAAA: %s\n", name);
					}

					/* Found a local address for the
					 * requested interface, return it.
					 */
					if (rta->rta_type == IFLA_MTU) {
						memcpy(&res, RTA_DATA(rta),
						       sizeof(int));
						printf("BBBB: %d\n", res);
						err = res;
						goto out;
					}
					rta = RTA_NEXT(rta, attr_len);
				}
			}

			/* Keep reading more data from the socket if the last
			 * message had the NLF_F_MULTI flag set.
			 */
			readmore = (msg->nlmsg_flags & NLM_F_MULTI);

			/* Look at the next message received in this buffer. */
			msg = NLMSG_NEXT(msg, recv_len);
		}
	} while (readmore);

	/* If we end up here, we didn't find any result, so signal an error. */
	err = -1;

out:
	netlink_close(&nlh);
	nlmsg_free(answer);
	nlmsg_free(nlmsg);
	return err;
}

int main(int argc, char *argv[])
{
	int ret;
	ret = netdev_get_mtu(1);
	if (ret < 0)
		return 1;

	return 0;
}
