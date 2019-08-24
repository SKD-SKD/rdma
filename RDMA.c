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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>
#include <CM.h>
#include "RDMA.h"


#include <time.h>

time_t timer;
char buffer[26];
struct tm* tm_info;
int countr = 0;

//#define TIMING
#define LOCAL_MR_REG
#define PROCS_2


struct timeval  tv1, tv2;
inline void  RDMA_Dt(char *st) {
#ifdef TIMING
	gettimeofday(&tv2, NULL);
	printf ("%s : total time = %f seconds\n",
			(double) (tv2.tv_usec - tv1.tv_usec) +
			(double) (tv2.tv_sec - tv1.tv_sec),st) / 1000000;
	gettimeofday(&tv1, NULL);
#endif
}

static struct timeval t1, t2;
static int itime, atime=0;
static inline double  RDMA_Dt_(char *st) { /*~3 or less usec overhead */
	int dtime;
	gettimeofday(&t2, NULL);
	dtime = t2.tv_usec - t1.tv_usec;
	if ( (dtime > 0) && (dtime <10000) ){ 
		itime = dtime;
		atime = atime + itime;
	}
	//t1.tv_usec = t2.tv_usec;
	gettimeofday(&t1, NULL);
	//printf ("< in  %s : total time = %u useconds,  avarage = %u \n", st, itime, atime);
	return itime;
}




inline void  Btrace(void){
	//  fprintf(stderr, "trace %08x->%08x->%08x->%08x\n",
	//                 __builtin_return_address(0), __builtin_return_address(1),  __builtin_return_address(2),  __builtin_return_address(3) );
}


inline void  RDMA_PS_Lock(int ContID){};
inline void  RDMA_PS_UnLock(int ContID){};
inline void  RDMA_PS_InitLock(){};


inline void  RDMA_WR_Lock(int ContID)   {
#if 0 
	Connection *Ep = get_connection( ContID );
	pthread_mutex_t * lock = &(Ep->lock);
	pthread_mutex_lock(lock);
#endif
};  // TBD for other run time

inline void  RDMA_WR_UnLock(int ContID) {
#if 0 
	Connection *Ep = get_connection( ContID );
	pthread_mutex_t * lock = &(Ep->lock);
	pthread_mutex_unlock(lock);
#endif 
};  // TBD for other run time

inline void  RDMA_WR_InitLock() {
	int conn  =0;
	int ret =0;
	for ( conn =0; conn < MAX_CONN ;conn++){
		Connection *Ep = get_connection( conn );
		pthread_mutex_t * lock = &(Ep->lock);
		ret = pthread_mutex_init(lock, NULL);
		if (ret) {
			printf("\n mutex init failed ...... \n");
			return ;
		}
	}
}

#define POLL_TIMEOUT 200000//500000////2*50000000
#define TIMES 1 //*1024



static int nodeID;
void *DMAddr;
volatile uint64_t *c;
static void  GM_RDMA_MEM(){
	int fd;
	//void *addr;
	//volatile uint64_t *c;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		printf("Open failed\n");
	}

	DMAddr = mmap((void *)0x7fb42273f000, (size_t)16 * 1024 * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_SHARED, fd,
			(uint64_t)1024 * 1024 * 1024 * 32);
	printf("errno = %d\n", errno);
	printf("addr = %p\n", DMAddr);

	c = (uint64_t *)DMAddr;
}


inline int  RDMA_GetThisNodeId(){
	return nodeID;
}


inline int  RDMA_SetThisNodeId(int node_id){
	nodeID = node_id;
}


