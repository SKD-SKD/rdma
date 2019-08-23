delay(s) in example.c - if the server cpu count too small for MLX5 testing

make sure you have Mofed and HCAs installed

build: cc -std=gnu90 -w -g -I ./ example.c CM.c RDMA.c -o test -lrdmacm -libverbs -lpthread

run: 
node 1: taskset 0x1f ./test -n 1 -f 11.11.11.66 -s 11.11.11.67 -p 2777 -d 3  
node 0: taskset 0x1f ./test -n 0 -f 11.11.11.66 -s 11.11.11.67 -p 2777 -d 3  
change taskset to run on more CPUs

include: 
CM.h -DMA.c

to drive send/receive testing and turning: 
example.c
