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


// this code is initial prototype to
// accelerate the detail design and
// enable initial code integration
// error paths are not tested at all 
// exit(1) on all connection steps with err  



#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>


#include <pthread.h>
#include <CM.h>

#include <getopt.h>
#include <CM.h>
#include "RDMA.h"
#include "example.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


static  pthread_t       tid[MAX_NODE];
static  pthread_attr_t  attr[MAX_NODE];
static  int             port_id[MAX_NODE];
static  unsigned int    node_id;
static  char            *remote_buffer;
static  int counT=0;  


Connection    *internal_conn_array[MAX_NODE][CONN_MAX_TYPE_E];


inline void wait_for_connections_done(){
	while  ( counT != (RDMA_GetThisNodeId() +1) * CONN_MAX_TYPE_E ) sleep(1);
	printf("wait_for_connections_done OK ready to go \n");
}


//receve protytype 1
void _recive_poc(void *args){
	struct ibv_wc wc;
	int connection_id = *((int *)args);//1
	printf(" connection_id  %u  \n", connection_id);
	while (1){
		sleep(1);
		if ((rdma_get_recv_comp( (get_connection( connection_id )->id), &wc)) != 0) {//check <0
			printf (" buffer %x \n",   *(int *) ((RecvInfo *)(wc.wr_id))->buf_addr );
			printf (" .> **** Got message %u context %u \n", connection_id , wc.wr_id )  ;
			rdma_post_recv(get_connection( connection_id )->id, wc.wr_id, ((RecvInfo *)(wc.wr_id))->buf_addr, DEFAULT_MSG_LENGTH, ((RecvInfo *)(wc.wr_id))->buf_mr);
		}
	}
}


inline ssize_t x_send(int ep_id , const void * buf, size_t len, int flags, struct ibv_mr *mr){
	
	//when call returns , with success, the buffer data is on remote node, with correct data  
	//success if ret = 0; on RDMA send 
	//printf ( "mr %u \n", mr );
	
	if ( flags == 1 ) mr = ibv_reg_mr((get_connection(ep_id)->id->pd), buf, len , 0); 
	
	if ( (RDMA_Send(ep_id, 0/*ofset*/, 0/*key*/, buf, len, mr , mr->lkey) == 0)) {
		/*printf ("first OK !\n");*/
		if ( flags == 1 ) mr = ibv_dereg_mr( mr);
		return len;
	}
	else {
		printf("\nx_send> RDMA_Send failed \n"); 
		if ( flags == 1 ) mr = ibv_dereg_mr( mr);
		return -1; 
	}
}  


inline ssize_t x_recv(int ep_id, void *buf, size_t len, int flags) {

	struct ibv_wc wc;
	//check buffer sise if len > DEFAULT_MSG_LENGTH - err
	if ( len > DEFAULT_MSG_LENGTH  ) { printf ( "\n buffer is too large for this Qp too recive\n"); return -1; }

	if ((rdma_get_recv_comp( (get_connection( ep_id )->id), &wc)) != 0) {//check <0
		//printf ("\nx_recv>  rdma_get_recv_comp OK buffer %x \n",   *(int *) ((RecvInfo *)(wc.wr_id))->buf_addr );
		//printf ("\nx_recv> **** Got message %u context %u \n", ep_id , wc.wr_id );
		//printf ("\nx_recv>  real length %u    \n ",wc.byte_len );                        

		if ( wc.byte_len < len ) 
		      memcpy ( buf, ((RecvInfo *)(wc.wr_id))->buf_addr, wc.byte_len );
		else  memcpy ( buf, ((RecvInfo *)(wc.wr_id))->buf_addr, len );
		
		//printf ("X_Rec    wc.wr_id %u, addr %u, mr%u \n", wc.wr_id,  ((RecvInfo *)(wc.wr_id))->buf_addr, ((RecvInfo *)(wc.wr_id))->buf_mr);
		//printf ("mr addr %u, mr length %u \n", (((RecvInfo *)(wc.wr_id))->buf_mr)->addr  , (((RecvInfo *)(wc.wr_id))->buf_mr)->length);
		rdma_post_recv(get_connection( ep_id )->id, wc.wr_id, ((RecvInfo *)(wc.wr_id))->buf_addr, 
				DEFAULT_MSG_LENGTH,
				((RecvInfo *)(wc.wr_id))->buf_mr);
		//printf ("X_Rec after rdma_post_recv len %u, wc.byte_len %u   \n", len, wc.byte_len);
	}
	if ( wc.byte_len < len ) 
	     return wc.byte_len;
	else return len;
};