//this API tbd
inline int  RDMA_Get_MR_Rkey_for_Region(int ContID, void *buf_addr, uint32_t buf_size) {
	RDMA_WR_Lock(ContID);

	int completionCode = 0;

	struct ibv_pd *pd;
	struct ibv_mr *mr;

	Connection *Ep = get_connection( ContID );
	pd = Ep->id->pd;

	mr = ibv_reg_mr(pd, buf_addr, buf_size, IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC);
	if (!mr) {
		fprintf(stderr, "Error, ibv_reg_mr() failed\n"); Btrace();
		completionCode = -22;//;(int)mr;
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
	fprintf(stderr, "OK mr->rkey\n");
	RDMA_WR_UnLock(ContID);
	return mr->rkey;
}



static inline int  Process_Completions2(int ContID) { 
	int completionCode=0;
	struct ibv_wc wc;

	Connection *Ep = get_connection( ContID );
	memset(&wc, 0, sizeof(wc));

	if ((completionCode = ibv_poll_cq(Ep->id->send_cq, 1, &wc)) < 0 ) {
		fprintf(stderr, "1.0 Error, ibv_poll_cq() failed\n");  Btrace();
		return completionCode;
	}

	if (completionCode == 0) return -222;

	if (completionCode > 1/*more then one element*/) {
		fprintf(stderr, "99.99 completionCode != 1 ibv_poll_cq() Hmm, return  \n"); Btrace();
		return completionCode;
	}

	if (completionCode == 1/*one element*/) {

		uint64_t * p  = (uint64_t *) wc.wr_id;
		//if (wc.wr_id != 0) printf (" *p %d\n ", *p);
		if (wc.wr_id != 0) *p = (uint64_t)wc.status;
		//if (wc.wr_id != 0) printf (" *p %d\n ", *p);

		if (wc.status != IBV_WC_SUCCESS) {
			fprintf(stderr, "3.1 Failed status: \033[31m --%s-- \033[37m (%d) for wc.status d: %d\n",
					ibv_wc_status_str(wc.status),
					wc.status, (int)wc.wr_id); Btrace();
		}

		completionCode = wc.status;
		return completionCode;
	}

	return completionCode;
}
static inline void  Process_All_Completions2(void) {
	int c;
	for (c =0; c<MAX_CONN; c++)  Process_Completions2(c);
}





static inline int  Process_Completions(int ContID) { // pass wc to poll on status - need to be zeerord at init
	int completionCode=0;
	struct ibv_wc wc;

	Connection *Ep = get_connection( ContID );

	memset(&wc, 0, sizeof(wc));
	int i;
	for (i = 0; i < POLL_TIMEOUT; i++) {

		if ((completionCode = ibv_poll_cq(Ep->id->send_cq, 1, &wc)) < 0 ) {
			fprintf(stderr, "1.0 Error, ibv_poll_cq() failed\n");  Btrace();

			return completionCode;
		}
		//fprintf(stderr, "-1.1OkPoll-");//"1.1 Ok, ibv_poll_cq()\n");

		if (completionCode == 0) {
			//	fprintf(stderr, ".");
			//fprintf(stderr, "2.0 Ok, ibv_poll_cq continue loop \n");
			continue;
		}

		if (completionCode == 1/*one element*/) {
			//fprintf(stderr, "3.0  with OK, ibv_poll_cq() Success - check for the wc.status\n");
			//verify the completion status !!

			uint64_t * p  = (uint64_t *) wc.wr_id; 
			//if (wc.wr_id != 0) printf (" *p %d\n ", *p);
			if (wc.wr_id != 0) *p = (uint64_t)wc.status; 
			//if (wc.wr_id != 0) printf (" *p %d\n ", *p);

			if (wc.status != IBV_WC_SUCCESS) {
				fprintf(stderr, "3.1 Failed status: \033[31m --%s-- \033[37m (%d) for wc.status d: %d\n",
						ibv_wc_status_str(wc.status),
						wc.status, (int)wc.wr_id); Btrace();
				completionCode = wc.status;
				return completionCode;
			}
			else {
				// IBV_WC_SUCCESS
				//fprintf(stderr, "3.3 OK, ibv_poll_cq() wc.status Success\n");
				//printf("ddd - - -- - : %d\n", completionCode);
				///!!! 
				if (wc.opcode == IBV_WC_RECV) printf ( "  WOOOOOOOOOOOOO  ");
				return completionCode;
			}
		}

		if (completionCode > 1/*more then one element*/) {
			fprintf(stderr, "99.99 completionCode != 1 ibv_poll_cq() Hmm, return  \n"); Btrace();
			return completionCode;
		}

	}

	//fprintf(stderr, "8888.888E rror, ibv_poll_cq() failed - POLL_TIMEOUT \n"); Btrace();
	completionCode = -POLL_TIMEOUT;

	return completionCode;
}


#ifdef SEND__
inline int  RDMA_Send(int ContID, uint64_t destAddr, uint32_t rkey, void *buf_addr, uint32_t buf_size, struct ibv_mr *fmr/*!!!!!*/ , uint32_t loc_key) {
#else 
	//inline int  RDMA_Send(int ContID, uint64_t destAddr, uint32_t rkey, void *buf_addr, uint32_t buf_size) {
#endif

	//printf( "  %u lkey -> %u\n", fmr , fmr->lkey );

	RDMA_Dt("WS start ----------  \0");

	RDMA_WR_Lock(ContID);
	int completionCode = 0;
	int completionCode1;
	int compStage=0;

	struct ibv_qp *qp;
	struct ibv_pd *pd;
	struct ibv_sge sg;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;

#ifdef SEND__
	struct ibv_mr *mr= fmr;
#else
	struct ibv_mr *mr;
#endif
	uint32_t PreRkey;
	int64_t BaseDestAddr;
	int64_t wc_status = -999;

	Connection *Ep = get_connection( ContID );
	pd = Ep->id->pd;
	qp = Ep->id->qp;
	//printf( " last, RDMA before post __counter %u mr  %u , lkey %u , pd %u, buf %u, size %u \n", countr, mr, mr->lkey, pd, buf_addr, buf_size );


	RDMA_Dt("WS before mr ->  \0");
#ifdef LOCAL_MR_REG
#ifndef SEND__ 
	mr = ibv_reg_mr(pd, buf_addr, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		fprintf(stderr, "Error, ibv_reg_mr() failed, %s\n",   strerror(errno));Btrace();
		completionCode = -22;// (int)mr;
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
#endif
#endif
	RDMA_Dt("WS after mr ->   \0");

	memset(&sg, 0, sizeof(sg));
	sg.addr = (uintptr_t)buf_addr;
	sg.length = buf_size;
	sg.lkey = mr->lkey; //loc_key

	memset(&wr, 0, sizeof(wr));
	wr.wr_id = (uint64_t )&wc_status;
	wr.sg_list = &sg;
	wr.num_sge = 1;
	wr.opcode = IBV_WR_SEND;
	wr.send_flags = IBV_SEND_SIGNALED ;//| IBV_SEND_INLINE;
	wr.next = NULL;

	RDMA_Dt("WS post send ->\0");


	RDMA_PS_Lock(ContID);
	if (completionCode = ibv_post_send(qp, &wr, &bad_wr)) { //  bad_wr not checked one wr
		fprintf(stderr, "Error, ibv_post_send() failed\n");Btrace();
#ifdef LOCAL_MR_REG
#ifndef SEND__
		ibv_dereg_mr(mr); //FIX THIS
#endif
#endif
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
	//fprintf(stderr, "Ok, ibv_post_send()\n");

	RDMA_Dt("WS after  post send -> \0");

	//
#ifdef SEND__
	// operations during time  
	countr++;
	if ( (countr % 5000000 ) == 0) {

		time(&timer);
		tm_info = localtime(&timer);

		strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
		puts(buffer);
		printf ( "..->5000000 countr %u \n", countr );

	}
#endif

	while(1){


          completionCode1 = Process_Completions(ContID);      
	  
	  if (completionCode1 != 1 && completionCode1 != -POLL_TIMEOUT )/*any problems in completion */ {
		  printf ("is other side have problem or exited ?  .. terminating ..\n");
		  sleep(2);
		  exit(1);
	  }
	  
	  if (completionCode1 == -POLL_TIMEOUT ) {usleep(1); compStage ++;}
          else {  if (compStage !=0) 
		  printf ( "stage = %u  \n", compStage ); 
		  break;}
	}

#ifdef PROCS_2
	while (wc_status == -999 ) ;  //processing 2
#endif

	RDMA_PS_UnLock(ContID);
	RDMA_Dt("WS after completom ->  \0");
#ifdef LOCAL_MR_REG
#ifndef SEND__
	ibv_dereg_mr(mr);//FIX THIS
#endif 
#endif
	RDMA_Dt("WS after derg  mr ->  \0");
	RDMA_WR_UnLock(ContID);
	RDMA_Dt("WS before return  ->  \0");
	return completionCode;
}




// Sgl, synchronous operation  __ TBG
inline int   RDMA_Send_Data(int ContID, uint64_t destAddr, uint64_t rkey, struct ibv_sge *sgl, int nsge ) {
	RDMA_Dt("WDATA start ----------  \0");
	RDMA_WR_Lock(ContID);
	int completionCode = 0;

	struct ibv_qp *qp;
	struct ibv_pd *pd;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;
	struct ibv_mr *mr[64];
	uint32_t PreRkey;
	int64_t BaseDestAddr;
	int64_t wc_status = -999;


	Connection *Ep = get_connection( ContID );
	pd = Ep->id->pd;
	qp = Ep->id->qp;

	struct ibv_sge *sglLp = sgl;
	struct ibv_sge *sglWp = sgl;
#ifdef LOCAL_MR_REG
	int n;
	for (n = 0; n < nsge; n++) {

		mr[n] = ibv_reg_mr(pd,  (void *)(sglLp->addr), sglLp->length, IBV_ACCESS_LOCAL_WRITE);
		if (!mr) {
			fprintf(stderr, "Error, ibv_reg_mr() for SGL failed\n"); Btrace();
			completionCode = -22;//(int)mr;
			RDMA_WR_UnLock(ContID);
			return completionCode;
		}
		sglLp->lkey = mr[n]->lkey;

		sglLp ++;
	}
#endif
	RDMA_Dt("WDATA got mr  \0");
	//printf ("SGL DATA ---- sglL->addr %u sglL->length %d sglL->lkey %d \n", sglLp->addr, sglLp->length, sglLp->lkey);


	memset(&wr, 0, sizeof(wr));
	wr.wr_id = (uint64_t )&wc_status;
	wr.next = NULL;
	wr.sg_list = sglWp;
	wr.num_sge = nsge;//1;
	wr.opcode = IBV_WR_SEND;
	wr.send_flags = IBV_SEND_SIGNALED;


	RDMA_PS_Lock(ContID);
	if (completionCode = ibv_post_send(qp, &wr, &bad_wr)) { //  bad_wr not checked one wr
		fprintf(stderr, "Error, ibv_post_send() failed\n"); Btrace();
#ifdef LOCAL_MR_REG
		for(n =0; n< nsge; n++) ibv_dereg_mr(mr[n]);
#endif
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
	RDMA_Dt("WDATA post done \0");
	//fprintf(stderr, "Ok, ibv_post_send()\n");
	completionCode =  Process_Completions(ContID);
	if (completionCode == -POLL_TIMEOUT ) {sleep(1); completionCode = Process_Completions(ContID); fprintf(stderr, "Send MSG poll retry 1\n");}
	if (completionCode == -POLL_TIMEOUT ) {sleep(2); completionCode = Process_Completions(ContID); fprintf(stderr, "Send MSG poll retry 2\n");}
	if (completionCode == -POLL_TIMEOUT ) {sleep(3); completionCode = Process_Completions(ContID); fprintf(stderr, "Send MSG poll retry 3\n");}



#ifdef PROCS_2
	while (wc_status == -999 ) ;  //processing 2
#endif


	RDMA_Dt("WDATA got completion \0");
	RDMA_PS_UnLock(ContID);
#ifdef LOCAL_MR_REG
	for(n =0; n< nsge; n++) ibv_dereg_mr(mr[n]);
#endif
	RDMA_WR_UnLock(ContID);
	RDMA_Dt("WDATA mr deregd , return  \0");
	return completionCode;

}





// one buffer 64 bit, synchronous operation
inline int   RDMA_Write_Short(int ContID, uint64_t destAddr, uint32_t rkey, void *buf_addr, uint32_t buf_size ) {

	RDMA_Dt("WS start ----------  \0");


	RDMA_WR_Lock(ContID);
	int completionCode = 0;

	struct ibv_qp *qp;
	struct ibv_pd *pd;
	struct ibv_sge sg;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;
	struct ibv_mr *mr;
	uint32_t PreRkey;
	int64_t BaseDestAddr;
	int64_t wc_status = -999;

	Connection *Ep = get_connection( ContID );
	pd = Ep->id->pd;
	qp = Ep->id->qp;
	PreRkey =  Ep->remote_info.rkey;
	BaseDestAddr =  Ep->remote_info.addr;;

	RDMA_Dt("WS before mr ->  \0");
#ifdef LOCAL_MR_REG 
	mr = ibv_reg_mr(pd, buf_addr, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		fprintf(stderr, "Error, ibv_reg_mr() failed, %s\n",   strerror(errno)); Btrace();
		completionCode = -22;// (int)mr;
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
#endif 
	RDMA_Dt("WS after mr ->   \0");

	memset(&sg, 0, sizeof(sg));
	sg.addr = (uintptr_t)buf_addr;
	sg.length = buf_size;
	sg.lkey = mr->lkey;

	memset(&wr, 0, sizeof(wr));
	wr.wr_id = (uint64_t )&wc_status;
	wr.sg_list = &sg;
	wr.num_sge = 1;
	wr.opcode = IBV_WR_RDMA_WRITE;
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.rdma.remote_addr = BaseDestAddr+destAddr;//remote_address
	//	wr.wr.rdma.rkey = rkey;// key to access remote memory
	wr.wr.rdma.rkey = PreRkey;// key to access remote memory


	//printf ("\nIO --- wr.wr.rdma.rkey:     %u  ---\n" , wr.wr.rdma.rkey) ;
	//printf ("\nIO ---  wr.wr.rdma.remote_addr: %u ---\n",  wr.wr.rdma.remote_addr) ;
	RDMA_Dt("WS post send ->\0");


	RDMA_PS_Lock(ContID);
	if (completionCode = ibv_post_send(qp, &wr, &bad_wr)) { //  bad_wr not checked one wr
		fprintf(stderr, "Error, ibv_post_send() failed\n"); Btrace();
#ifdef LOCAL_MR_REG
		ibv_dereg_mr(mr); //FIX THIS
#endif
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
	//fprintf(stderr, "Ok, ibv_post_send()\n");

	RDMA_Dt("WS after  post send -> \0");

	completionCode =  Process_Completions(ContID);

#ifdef PROCS_2
	while (wc_status == -999 ) ;  //processing 2
#endif

	RDMA_PS_UnLock(ContID);
	RDMA_Dt("WS after completom ->  \0");
#ifdef LOCAL_MR_REG
	ibv_dereg_mr(mr);//FIX THIS
#endif 
	RDMA_Dt("WS after derg  mr ->  \0");
	RDMA_WR_UnLock(ContID);
	RDMA_Dt("WS before return  ->  \0");
	return completionCode;
}



// one buffer 64 bit, synchronous operation
inline int   RDMA_Read_Short(int ContID, uint64_t destAddr, uint64_t rkey, void *buf_addr, uint32_t buf_size) {

	RDMA_Dt("RS start ----------  \0");

	RDMA_WR_Lock(ContID);
	int completionCode = 0;

	struct ibv_qp *qp;
	struct ibv_pd *pd;
	struct ibv_sge sg;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;
	struct ibv_mr *mr;
	uint32_t PreRkey;
	int64_t BaseDestAddr;
	int64_t wc_status = -999;


	Connection *Ep = get_connection( ContID );
	pd = Ep->id->pd;
	qp = Ep->id->qp;
	PreRkey =  Ep->remote_info.rkey;
	BaseDestAddr =  Ep->remote_info.addr;;

#ifdef LOCAL_MR_REG
	mr = ibv_reg_mr(pd, buf_addr, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		fprintf(stderr, "Error, ibv_reg_mr() failed\n"); Btrace();
		completionCode = -22;//(int)mr;
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
#endif
	RDMA_Dt("RS got mr  \0");

	memset(&sg, 0, sizeof(sg));
	sg.addr = (uintptr_t)buf_addr;
	sg.length = buf_size;
	sg.lkey = mr->lkey;

	memset(&wr, 0, sizeof(wr));
	wr.wr_id = (uint64_t )&wc_status;
	wr.sg_list = &sg;
	wr.num_sge = 1;
	wr.opcode = IBV_WR_RDMA_READ;
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.rdma.remote_addr = BaseDestAddr+destAddr;//remote_address
	//      wr.wr.rdma.rkey = rkey;// key to access remote memory
	wr.wr.rdma.rkey = PreRkey;// key to access remote memory

	RDMA_PS_Lock(ContID);
	if (completionCode = ibv_post_send(qp, &wr, &bad_wr)) { //  bad_wr not checked one wr
		fprintf(stderr, "Error, ibv_post_send() failed\n"); Btrace();
#ifdef LOCAL_MR_REG
		ibv_dereg_mr(mr); //FIX THIS
#endif
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
	RDMA_Dt("RS post done \0");
	//fprintf(stderr, "Ok, ibv_post_send()\n");
	completionCode =  Process_Completions(ContID);


#ifdef PROCS_2
	while (wc_status == -999 ) ;  //processing 2
#endif


	RDMA_Dt("RS got completion \0");
	RDMA_PS_UnLock(ContID);
#ifdef LOCAL_MR_REG
	ibv_dereg_mr(mr);//FIX THIS
#endif
	RDMA_WR_UnLock(ContID);
	RDMA_Dt("RS mr deregd , return  \0");
	return completionCode;

}


// one buffer 64 bit, synchronous operation
inline int   RDMA_CSAtomic(int ContID, uint64_t destAddr, uint64_t rkey, void *buf_addr, uint32_t buf_size, uint64_t compare, uint64_t swap) {
	RDMA_Dt("CSA start ----------  \0");
	RDMA_WR_Lock(ContID);
	int completionCode =0;

	struct ibv_qp *qp;
	struct ibv_pd *pd;
	struct ibv_sge sg;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;
	struct ibv_mr *mr;
	uint32_t PreRkey;
	int64_t BaseDestAddr;
	int64_t wc_status = -999;


	Connection *Ep = get_connection( ContID );
	pd = Ep->id->pd;
	qp = Ep->id->qp;
	PreRkey =  Ep->remote_info.rkey;
	BaseDestAddr =  Ep->remote_info.addr;;
#ifdef LOCAL_MR_REG
	mr = ibv_reg_mr(pd, buf_addr, buf_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		fprintf(stderr, "Error, ibv_reg_mr() failed\n"); Btrace();
		completionCode = -22;//(int)mr;
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
#endif
	RDMA_Dt("CSA got mr  \0");
	memset(&sg, 0, sizeof(sg));
	sg.addr = (uintptr_t)buf_addr;
	sg.length = 8;//buf_size;
	sg.lkey = mr->lkey;

	memset(&wr, 0, sizeof(wr));
	wr.wr_id = (uint64_t )&wc_status;
	wr.sg_list = &sg;
	wr.num_sge = 1;
	wr.send_flags = IBV_SEND_SIGNALED;
	// addr maust be 8 byte alligned --  if (BaseDestAddr & 0x7)  error 9  
	if ( BaseDestAddr+destAddr & 0x7 ) {  fprintf(stderr, "Error! Must be allignrd 8\n");  Btrace(); return -888; }
	// to test using RDMA 
#if 0
	wr.opcode =    IBV_WR_RDMA_WRITE ;/// IBV_WR_ATOMIC_CMP_AND_SWP;
	wr.wr.rdma.remote_addr = BaseDestAddr+destAddr;//remote_address
	wr.wr.rdma.rkey = rkey;// key to access remote memory
	wr.wr.rdma.rkey = PreRkey;// key to access remote memory

#endif 
#if 1
	wr.opcode =    IBV_WR_ATOMIC_CMP_AND_SWP;
	wr.wr.atomic.rkey =  PreRkey;
	wr.wr.atomic.remote_addr =  BaseDestAddr+destAddr;
	wr.wr.atomic.compare_add =  compare;//0ULL; /* expected value in remote address */
	wr.wr.atomic.swap = swap;// 1ULL; /* the value that remote address will be assigned to */
#endif 
	RDMA_PS_Lock(ContID);
	if (completionCode = ibv_post_send(qp, &wr, &bad_wr)) { //  bad_wr not checked one wr
		fprintf(stderr, "Error, ibv_post_send() failed\n"); Btrace();
#ifdef LOCAL_MR_REG
		ibv_dereg_mr(mr); //fix THIS
#endif
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
	RDMA_Dt("CSA post done \0");
	//fprintf(stderr, "Ok, ibv_post_send()\n");
	completionCode =  Process_Completions(ContID);


#ifdef PROCS_2
	while (wc_status == -999 ) ;  //processing 2
#endif


	RDMA_PS_UnLock(ContID);
	RDMA_Dt("CSA got completion \0");
#ifdef LOCAL_MR_REG
	ibv_dereg_mr(mr);
#endif
	RDMA_WR_UnLock(ContID);
	RDMA_Dt("CSA mr deregd , return  \0");
	return completionCode;

}



// Sgl, synchronous operation
inline int   RDMA_Read_Data(int ContID, uint64_t destAddr, uint64_t rkey, struct ibv_sge *sgl, int nsge ) {
	RDMA_Dt("RDATA start ----------  \0");       
	RDMA_WR_Lock(ContID);
	int completionCode = 0;

	struct ibv_qp *qp;
	struct ibv_pd *pd;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;
	struct ibv_mr *mr[64];
	uint32_t PreRkey;
	int64_t BaseDestAddr;
	int64_t wc_status = -999;


	Connection *Ep = get_connection( ContID );
	pd = Ep->id->pd;
	qp = Ep->id->qp;
	PreRkey =  Ep->remote_info.rkey;
	BaseDestAddr =  Ep->remote_info.addr;;

	struct ibv_sge *sglLp = sgl;
	struct ibv_sge *sglWp = sgl;
#ifdef LOCAL_MR_REG
	int n;
	for (n = 0; n < nsge; n++) {

		mr[n] = ibv_reg_mr(pd,  (void *)(sglLp->addr), sglLp->length, IBV_ACCESS_LOCAL_WRITE);
		if (!mr[n]) {
			fprintf(stderr, "Error, ibv_reg_mr() for SGL failed\n"); Btrace();

			completionCode = -22;//(int)mr;
			RDMA_WR_UnLock(ContID);
			return completionCode;
		}
		sglLp->lkey = mr[n]->lkey;

		sglLp ++;
	}
#endif
	RDMA_Dt("RDATA got mr  \0");

	memset(&wr, 0, sizeof(wr));
	wr.wr_id = (uint64_t )&wc_status;
	wr.next = NULL;
	wr.sg_list = sglWp;
	wr.num_sge = nsge;//1;
	wr.opcode = IBV_WR_RDMA_READ; 
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.rdma.remote_addr = BaseDestAddr+destAddr;//remote_address
	//      wr.wr.rdma.rkey = rkey;// key to access remote memory
	wr.wr.rdma.rkey = PreRkey;// key to access remote memory


	RDMA_PS_Lock(ContID);
	if (completionCode = ibv_post_send(qp, &wr, &bad_wr)) { //  bad_wr not checked one wr
		fprintf(stderr, "Error, ibv_post_send() failed\n"); Btrace();
#ifdef LOCAL_MR_REG
		for(n =0; n< nsge; n++) ibv_dereg_mr(mr[n]);
#endif
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
	RDMA_Dt("RDATA post done \0");
	//fprintf(stderr, "Ok, ibv_post_send()\n");
	completionCode =  Process_Completions(ContID);


#ifdef PROCS_2
	while (wc_status == -999 ) ;  //processing 2
#endif


	RDMA_Dt("RDATA got completion \0");
	RDMA_PS_UnLock(ContID);
#ifdef LOCAL_MR_REG
	for(n =0; n< nsge; n++) ibv_dereg_mr(mr[n]);
#endif
	RDMA_WR_UnLock(ContID);
	RDMA_Dt("RDATA mr deregd , return  \0");
	return completionCode;

}



// Sgl, synchronous operation
inline int   RDMA_Write_Data(int ContID, uint64_t destAddr, uint64_t rkey, struct ibv_sge *sgl, int nsge ) {
	RDMA_Dt("WDATA start ----------  \0");
	RDMA_WR_Lock(ContID);
	int completionCode = 0;

	struct ibv_qp *qp;
	struct ibv_pd *pd;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;
	struct ibv_mr *mr[64];
	uint32_t PreRkey;
	int64_t BaseDestAddr;
	int64_t wc_status = -999;


	Connection *Ep = get_connection( ContID );
	pd = Ep->id->pd;
	qp = Ep->id->qp;
	PreRkey =  Ep->remote_info.rkey;
	BaseDestAddr =  Ep->remote_info.addr;;

	struct ibv_sge *sglLp = sgl; 
	struct ibv_sge *sglWp = sgl;
#ifdef LOCAL_MR_REG
	int n;
	for (n = 0; n < nsge; n++) {

		mr[n] = ibv_reg_mr(pd,  (void *)(sglLp->addr), sglLp->length, IBV_ACCESS_LOCAL_WRITE);
		if (!mr) {
			fprintf(stderr, "Error, ibv_reg_mr() for SGL failed\n"); Btrace();
			completionCode = -22;//(int)mr;
			RDMA_WR_UnLock(ContID);
			return completionCode;
		}
		sglLp->lkey = mr[n]->lkey;

		sglLp ++;
	}
#endif
	RDMA_Dt("WDATA got mr  \0");
	//printf ("SGL DATA ---- sglL->addr %u sglL->length %d sglL->lkey %d \n", sglLp->addr, sglLp->length, sglLp->lkey);


	memset(&wr, 0, sizeof(wr));
	wr.wr_id = (uint64_t )&wc_status;
	wr.next = NULL;
	wr.sg_list = sglWp;
	wr.num_sge = nsge;//1;
	wr.opcode = IBV_WR_RDMA_WRITE;
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.rdma.remote_addr = BaseDestAddr+destAddr;//remote_address
	//      wr.wr.rdma.rkey = rkey;// key to access remote memory
	wr.wr.rdma.rkey = PreRkey;// key to access remote memory


	RDMA_PS_Lock(ContID);
	if (completionCode = ibv_post_send(qp, &wr, &bad_wr)) { //  bad_wr not checked one wr
		fprintf(stderr, "Error, ibv_post_send() failed\n"); Btrace();
#ifdef LOCAL_MR_REG
		for(n =0; n< nsge; n++) ibv_dereg_mr(mr[n]);
#endif
		RDMA_WR_UnLock(ContID);
		return completionCode;
	}
	RDMA_Dt("WDATA post done \0");
	//fprintf(stderr, "Ok, ibv_post_send()\n");
	completionCode =  Process_Completions(ContID);

#ifdef PROCS_2
	while (wc_status == -999 ) ;  //processing 2
#endif


	RDMA_Dt("WDATA got completion \0");
	RDMA_PS_UnLock(ContID);
#ifdef LOCAL_MR_REG
	for(n =0; n< nsge; n++) ibv_dereg_mr(mr[n]);
#endif
	RDMA_WR_UnLock(ContID);
	RDMA_Dt("WDATA mr deregd , return  \0");
	return completionCode;

}


static inline void  RDMA_Connect(int node_id){

	//Start connection server 
	int ret =0;
	ret = start_connection_server(node_id);
	if (ret){
		printf(" RDMA_Connect - fail to start thread(s) for connection service."); 
		exit(1);
	}
	printf( "# Successfully thread(s) for server listenning \n"); 

	int i;
	for (i = node_id ; i< MAX_NODE; i++){

		while (ret = create_connections(i, CONN_DATA_TYPE_E) )sleep (1) ;
		printf( "# Successfully created connections to %u target node for DATA\n",i); ;
		while (ret = create_connections(i, CONN_ATOMIC_TYPE_E) )sleep (1) ;
		printf( "# Successfully created connections to %u target node for ATOMIC\n",i); 

		while (ret = create_connections(i, CONN_DATA_TYPE_E_1) )sleep (1) ;
		printf( "# Successfully created connections to %u target node for DATA 1\n",i); ;
		while (ret = create_connections(i, CONN_ATOMIC_TYPE_E_1) )sleep (1) ;
		printf( "# Successfully created connections to %u target node for ATOMIC 1\n",i);

                while (ret = create_connections(i, CONN_DATA_TYPE_E_2) )sleep (1) ;
                printf( "# Successfully created connections to %u target node for DATA 2\n",i); ;
                while (ret = create_connections(i, CONN_ATOMIC_TYPE_E_2) )sleep (1) ;
                printf( "# Successfully created connections to %u target node for ATOMIC 2\n",i);

//                while (ret = create_connections(i, CONN_DATA_TYPE_E_3) )sleep (1) ;
//                printf( "# Successfully created connections to %u target node for DATA 3\n",i); ;
//                while (ret = create_connections(i, CONN_ATOMIC_TYPE_E_3) )sleep (1) ;
//                printf( "# Successfully created connections to %u target node for ATOMIC 3\n",i);


	}  
}



pthread_t thread[MAX_CONN];
int id[MAX_CONN];
inline int  RDMA_Init_Connect(int node_id) {
	int completionCode=0;

	RDMA_SetThisNodeId(node_id);
	printf( "> My Node id %d \n", RDMA_GetThisNodeId());

	RDMA_Connect(node_id);
	printf( ">  RDMA_Connect - done\n");

	wait_for_connections_done();

	RDMA_WR_InitLock();

	return completionCode;
}















