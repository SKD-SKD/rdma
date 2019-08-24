/*
   Copyright 2018 sk

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/


//example code no error recovery
//drive HCA using multiple threads on both side of the connection   
//measurements are printed in RDMA.c - for simplicity and accuracy reasons 
//install after the test or instead the test
//some printouts left below for informational purposes

#include <pthread.h>
#include <CM.h>
#include "RDMA.h"

#include <unistd.h>
#include <sys/time.h>


#define BUF_TEST_MAX_SIZE DEFAULT_MSG_LENGTH//8192//110//30//60//100//32
#define TEST_DATA_SIZE    DEFAULT_MSG_LENGTH//8192//100//30//60//100//32
#define TEST_ASK_SIZE 500

enum ex_msghead{
	DATA___PING = 0xDA,
	DATA_ASK___PONG = 0xAC
};


static struct timeval t1[MAX_CONN], t2[MAX_CONN];
static int itime[MAX_CONN], atime[MAX_CONN]={0,0,0,0,0,0,0,0};
static int cnta[MAX_CONN]={0,0,0,0,0,0,0,0};
static inline int dt(char *st , struct timeval * t1, struct timeval * t2, int itime, int atime ) { /*~3 or less usec overhead */
	int dtime;
	gettimeofday(t2, NULL);
	dtime = t2->tv_usec - t1->tv_usec;
	if ( (dtime > 0) && (dtime <1000) ){
		itime = dtime;
		atime = atime + itime;
	}
	//t1.tv_usec = t2.tv_usec;
	gettimeofday(t1, NULL);
	//printf ("< in  %s : total time = %u useconds,  avarage = %u \n", st, itime, atime);
	return atime;
}

static inline void da(int ex_msg_destID, unsigned int iter){
	// mesure avrage time to deliver to remote host memory
	cnta[ex_msg_destID]++;
	atime[ex_msg_destID] =  dt(",", &t1[ex_msg_destID], &t2[ex_msg_destID], itime[ex_msg_destID], atime[ex_msg_destID] );
	if (cnta[ex_msg_destID] == iter){
		printf("\navarage on %u is %u\n", ex_msg_destID, atime[ex_msg_destID] / (iter));
		atime[ex_msg_destID] =0;
		cnta[ex_msg_destID]=0;
	}
}

//if you need to lock 
static inline void  My_Lock(int ContID)   {
#if 1 
	Connection *Ep = get_connection( ContID );
	pthread_mutex_t * lock = &(Ep->lock_S);
	pthread_mutex_lock(lock);
#endif
}; 

static inline void  My_UnLock(int ContID) {
#if 1 
	Connection *Ep = get_connection( ContID );
	pthread_mutex_t * lock = &(Ep->lock_S);
	pthread_mutex_unlock(lock);
#endif
};  

static inline void  My_InitLock() {
	int conn  =0;
	int ret =0;
	for ( conn =0; conn < MAX_CONN ;conn++){
		Connection *Ep = get_connection( conn );
		pthread_mutex_t * lock = &(Ep->lock_S);
		ret = pthread_mutex_init(lock, NULL);
		if (ret) {
			printf("\n My_Lock mutex init failed ...... \n");
			return ;
		}
	}
}

static int ex_channel_base_number(){

	if (RDMA_GetThisNodeId() == 0)  return 1; 
	if (RDMA_GetThisNodeId() == 1)  return 0; 
} 





static void ex_send_data(void *args){
	int * buf = malloc (BUF_TEST_MAX_SIZE );
	int ex_msg_destID = *((int *)args);

	struct ibv_mr * mr = malloc (sizeof (struct ibv_mr) );
	memset(mr, 0, sizeof( struct ibv_mr));

	// preregister mr, flags 0
	mr = ibv_reg_mr((get_connection(ex_msg_destID)->id->pd), buf, BUF_TEST_MAX_SIZE, 0);
	//..IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC);
	//..printf ("destID %u  protection domain %u   \n", ex_msg_destID, (get_connection(ex_msg_destID)->id->pd ));
	//..printf( "caller -***********mr  %u , lkey %u , pd %u \n", mr, mr->lkey, get_connection(ex_msg_destID)->id->pd );

	while(1){
		//..slow down sending
		//..sleep(1);usleep(100);pthread_yield();
                //usleep(5);// green zone setup needs the delay - 6cpus only
		memset(buf, ex_msg_destID, sizeof(buf));
		//..printf("\nex_send_data> DestID  %u \n", ex_msg_destID);
		//1 to register and deregister mr in x_send
		//0 preregister mr with mr passed in
		//pass in mr struct with both modes   
		buf[0] = DATA___PING; x_send(ex_msg_destID, buf, TEST_DATA_SIZE, /*1*/0 ,  mr);
		// if need to send ACKs - better to do it in other thread  
		//buf[0] = DATA_ASK___PONG; x_send(ex_msg_destID, buf, TEST_DATA_SIZE, /*1*/0 ,  mr);
		//..da(ex_msg_destID, 1000000);
	}	
}