//protytype 2
void recive_poc(void *args){
	int buf[100];
	int length;
	struct ibv_wc wc;
	int ep_id = *((int *)args);//1
	printf("\nx_recv> thread |~~> ep_id %u started \n", ep_id);
	while (1){
		//sleep slow down test 
		memset(buf, 0, sizeof(buf));
		//sleep(1);
		length = x_recv(ep_id, (void *)buf, 100, 0); 
		printf ("\nx_recv POC thread> length %u\n", length );
		int i; for ( i=0; i<length; i++ ) printf ( "%x ", buf[i] );
		if ( length ==3 ) break; // magic size
		// over buffer size test - x_recv(ep_id, (void *)buf, 2048, 0);
	}
	printf("\nx_recv> thread >~~| ep_id %u ended \n", ep_id);
}


static inline void listen_accept_connection(void *arg);
//Start the connection server to listen to connection request from client
int start_connection_server(int n) {

        int ret = 0;
        int i = 0; //loop if need later

        node_id = n;
        port_id[i] = i;

        if(! (remote_buffer = (char *) malloc(DEFAULT_BUF_SIZE))) {
                printf("out of memory.\n");
                exit(1);
        }
        memset(remote_buffer, 0x0, DEFAULT_BUF_SIZE);
        memset(internal_conn_array, 0, sizeof(internal_conn_array));

        if((ret = pthread_attr_init(&attr[i]))) {
                printf("failed to initialize thread attribute.\n");
                exit(1);
        }

        if((ret = pthread_create(&tid[i], &attr[i], (void *)&listen_accept_connection, &port_id[i]))) {
                printf("failed to start listening thread.\n");
                exit(1);
        }

        return ret;
}


//Register msg region for initial server/client data exchange
static inline int reg_msg(Connection *conn) {
	int ret = 0;

	// register rec and send buf for internal communication
	conn->send_buf = (char *) malloc(conn->msg_length);
	conn->recv_buf = (char *) malloc(conn->msg_length);
	memset(conn->recv_buf, 0, conn->msg_length);
	memset(conn->send_buf, 0, conn->msg_length);

	//rdma reg msgs
	if(! (conn->recv_mr = rdma_reg_msgs(conn->id, conn->recv_buf, conn->msg_length))) {
		printf("rdma_reg_msgs failed for receive buffer.\n");
		exit(1);
	}

	if (! ( conn->send_mr = rdma_reg_msgs(conn->id, conn->send_buf, conn->msg_length))) {
		printf("rdma_reg_msgs failed for send buffer.\n");
		exit(1);
	}
	return ret;
}


//TBD 
static inline int reg_msg_del(Connection *conn) {
	int ret = 0;

	free (conn->send_buf);
	free (conn->recv_buf);

	return ret;
}


