
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <linux/if_ether.h>
#include <linux/if_packet.h>



//#include "hexdump.h"
#include "err.h"
#include "mac.h"

#define BIGGEST_FRAME 4096

typedef union {
	
	struct ethhdr eth;
	char buffer[BIGGEST_FRAME];

} framedata_t;

typedef struct frame {
	/* How long is the frame */
	int length; /* don't use size_t read() returns -1 */

	/* What time to send at */
	struct timeval timetogo;

	struct frame *next;

	/* The frame, this must be last for memory alloc! */
	framedata_t data;
} frame_t;


inline void schedule_frame(frame_t *f);
inline void queue_frame(frame_t *f);
inline void dole_frame(int outfd);
inline void translate_frame(frame_t *f);
inline int ttn_frame(void);
inline int count_frames(void);


int main(void) {
	int input, output; 

	/* stevens 793 */
	if ( 0 > (input = socket (PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) )
		err("socket()");
	if ( 0 > (output = socket (PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) )
		err("socket()");

	if ( 0 > fcntl(input, F_SETFL, fcntl(F_GETFL, 0) | O_NONBLOCK) )
		err("fcntl()");
	
	/* SIOCGIFINDEX */
	/* see also packet/af_packet.c */

	struct sockaddr_ll in_nic, out_nic;
	memset(&in_nic, 0, sizeof(in_nic));
	memset(&out_nic, 0, sizeof(out_nic));

	in_nic.sll_family = out_nic.sll_family = AF_PACKET;
	in_nic.sll_protocol = out_nic.sll_protocol = htons(ETH_P_ALL);
	in_nic.sll_ifindex = if_nametoindex("nic2");
	out_nic.sll_ifindex = if_nametoindex("nic1");
	/* nic.sll_hatype .. not sure what values are .. leave as 0 */
	/* nic.sll_pkttype = */
	/* nic.sll_halen = ? */
	/* nic.sll_addr = ? */

	if ( 0 > bind(input, (struct sockaddr *)&in_nic, sizeof(in_nic) ) )
		err("bind()");
	if ( 0 > bind(output, (struct sockaddr *)&out_nic, sizeof(out_nic) ) )
		err("bind()");
	
	/* main program part.. */

	frame_t work;
	
	struct pollfd pollinput;
	pollinput.fd = input;
	pollinput.events = POLLIN;

	int mswait;

	while ( 1 ) {
		printf("%40d frames in buffer.\r", count_frames());

		work.length = read(input, work.data.buffer, BIGGEST_FRAME);

		if ( work.length > 0 ) {
			translate_frame(&work);
			schedule_frame(&work);
			queue_frame(&work);

			// freaking woo hoo
		}
		
		dole_frame(output);
		mswait = ttn_frame();

		poll(&pollinput, 1, mswait);

	}

	/* I must speak to a highly concerning confluence
	 *  of malfeasance taken place */
	
	return 0;
}


/*		mac_atob("F5:0:0:0:0:1", work.eth.h_source);
		mac_atob("F5:0:0:0:0:2", work.eth.h_dest);*/

inline void schedule_frame(frame_t *f) {
	struct timeval tv, diff;

	diff.tv_sec = 1;
	diff.tv_usec = 500000; 

	// how could gettimeofday fail?
	if ( 0 > gettimeofday(&tv, 0) )
		err("gettimeofday()");

	/* TIMERADD(3) */
	timeradd(&tv, &diff, &f->timetogo);
	
}
inline void translate_frame(frame_t *f) {
	mac_atob("F5:0:0:0:0:1", f->data.eth.h_source);
	mac_atob("F5:0:0:0:0:2", f->data.eth.h_dest);
}

frame_t *first_frame = NULL;

inline void queue_frame(frame_t *f) {
	frame_t *new_frame, *last_frame;

	/* allocate memory the size of f */
	int f_size = sizeof(frame_t) - sizeof(framedata_t) + f->length;

	if ( ! (new_frame = malloc(f_size) ) )
			err("malloc()");

	/* copy f into the memory */
	memcpy(new_frame, f, f_size);

	/* find the last frame */
	if ( first_frame == NULL ) {
		first_frame = new_frame;
		last_frame = new_frame;
	} else {
		last_frame = first_frame;
		while ( last_frame->next )
			last_frame = last_frame->next;
	}

	/* set its next to the new memory */
	last_frame->next = new_frame;

	/* this is the last frame */
	new_frame->next = NULL;
}
inline void dole_frame(int outfd) {
	
	/* return if first_frame is not ready to send
	 *  (or no first frame)*/
	if ( (first_frame == NULL) || (ttn_frame() > 0) )
		return;

	/* send it away - update first_frame then free the memory */
	if ( 0 > write(outfd, first_frame->data.buffer, first_frame->length) )
				err("write()");
	
	frame_t *tmp = first_frame;
	first_frame = first_frame->next;
	free(tmp);
}

/* return the time to the next frame */
inline int ttn_frame(void)
{
	
	struct timeval tv, diff;

	if ( first_frame == NULL)
		/* return a large value. when one comes in it will
		 * break the poll() and we will be called again 
		 * with a real first_frame */
		return 5000; /* 5 seconds .. pros? cons? */
		
	gettimeofday(&tv, 0);
	timersub(&first_frame->timetogo, &tv, &diff);

	/* if the next packet is ready to go, sec will be < 0.
	 * for example, if it is 1 microsecond past its time to go,
	 * diff will be sec = -1, usec = 999999. */

	int retval;

	if ( diff.tv_sec < 0 ) 
		retval = 0;
	else
		retval = diff.tv_sec * 1000 + diff.tv_usec / 1000;

	return retval;
}

inline int count_frames(void) {
	int c=0;
	frame_t *ptr = first_frame;
	
	while(ptr != NULL) {
		c++;
		ptr = ptr->next;
	}	

	return c;
}







