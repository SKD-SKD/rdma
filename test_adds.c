//add to RDMA.c to do all testing below
//modify as needed 
//unit test area  


void  RDMA_Test(){
	/*  some see CM.h
	//User parameters
	char    *server_name;
	char    *server_port;
	uint8_t is_server;      // indicate this is server side or client side of qp
	int     client_node_id; // client node id, so human knows what server/client
	int     send_flags;     // to record whether inline data is supported or not

	//Resources
	struct rdma_cm_id   *id; //short

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

	ConnEx  remote_info;
	*/

	printf ( " ..... %u, %u", get_connection(0), get_connection(1) );
	int i; for ( i =0; i<MAX_CONN; i++){
		printf ( " \n ..... element %u, is server %u, client node id %u, send flags %u, dm buff %u, cm id %u, remote info.rkey %u, remote info.addr %u ",  
				i,
				get_connection(i)->is_server,
				get_connection(i)->client_node_id,
				get_connection(i)->send_flags,
				get_connection(i)->dm_buf,
				get_connection(i)->id,
				get_connection(i)->remote_info.rkey,
				get_connection(i)->remote_info.addr
		       ); }
	printf ("\n ");

	int node_id =  RDMA_GetThisNodeId();

	int DATA[100];
	char * pDATA;
	pDATA = &DATA[0]; 
	struct ibv_sge sglA[10];

	//printf ( "&DATA addr %u  \n", &DATA   );
	//printf ( "DATA addr %u  \n", DATA   );
	//printf ( "pDATA addr %u  \n",pDATA   );

	memset(DATA, 1, sizeof(100)); //(:)

	sglA[0].addr = ( uint64_t )(void *) DATA;//*(uint64_t *)DATA;
	//printf ( "sglA addr %u  \n", sglA[0].addr   );
	sglA[0].length = 8;
	sglA[1].addr = ( uint64_t ) DATA;//*(uint64_t *)DATA;
	//printf ( "sglA addr %u  \n", sglA[1].addr   );
	sglA[1].length = 8;
	sglA[2].addr = ( uint64_t )(void *) DATA;//*(uint64_t *)DATA;
	//printf ( "sglA addr %u  \n", sglA[2].addr   );
	sglA[2].length = 8;
	sglA[3].addr = ( uint64_t ) DATA;//*(uint64_t *)DATA;
	//printf ( "sglA addr %u  \n", sglA[3].addr   );
	sglA[3].length = 8;
	sglA[4].addr = ( uint64_t )(void *) DATA;//*(uint64_t *)DATA;
	//printf ( "sglA addr %u  \n", sglA[4].addr   );
	sglA[4].length = 8;
	sglA[5].addr = ( uint64_t ) DATA;//*(uint64_t *)DATA;
	//printf ( "sglA addr %u  \n", sglA[5].addr   );
	sglA[5].length = 8;
	sglA[6].addr = ( uint64_t )(void *) DATA;//*(uint64_t *)DATA;
	//printf ( "sglA addr %u  \n", sglA[6].addr   );
	sglA[6].length = 8;
	sglA[7].addr = ( uint64_t ) DATA;//*(uint64_t *)DATA;
	//printf ( "sglA addr %u  \n", sglA[7].addr   );
	sglA[7].length = 8;

	printf("Test> start ********** \n");

#if 0    
	struct ibv_wc wc;
	struct ibv_mr *Tmr;
	Tmr = ibv_reg_mr(conn_array[1]->id->pd, DATA, 40, IBV_ACCESS_LOCAL_WRITE);
	if (!Tmr) {
		fprintf(stderr, "Error, ibv_reg_mr() failed\n");
		return -1;
	}
	int Rret = rdma_post_write(conn_array[1]->id, NULL, DATA/*conn_array[1]->dm_buf*/, 16, Tmr/*conn_array[1]->dm_mr*/, 0, 
			conn_array[1]->remote_info.addr, conn_array[1]->remote_info.rkey); 

	if (Rret)
	{
		printf("failed to post write.\n");
	}
	while ((Rret = rdma_get_send_comp(conn_array[1]->id, &wc)) == 0);
	if (Rret < 0) {
		perror("rdma_get_send_comp");
		return Rret;
	}
	if (wc.status != IBV_WC_SUCCESS)
	{
		printf("completion status is %d.\n", wc.status);
	} else
	{
		printf(" from LI success.\n");
	}
#endif

	//below send test fot the node 0, modify on node 2 


	//     RDMA_Send(0, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  test ** RDMA_Send Done \n");
	//     RDMA_Send(1, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  test ** RDMA_Send Done \n");
	//     RDMA_Send(2, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  test ** RDMA_Send Done \n");
	//     RDMA_Send(3, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  test ** RDMA_Send Done \n");
#if 0
	if (node_id == 0){
		int s1;
		memset(DATA, 1, sizeof(100)); //(:
		for (s1=0;s1<32;s1++){   x_send(1, DATA, 32, 0);}
		memset(DATA, 3, sizeof(100)); //(:
		for (s1=0;s1<32;s1++){   x_send(3, DATA, 32, 0);}
		printf("\nTest> ~~~~~~ loop test x_send to 1 and 3 Done \n");
	}


	if (node_id == 1){
		int s1;
		memset(DATA, 0, sizeof(100)); //(:
		for (s1=0;s1<32;s1++){   x_send(0, DATA, 32, 0);}
		memset(DATA, 2, sizeof(100)); //(:
		for (s1=0;s1<32;s1++){   x_send(2, DATA, 32, 0);}
		printf("\nTest> ~~~~~~ loop test x_send to 0 and 2 Done \n");
	}

#endif
#if 0
	if (node_id == 0){
		int s1;
		memset(DATA, 1, sizeof(100));
		for (s1=0;s1<4;s1++) RDMA_Send(0, 0/* ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  loop test to 1 ** RDMA_Send Done \n");
		for (s1=0;s1<32;s1++) RDMA_Send(1, 0/* ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  loop test to 1 ** RDMA_Send Done \n");
		for (s1=0;s1<4;s1++) RDMA_Send(2, 0/* ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  loop test to 1 ** RDMA_Send Done \n");
		for (s1=0;s1<32;s1++) RDMA_Send(3, 0/* ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  loop test to 3 ** RDMA_Send Done \n");
		memset(DATA, 2, sizeof(100)); //(:)
		RDMA_Send_Data(1, 0/*ofset*/,  0/*key*/,   sglA, 2);  printf(" test ** RDMA_Send_Data Done \n");
		RDMA_Send_Data(3, 0/*ofset*/,  0/*key*/,   sglA, 2);  printf(" test ** RDMA_Send_Data Done \n");
		//
	}
	if (node_id == 1){
		int s1;
		memset(DATA, 1, sizeof(100)); //(:
		for (s1=0;s1<32;s1++) RDMA_Send(0, 0/* ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  loop test to 1 ** RDMA_Send Done \n");
		for (s1=0;s1<4;s1++) RDMA_Send(1, 0/* ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  loop test to 1 ** RDMA_Send Done \n");
		for (s1=0;s1<32;s1++) RDMA_Send(2, 0/* ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  loop test to 1 ** RDMA_Send Done \n");
		for (s1=0;s1<4;s1++) RDMA_Send(3, 0/* ofset*/,  0/*key*/, DATA, 8);  printf("SSSSSS  loop test to 3 ** RDMA_Send Done \n");
		memset(DATA, 2, sizeof(100)); //(:)
		RDMA_Send_Data(0, 0/*ofset*/,  0/*key*/,   sglA, 2);  printf(" test ** RDMA_Send_Data Done \n");
		RDMA_Send_Data(2, 0/*ofset*/,  0/*key*/,   sglA, 2);  printf(" test ** RDMA_Send_Data Done \n");
		//
	}
#endif 

	/////	RDMA_Write_Short(1, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("Test> ** RDMA_Write_Short Done \n");

	///	RDMA_Write_Short(1, 8/*ofset*/,  0/*key*/, DATA, 8);  printf("Test> ** RDMA_Write_Short Done \n");
	RDMA_Read_Short (1, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("Test> ** RDMA_Read_Short Done \n");
	RDMA_CSAtomic   (1, 0,           0,        DATA, 8, 0, 22);   printf("Test> ** RDMA_CSA Done \n");
	RDMA_Read_Short (1, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("Test> ** RDMA_Read_Short Done \n");

	////	RDMA_Write_Short(0, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("Test> ** RDMA_Write_Short Done \n");
	////	RDMA_Write_Short(0, 8/*ofset*/,  0/*key*/, DATA, 8);  printf("Test> ** RDMA_Write_Short Done \n");
	RDMA_Read_Short (0, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("Test> ** RDMA_Read_Short Done \n");
	RDMA_CSAtomic   (0, 0,           0,        DATA, 8, 0, 22);   printf("Test> ** RDMA_CSA Done \n");
	RDMA_Read_Short (0, 0/*ofset*/,  0/*key*/, DATA, 8);  printf("Test> ** RDMA_Read_Short Done \n");

	RDMA_Write_Data(1, 0/*ofset*/,  0/*key*/,   sglA, 2); printf("Test> ** RDMA_Write_Data Done \n");
	RDMA_Read_Data(1, 0/*ofset*/,  0/*key*/,   sglA, 2);  printf("Test> ** RDMA_Read_Data Done \n");

#if 1 
	for ( i =0; i<MAX_CONN; i++){
		int testloop;
		for (testloop = 0 ; testloop < TIMES*1024 ; testloop= testloop+8 ){

			/////			RDMA_Write_Short(i, testloop/*ofset*/,  0/*key*/, DATA, 8);  
#if 1
			//////			RDMA_Write_Short(i, testloop/*ofset*/,  0/*key*/, DATA, 8);  
			RDMA_Read_Short (i, testloop/*ofset*/,  0/*key*/, DATA, 8);  
			// RDMA_CSAtomic   (i, testloop,           0,        DATA, 8, 0, 22);   
			RDMA_Read_Short (i, testloop/*ofset*/,  0/*key*/, DATA, 8);  

			/////			RDMA_Write_Short(i, testloop/*ofset*/,  0/*key*/, DATA, 8); 
			//////			RDMA_Write_Short(i, testloop/*ofset*/,  0/*key*/, DATA, 8); 
			// RDMA_Read_Short (0, testloop/*ofset*/,  0/*key*/, DATA, 8); 
			RDMA_CSAtomic   (i, testloop         ,  0,        DATA, 8, 0, 22);
			RDMA_Read_Short (i, testloop/*ofset*/,  0/*key*/, DATA, 8);  

			RDMA_Write_Data(i, 0/*ofset*/,  0/*key*/,   sglA, 4);  
			RDMA_Read_Data(i, 0/*ofset*/,  0/*key*/,   sglA, 4);  
			RDMA_Write_Data(i, 0/*ofset*/,  0/*key*/,   sglA, 8); 
			RDMA_Read_Data(i, 0/*ofset*/,  0/*key*/,   sglA, 8);  
		}
#endif
	}
	printf("Test> RDMA testloop 1024*4 Done \n");
#endif

#if 0
	if (node_id == 0){
		int s1;
		memset(DATA, 1, sizeof(DATA)); //(:
		for (s1=0;s1<32;s1++){   x_send(1, DATA, 32, 0);}
		memset(DATA, 3, sizeof(DATA)); //(:
		for (s1=0;s1<32;s1++){   x_send(3, DATA, 32, 0);}
		x_send(1, DATA, 16, 0); 
		x_send(3, DATA, 16, 0);
		printf("\nTest> ~~~~~~ loop test x_send to 1 and 3 Done \n");
	}

	if (node_id == 1){
		int s1;
		memset(DATA, 0, sizeof(DATA)); //(:
		for (s1=0;s1<32;s1++){   x_send(0, DATA, 32, 0);}
		memset(DATA, 2, sizeof(DATA)); //(:
		for (s1=0;s1<32;s1++){   x_send(2, DATA, 32, 0);}
		x_send(0, DATA, 16, 0);
		x_send(2, DATA, 16, 0);
		printf("\nTest> ~~~~~~ loop test x_send to 0 and 2 Done \n");
	}


	//sinal to end processing threads
	x_send(0, DATA, 3, 0);
	x_send(2, DATA, 3, 0);
	x_send(1, DATA, 3, 0);
	x_send(3, DATA, 3, 0);


#endif
	printf ( "\nTest> end *********\n " ); 

}