//create end point
static inline int create_ep (Connection *conn){

	int ret = 0;
	struct rdma_addrinfo hints, *res;
	struct ibv_qp_init_attr init_attr;
	struct ibv_qp_attr qp_attr;

	memset(&hints, 0, sizeof hints);
	hints.ai_port_space = RDMA_PS_TCP;

	if (conn->is_server) {
		hints.ai_flags = RAI_PASSIVE;
	}

	if((ret = rdma_getaddrinfo(conn->server_name, conn->server_port, &hints, &res))) {
		printf("rdma_getaddrinfo: %s\n", gai_strerror(ret));
		exit(1);
	}

	//printf("get addrinfo for server %s, port %s\n", conn->server_name? conn->server_name:"NULL", conn->server_port);

	memset(&init_attr, 0, sizeof init_attr);
	init_attr.cap.max_send_wr  = MAX_SEND_WORK_REQUEST;
	init_attr.cap.max_recv_wr  = MAX_RECV_WORK_REQUEST;
	init_attr.cap.max_send_sge = MAX_SEND_SGE;
	init_attr.cap.max_recv_sge = MAX_RECV_SGE;
	init_attr.cap.max_inline_data = MAX_INLINE_DATA;

	init_attr.sq_sig_all = 1;

	// set a back pointer to the conn in the pq
	init_attr.qp_context = conn->id;

	if (conn->is_server)
	{
		// for server, we only call this function to create listen ep
		if ((ret = rdma_create_ep(&conn->id, res, NULL, NULL)))
		    printf ( "\n error rdma_create_ep Server ret %u\n", ret);
	}
	else
	{
		if ((ret = rdma_create_ep(&conn->id, res, NULL, &init_attr)))
		    printf ("\n error rdma_create_ep Client ret %u\n", ret);
	}

#if 0
	if (!conn->is_server){
		memset(&qp_attr, 0, sizeof qp_attr);
		//printf ("  before ibv_query_qp  \n" );sleep(1);
		if ((ret = ibv_query_qp(conn->id->qp, &qp_attr, IBV_QP_CAP, &init_attr))) {
			printf("ibv_query_qp error\n");
			exit(1);
		}
		printf (" QP info after rdma_create_ep > .......  %u max inline\n", qp_attr.cap.max_inline_data);
		printf (" QP info after rdma_create_ep > .......  %u max recv_wr\n", qp_attr.cap.max_recv_wr);
	}
#endif

#if 0     
	memset(&init_attr, 0, sizeof init_attr);
	memset(&qp_attr, 0, sizeof qp_attr);
	if ((ret = ibv_query_qp(&conn->id->qp, &qp_attr, IBV_QP_CAP, &init_attr);//)) {
	printf("ibv_query_qp error\n");
	return;
}
if (qp_attr.cap.max_inline_data < MAX_INLINE_DATA)  printf(" err  ....rdma_server: device doesn't support MAX_INLINE_DATA \n ");
printf (" info > .......  %u max inline", qp_attr.cap.max_inline_data);
#endif

       rdma_freeaddrinfo(res);

       return ret;
}



//server thread to listen to connection requests
static inline void listen_accept_connection(void *args){

	int send_flags;
	int ret = 0;
	int i = 0;

	struct ibv_qp_init_attr init_attr;
	struct ibv_qp_attr qp_attr;
	struct ibv_wc wc;

	struct ibv_context **context;
	int    num_device = 0;
	ConnEx *conn_ex;

	Connection   *listen_conn = (Connection *)malloc(sizeof(Connection));
	memset (listen_conn, 0, sizeof(Connection));
	listen_conn->is_server = 1;

	listen_conn->server_port = RDMA_CM_PORT;
	if (create_ep(listen_conn)) {
		printf("listen_connection: failed to create end point to listen connection.\n");
		exit(1);
	}

	if ((ret = rdma_listen(listen_conn->id, 0))) {     //0 backlog queue
		printf("rdma_listen err\n");
		exit(1);
	}
	printf("Listen to port %s.\n", listen_conn->server_port);

	counT = 0;
	while (1){ //keep accepting connection requests
		
		Connection   *conn = (Connection *)malloc(sizeof(Connection));
		memset (conn, 0, sizeof(Connection));
		conn->is_server = 1;
		conn->msg_length = DEFAULT_MSG_LENGTH;


		if ((ret = rdma_get_request(listen_conn->id, &conn->id))){
			printf("rdma_get_request err\n");
			exit(1);
		}
		printf("Server node %d, get request at port %s.\n", node_id, listen_conn->server_port);

		//get device
		context = rdma_get_devices(&num_device);
		if (num_device == 0){
			printf("no suitable device is found. \n");
			exit(1);
		}

		//create pd
		conn->pd = ibv_alloc_pd(context[ mlx_device ]);

		memset(&init_attr, 0, sizeof init_attr);
		init_attr.qp_context = context;
		init_attr.qp_type = IBV_QPT_RC;

		init_attr.cap.max_send_wr  = MAX_SEND_WORK_REQUEST;
		init_attr.cap.max_recv_wr  = MAX_RECV_WORK_REQUEST;
		init_attr.cap.max_send_sge = MAX_SEND_SGE;
		init_attr.cap.max_recv_sge = MAX_RECV_SGE;
		init_attr.cap.max_inline_data = MAX_INLINE_DATA;
		init_attr.sq_sig_all = 1;


		if ((ret = rdma_create_qp(conn->id, conn->pd, &init_attr))){
			printf("rdma_create_qp err \n");
			exit(1);
		}

		memset(&qp_attr, 0, sizeof qp_attr);
		if ((ret = ibv_query_qp(conn->id->qp, &qp_attr, IBV_QP_CAP, &init_attr))) {
			printf("ibv_query_qp error\n");
			exit(1);
		}
		//!!!
		if (qp_attr.cap.max_inline_data < MAX_INLINE_DATA)  
			printf(" err  ....rdma_server: device doesn't support MAX_INLINE_DATA \n ");

		printf (" %u QP max inline", qp_attr.cap.max_inline_data);


		if ((ret = reg_msg(conn))) {
			printf("failed to register memory.\n");
			exit(1);
		}

		// post recv buffer for incoming messages
		if ((ret = rdma_post_recv(conn->id, NULL, conn->recv_buf, conn->msg_length, conn->recv_mr))) {
			printf("rdma_post_recv err\n");
			exit(1);
		}

		if ((ret = rdma_accept(conn->id, NULL))) {
			printf("rdma_accept err\n");
			exit(1);
		}


		while ((ret = rdma_get_recv_comp(conn->id, &wc)) == 0);
		if (ret < 0) {
			printf("rdma_get_recv_comp err\n");
			exit(1);
		}

		// save client id sent from client
		conn->client_node_id = ((ClientInfo *)conn->recv_buf)->client_node_id;

		// server side always register the memory block
		conn->dm_buf = remote_buffer;
		conn->dm_mr = ibv_reg_mr ( conn->id->qp->pd, conn->dm_buf, DEFAULT_BUF_SIZE ,
				IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC);

		if (!conn->dm_mr) {
			printf("Error, ibv_reg_mr() failed\n");
			exit(1) ;
		}

		//printf("Server node %d, for client node %d, lkey is %u, rkey is %u, address is %ul.\n", 
		//		node_id, conn->client_node_id, conn->dm_mr->lkey, conn->dm_mr->rkey, (uint64_t)conn->dm_mr->addr);

		if (node_id == conn->client_node_id){
			// get a loop back connection, no need to update conn_array as client side already did
			// set flag to free this conn if it is not going to be used to pass msg
		}
		else{
			uint32_t conn_type = ((ClientInfo *)conn->recv_buf)->connEx.flags;
			Connection* dm_conn =  get_connection_internal(conn->client_node_id, conn_type);

			// get a connection from peer node, update the connection
			if ( dm_conn ){
				rdma_destroy_ep(conn->id);
				free(dm_conn);
				printf(" get_connection_internal err \n");
				exit(1); 
			}

			// save peer node rkey and addr information.
			conn->remote_info.addr = ((ClientInfo *)conn->recv_buf)->connEx.addr;
			conn->remote_info.rkey = ((ClientInfo *)conn->recv_buf)->connEx.rkey;
			conn->remote_info.flags = ((ClientInfo *)conn->recv_buf)->connEx.flags;
			set_connection(conn->client_node_id, conn_type, conn);

			printf("Server node %d, add to conn_array at index %d with rkey %u, address %ul.\n", 
					node_id, conn->client_node_id, conn->remote_info.rkey, conn->remote_info.addr);
		}

		// put rkey and addr to send buffer and send to client
		conn_ex = (ConnEx *)conn->send_buf;
		conn_ex->rkey = conn->dm_mr->rkey;
		conn_ex->addr = (uint64_t)conn->dm_mr->addr;

		if ((   ret = rdma_post_send(conn->id, NULL, conn->send_buf, conn->msg_length, conn->send_mr, conn->send_flags))) {
			printf("rdma_post_send err \n");
			exit(1);
		}

		while ((ret = rdma_get_send_comp(conn->id, &wc)) == 0);
		if (ret < 0){
		       	printf("rdma_get_send_comp err\n");
		        exit(1);
		}
		
		else printf("Server connection request processing - last post send complete.\n");

#if 0 
		printf ( " \n ..... #### Server conn  is server %u, client node id %u, send flags %u, dm buff %u, cm id %u, remote info.rkey %u, remote info.addr %u ",
				conn->is_server, conn->client_node_id, conn->send_flags, conn->dm_buf, conn->id, conn->remote_info.rkey, conn->remote_info.addr );
#endif 

		int iBR;
		for (iBR=0; iBR< MAX_RECV_WORK_REQUEST; iBR++){
			RecvInfo * info = malloc(sizeof(RecvInfo));
			memset(info, 0, sizeof(RecvInfo));
			info->buf_addr = malloc(DEFAULT_MSG_LENGTH); 
			info->buf_mr = malloc (sizeof (struct ibv_mr) ) ;
			info->buf_mr = ibv_reg_mr ( conn->id->qp->pd, info->buf_addr, DEFAULT_MSG_LENGTH, IBV_ACCESS_LOCAL_WRITE );
			rdma_post_recv(conn->id, (void *)info, info->buf_addr, DEFAULT_MSG_LENGTH, info->buf_mr);
			//			printf("\n server ----------------------iBR %u",iBR);
			//			printf("\n server -----server node id %u, connect id %u info %u, info->buf_addr %u, info->buf_mr %u", 
			//			conn->client_node_id, conn->id, info, info->buf_addr, info->buf_mr ) ;
		}
                printf("Server connection request processing - RX buffers post complete.\n");
		counT ++; printf ("\n @ listen times with connection done %u \n", counT );
	}
}


//client to create connections
inline int create_connections(int client_node_id, uint32_t type) {
	int ret = 0;
	int i = 0;
	int private_data = 0;

	struct ibv_wc wc;
	ConnEx *conn_ex;

	i=client_node_id;

	Connection * conn = malloc(sizeof(Connection));
	memset(conn, 0, sizeof(Connection));

	set_connection(i, type, conn);

	//printf("ip 0 ...%s   \n", mySERVER_NAME[0]);
	//printf("ip 1 ...%s   \n", mySERVER_NAME[1]);

	conn->server_name = mySERVER_NAME[i];
	conn->server_port = RDMA_CM_PORT;
	conn->client_node_id = client_node_id;

	if ((ret = create_ep(conn))){
		printf("create_connections: failed to create end point.\n");
		exit(1);
	}

	conn->msg_length = DEFAULT_MSG_LENGTH;
	if ((ret = reg_msg(conn))){
		printf("failed to register memory.\n");
		exit(1);
	}

	//!!!
	// if not a loop back, register the memory block for server side
	conn->dm_buf = remote_buffer;

	//register mr with all modes
	conn->dm_mr = ibv_reg_mr ( conn->id->qp->pd, conn->dm_buf, DEFAULT_BUF_SIZE ,
			IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC);
	if (!conn->dm_mr) {
		fprintf(stderr, "Error, ibv_reg_mr() failed\n");
		exit(1);
	}

	//printf("try Client node %d: for server node %d, lkey is %u, rkey is %u, address is %ul.\n", 
	//		conn->client_node_id, i, conn->dm_mr->lkey, conn->dm_mr->rkey, (uint64_t)conn->dm_mr->addr);

	//post receive bufferes 
	if ((ret = rdma_post_recv(conn->id, NULL, conn->recv_buf, DEFAULT_MSG_LENGTH, conn->recv_mr))){
		printf("rdma_post_recv err\n");
		exit(1);
	}

	//try to connect and return for the reconnect 
	if ((ret = rdma_connect(conn->id, NULL))){
		printf("rdma_connect err ...... return for retry \n");
		reg_msg_del (conn);
		ibv_dereg_mr(conn->dm_mr);
		rdma_destroy_ep(conn->id);
		return ret;
	}
	printf ("\n Connected or Reconnect OK  \n");

	((ClientInfo *)conn->send_buf)->client_node_id = node_id;

	//!!
	// When it is not loop back send the rkey and address across
	((ClientInfo *)conn->send_buf)->connEx.rkey = conn->dm_mr->rkey;
	((ClientInfo *)conn->send_buf)->connEx.flags = type;
	((ClientInfo *)conn->send_buf)->connEx.addr = (uint64_t)conn->dm_mr->addr;

	//send info
	if ((ret = rdma_post_send(conn->id, NULL, conn->send_buf, DEFAULT_MSG_LENGTH, conn->send_mr, conn->send_flags))){
		printf("rdma_post_send err \n");
		exit(1);
	}

	//get complition 
	while ((ret = rdma_get_send_comp(conn->id, &wc)) == 0);
	if (ret < 0) {
		printf("rdma_get_send_comp err\n");
		 exit(1);
	}

	// check send ok
	while ((ret = rdma_get_recv_comp(conn->id, &wc)) == 0);
	if (ret < 0){
		printf("rdma_get_recv_comp err\n");
		exit(1);
	}
	else    ret = 0;

	if (wc.status == IBV_WC_SUCCESS){
		conn_ex = (ConnEx *)conn->recv_buf;
		conn->remote_info.addr = conn_ex->addr;
		conn->remote_info.rkey = conn_ex->rkey;
	}
	else{
		printf("work completion status err %s.\n", ibv_wc_status_str(wc.status));
		exit(1);
	}

	printf("Client node %d: Successfully create a connection to node[%d]. connection id %ul\n", 
			client_node_id , i, conn->id);
	
	//printf("Client node %d: rkey to node %d is %u, address is %ul.\n", 
	//		conn->client_node_id, i, conn->remote_info.rkey, conn->remote_info.addr);

	int iBR;
	for (iBR=0; iBR< MAX_RECV_WORK_REQUEST; iBR++){
		RecvInfo * info = malloc(sizeof(RecvInfo));
		memset(info, 0, sizeof(RecvInfo));
		info->buf_addr = malloc(DEFAULT_MSG_LENGTH); 
		info->buf_mr = malloc (sizeof (struct ibv_mr) ) ;
		info->buf_mr = ibv_reg_mr ( conn->id->qp->pd, info->buf_addr, DEFAULT_MSG_LENGTH, IBV_ACCESS_LOCAL_WRITE );
		rdma_post_recv(conn->id, (void *)info, info->buf_addr, DEFAULT_MSG_LENGTH, info->buf_mr);
		//printf("\n client ----------------------iBR %u",iBR); 
		//printf("\n client-----client node id %u, connect id %u info %u, info->buf_addr %u, info->buf_mr %u\n",
		//		conn->client_node_id, conn->id, info, info->buf_addr, info->buf_mr ) ;
		//printf ("mr addr %u, mr length %u\n",info->buf_mr->addr  ,info->buf_mr->length  );

	}
	printf("Client - post RX buffers complete.\n");
	return ret;
}


inline Connection * get_connection_internal(int node_id, uint32_t type){
	if( type >= CONN_MAX_TYPE_E )
		return NULL;

	if( node_id > MAX_NODE )
		return NULL;

	return internal_conn_array[node_id][type];
}


inline Connection * get_connection(uint32_t conn_id){

	uint32_t type;
	int node_id;

	node_id = conn_id % MAX_NODE;
	type = conn_id / MAX_NODE;

	if( type >= CONN_MAX_TYPE_E )
		return NULL;

	if( node_id > MAX_NODE )
		return NULL;
	return internal_conn_array[node_id][type];
}

inline void set_connection(int node_id, uint32_t type, Connection * conn){

	if( type >= CONN_MAX_TYPE_E )
		return;

	if( node_id > MAX_NODE )
		return;
	internal_conn_array[node_id][type] = conn;
}

inline void  set_port ( char * port ){ RDMA_CM_PORT = port;}
inline void  set_ip   ( char *ip, int node) {  mySERVER_NAME[node] = ip; }
inline void  set_device (int device) { mlx_device = device; }

int main(int argc, char **argv)
{
	int node_id;
	int op;
	if (argc <= 10){
		printf("_usage: %s\n", argv[0]);
		printf("\t[-n node number]\n");
		printf("\t[-s node 1 ip]\n");
		printf("\t[-f node 0 ip]\n");
		printf("\t[-p port]\n");
		printf("\t[-d device]\n");
		exit(1);
	}
	while ((op = getopt(argc, argv, "n:s:f:p:d:")) != -1) {
		switch (op) {
			case 'n':
				node_id = atoi(optarg);
				break;
			case 's':
				mySERVER_NAME[1]=optarg;
				break;
			case 'f':
				mySERVER_NAME[0]=optarg;
				break;
			case 'p':
				RDMA_CM_PORT=optarg;
				break;
			case 'd':
	                        mlx_device=atoi(optarg); 
			        break;	

			default:
				printf("usage: %s\n", argv[0]);
				printf("\t[-n node number]\n");
				printf("\t[-s node 1 ip]\n");
				printf("\t[-f node 0 ip]\n");
				printf("\t[-p port]\n");
                                printf("\t[-d device]\n");
				exit(1);
		}
	}
	printf("Main> node_id .. %d \n", node_id);
	printf("Main> ip 0    ...%s   \n", mySERVER_NAME[0]);
	printf("Main> ip 1    ...%s   \n", mySERVER_NAME[1]);
	printf("Main> port    ...%s   \n", RDMA_CM_PORT);
        printf("Main> device  ...%d   \n", mlx_device);

        //set_port ("3777");
	//set_ip ("11.11.11.234",0);
	//set_ip ("11.11.11.235",1);
	//set_device (2); 

	RDMA_Init_Connect(node_id);

	//RDMA_Test()    ;
	//printf( "Main> RDMA_Test - done\n");

	// call rec / send 
	ex_create_threads();

	//put the test here late - now in the init
	printf( "Main> while(1) sleep(forever) \n");
	while(1) {sleep(100000);}

}
