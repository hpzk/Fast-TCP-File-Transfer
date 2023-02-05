/*
 * TCP Veno congestion control
 *
 * This is based on the congestion detection/avoidance scheme described in
 *    C. P. Fu, S. C. Liew.
 *    "TCP Veno: TCP Enhancement for Transmission over Wireless Access Networks."
 *    IEEE Journal on Selected Areas in Communication,
 *    Feb. 2003.
 * 	See http://www.ie.cuhk.edu.hk/fileadmin/staff_upload/soung/Journal/J3.pdf
 */

/*
* TCP AHS (Ali, Hossain, Spencer) is an enhanced version of the TCP Veno congestion controller.
*
*/

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/inet_diag.h>
#include <net/tcp.h>

#define TCP_AHS_INIT_RTT 1000000 /* 1 second */
#define DEFAULT_AHS_WINDOW_SIZE 65000

/* ahs variables */
struct ahs{
	u8	ahs_en;
	u8	if_cong;
	u32		rtt_min;
	u32		rtt;
};

/* initialize ahs variables */
static void tcp_ahs_init(struct sock *sk)
{
	struct ahs *ahs = inet_csk_ca(sk);
	
	// initialize AHS variables (RTT and RTT_MIN are initializes to 1 second)
	ahs->ahs_en = 1;
	ahs->if_cong = 0;
	ahs->rtt_min = TCP_AHS_INIT_RTT;
	ahs->rtt = TCP_AHS_INIT_RTT;
}

/* when packet is acked: 
* because we are choosing a static, large window, we record rtt
* but force a large send window size because we know the network
* is lossy and not congested
*/
static void tcp_ahs_pkts_acked(struct sock *sk, u32 cnt, s32 rtt_us)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct ahs *ahs = inet_csk_ca(sk);

	// if we read a new rtt from the network, put it into the ahs rtt
	if (rtt_us > 0)
		ahs->rtt = rtt_us;
	
	// store the minimum rtt
	ahs->rtt_min = min(ahs->rtt_min, ahs->rtt);
	
	// force the send window size to our default window size
	tp->snd_cwnd = DEFAULT_AHS_WINDOW_SIZE;
}

/*
* force default window size
*/
static u32 tcp_ahs_undo_cwnd(struct sock *sk) {
	return DEFAULT_AHS_WINDOW_SIZE;
}

static void tcp_ahs_state(struct sock *sk, u8 ca_state)
{
	struct ahs *ahs = inet_csk_ca(sk);
	// enable ahs when state function is called
	ahs->ahs_en = 1;
}

/*
* force send window back to default window size upon occurance of any
* collision avoidance event
*/
static void tcp_ahs_cwnd_event(struct sock *sk, enum tcp_ca_event event)
{
	struct tcp_sock *tp = tcp_sk(sk);
	tp->snd_cwnd = DEFAULT_AHS_WINDOW_SIZE;
}

/* instead of relying on veno's dynamic window sizing for congestion avoidance,
* we again force the window to our default window size
*/
static void tcp_ahs_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct ahs *ahs = inet_csk_ca(sk);

	// if ahs is not enabled, default to veno
	if (!ahs->ezs_en) {
		tcp_veno_cong_avoid(sk, ack, acked);
		return;
	}

	tp->snd_cwnd = DEFAULT_AHS_WINDOW_SIZE;
}

/*
* force slow start threshold to default window size
*/
static u32 tcp_ahs_ssthresh(struct sock *sk)
{
	return DEFAULT_AHS_WINDOW_SIZE;
}

static struct tcp_congestion_ops tcp_ahs __read_mostly = {
	.init		= tcp_ahs_init,
	.ssthresh	= tcp_ahs_ssthresh,
	.cong_avoid	= tcp_ahs_cong_avoid,
	.cwnd_event = tcp_ahs_cwnd_event,
	.pkts_acked	= tcp_ahs_pkts_acked,
	.set_state	= tcp_ahs_state,
	.undo_cwnd	= tcp_ahs_undo_cwnd,

	.owner		= THIS_MODULE,
	.name		= "ahs",
};

static int __init tcp_ahs_register(void)
{
	BUILD_BUG_ON(sizeof(struct ahs) > ICSK_CA_PRIV_SIZE);
	tcp_register_congestion_control(&tcp_ahs);
	return 0;
}

static void __exit tcp_ahs_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_ahs);
}

module_init(tcp_honey_register);
module_exit(tcp_honey_unregister);

MODULE_AUTHOR("Ali Afzal, Hossain Pazooki, Spencer McDonough");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP AHS");