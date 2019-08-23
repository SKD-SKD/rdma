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

#ifndef CM_H_
#define CM_H_

#include <pthread.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>


typedef enum {
	CONN_DATA_TYPE_E = 0,
	CONN_ATOMIC_TYPE_E = 1,
	CONN_DATA_TYPE_E_1 = 2,
	CONN_ATOMIC_TYPE_E_1 = 3,
        CONN_DATA_TYPE_E_2 = 4,
        CONN_ATOMIC_TYPE_E_2 = 5,
//	CONN_DATA_TYPE_E_3 = 6,
//        CONN_ATOMIC_TYPE_E_3 = 7,
	CONN_MAX_TYPE_E 
} E_CONNECTION_TYPE;

#define MAX_NODE                2
#define MAX_CONN                ( MAX_NODE *  CONN_MAX_TYPE_E ) 

#define MAX_SEND_WORK_REQUEST   1
#define MAX_RECV_WORK_REQUEST   2000//10//30//60//4//80 
#define MAX_SEND_SQE            1
#define MAX_SEND_SGE            1
#define MAX_RECV_SGE            1 
#define MAX_INLINE_DATA         800//128 // 

#define DEFAULT_MSG_LENGTH      8192//16384//4096//8192//120//30//120//60//120//60//1024//128

#define DEFAULT_BUF_SIZE        1*1024*1024*1024 // remote buff


static char *RDMA_CM_PORT = "2777";
static char *mySERVER_NAME[MAX_CONN] = {"11.11.11.245","11.11.11.247"};
static int  mlx_device = 2; 

typedef struct tagConnEx
{
	uint32_t    flags;
	uint32_t    rkey;
	uint64_t    addr;
}ConnEx;

typedef struct tagClientInfo
{
	int client_node_id;
	ConnEx connEx;
}ClientInfo;

typedef struct tagRecvInfo
{
	struct ibv_mr * buf_mr;
	char   *buf_addr;  
}RecvInfo;



//connection
typedef	struct	tagDMConnection
{
	//User parameters
	char    *server_name;
	char    *server_port;
	uint8_t is_server;      // indicate this is server side or client side of qp
	int     client_node_id; // client node id, so human knows what server/client
	int     send_flags;     // to record whether inline data is supported or not

	//Resources
	struct rdma_cm_id	*id; //short

	// the msg resource
	struct ibv_mr *send_mr;
	struct ibv_mr *recv_mr;
	char *send_buf;
	char *recv_buf;
	uint32_t  msg_length;

	// the pd resource
	struct ibv_pd *pd;
	struct ibv_mr *dm_mr;
	char *dm_buf;
	//for debugging only  
	ConnEx  remote_info;
	pthread_mutex_t lock;
	pthread_mutex_t lock_S;

}Connection;

//
void recive_poc (void *argv );
void wait_for_connections_done();

//Start connection server 
int start_connection_server(int node_id);

//convinience 
Connection * get_connection_internal(int  node_id, uint32_t type);
Connection * get_connection (uint32_t conn_id);
void set_connection(int node_id, uint32_t type, Connection * conn);

//args 
void  set_port ( char * port );
void  set_ip   ( char *ip, int node);
void  set_device (int device);

//send recv
ssize_t x_send(int ep_id , const void * buf, size_t len, int flags, struct ibv_mr *mr);
ssize_t x_recv(int ep_id, void *buf, size_t len, int flags);


#endif /* CM_H_ */
