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

#ifndef  RDMA_H
#define  RDMA_H
#define SEND__ 

int   RDMA_Get_MR_Rkey_for_Region(int ContID, void *buf_addr, uint32_t buf_size);
int   RDMA_Write_Short(int ContID, uint64_t destAddr, uint32_t rkey, void *buf_addr, uint32_t buf_size);
int   RDMA_Read_Short(int ContID, uint64_t destAddr, uint64_t rkey, void *buf_addr, uint32_t buf_size);
int   RDMA_CSAtomic(int ContID, uint64_t destAddr, uint64_t rkey, void *buf_addr, uint32_t buf_size, uint64_t compare, uint64_t swap);
int   RDMA_Read_Data(int ContID, uint64_t destAddr, uint64_t rkey, struct ibv_sge *sgl, int nsge);
int   RDMA_Write_Data(int ContID, uint64_t destAddr, uint64_t rkey, struct ibv_sge *sgl, int nsge);

int   RDMA_SetThisNodeId(int node_id);
int   RDMA_GetThisNodeId();   
int   RDMA_Init_Connect(int NodeId); 
void  RDMA_Test(void); 

#ifdef SEND__
int   RDMA_Send(int ContID, uint64_t destAddr, uint32_t rkey, void *buf_addr, uint32_t buf_size, struct ibv_mr *mr/*!!!!!*/, uint32_t loc_key );
#else 
int   RDMA_Send(int ContID, uint64_t destAddr, uint32_t rkey, void *buf_addr, uint32_t buf_size );
#endif

#endif /*  RDMA_H */