static int count = 0;
static void ex_recive(void *args){

	int buf[BUF_TEST_MAX_SIZE];
	int length;
	int ex_msg_head;
	struct ibv_wc wc;

	int ep_id = *((int *)args);//1
	int ep_conn_id_ajust;
	//..printf("\nex_recive> thread ep_id %u started \n", ep_id);

	int * buf_send = malloc (BUF_TEST_MAX_SIZE );
	int ex_msg_destID = *((int *)args);
	struct ibv_mr * mr = malloc (sizeof (struct ibv_mr) );
	memset(mr, 0, sizeof( struct ibv_mr));

	mr = ibv_reg_mr((get_connection(ex_msg_destID)->id->pd), buf_send, TEST_ASK_SIZE, 0);
	//..IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC);
	//..printf ("destID %u  protection domain %u   \n", ex_msg_destID, (get_connection(ex_msg_destID)->id->pd ));
	//..printf( " 1t caller -***********mr  %u , lkey %u , pd %u \n", mr, mr->lkey, get_connection(ex_msg_destID)->id->pd );

	while (1){
		count++;
		//..slow down reciving
		//..sleep(1);usleep(100);pthread_yield();

		memset(buf, 0, sizeof(buf));

		length = x_recv(ep_id, (void *)buf, DEFAULT_MSG_LENGTH/*8192*/, 0);
		//..printf ("\nex_recive POC thread> length %u\n", length );
		//..int i; for ( i=0; i<length; i++ ) printf ( "%x ", buf[i] );
		ex_msg_head = buf[0];
		//..printf ("head %x, count %u     \n", ex_msg_head, count);
		//..x_recv(ep_id, (void *)buf, 2* DEFAULT_MSG_LENGTH, 0);

		switch(ex_msg_head) {

			case DATA___PING:
				//..printf(" \nex_recive> ep_id %u - head %x got data  DATA, send an ASK msg\n", ep_id, ex_msg_head);
				buf_send[0] = DATA_ASK___PONG;
				//..send "ACK" on QP 
				//..x_send(ep_id, buf_send, TEST_ASK_SIZE, 0, mr);
				//..use another thread , for now testing added the line to Send thread to send "ACK" :(
				break;

			case DATA_ASK___PONG:
				//..printf (" \nex_recive> ep_id %u head %x got ack  DATA_ASK_PONG, send data again\n", ep_id, ex_msg_head);
				//..for testing do nothnig    
				break;

			default :
				printf ("\nex_recive> ep_id %u head %x unknown message \n", ep_id, ex_msg_head);
		}
	}
}



//on the QPs connected to other nodes only
static pthread_t ex_thread_r[MAX_CONN];
static int ex_id_r[MAX_CONN];

static pthread_t ex_thread_s[MAX_CONN];
static int ex_id_s[MAX_CONN];
//modify #if as needed for the load 
void  ex_create_threads() {

	My_InitLock();

	printf( "ex_> Node id %d \n", RDMA_GetThisNodeId());
	int i;
	for (i =  ex_channel_base_number(); i< MAX_CONN; i= i + MAX_NODE){
		ex_id_r[i] = i;
		if((pthread_create(&ex_thread_r[i],  NULL, (void *)&ex_recive, &ex_id_r[i] ))) {
			printf("ex_>  failed to start receive listening thread %u\n", i);
			exit(1);
		}
#if 0 
		if((pthread_create(&ex_thread_r[i],  NULL, (void *)&ex_recive, &ex_id_r[i] ))) {
			printf("ex_>  failed to start receive listening thread %u\n", i);
			exit(1);
		}
		if((pthread_create(&ex_thread_r[i],  NULL, (void *)&ex_recive, &ex_id_r[i] ))) {
			printf("ex_>  failed to start receive listening thread %u\n", i);
			exit(1);
		}
		if((pthread_create(&ex_thread_r[i],  NULL, (void *)&ex_recive, &ex_id_r[i] ))) {
			printf("ex_>  failed to start receive listening thread %u\n", i);
			exit(1);
		}
#endif 
	}
	if ( RDMA_GetThisNodeId() == 0 )sleep(3);
	for (i = ex_channel_base_number() ; i< MAX_CONN; i= i+ MAX_NODE ){
		ex_id_s[i] = i;
		if((pthread_create(&ex_thread_s[i],  NULL, (void *)&ex_send_data, &ex_id_s[i] ))) {
			printf("ex_>  failed to start msg sending thread %u\n", i);
			exit(1);
		}
#if 0  
		if((pthread_create(&ex_thread_s[i],  NULL, (void *)&ex_send_data, &ex_id_s[i] ))) {
			printf("ex_>  failed to start msg sending thread %u\n", i);
			exit(1);
		}
#endif
	}
	printf("\n ex_recive ex_send *********threads started**********  \n");
}
