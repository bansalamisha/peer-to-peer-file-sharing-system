#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <errno.h>
#include <error.h>
#include <pthread.h>
#include <time.h>
#include<openssl/sha.h>
#include <sys/stat.h>
#define BUFFER 1024
#define MSG_SIZE 1024
//#define PIECE 512
#define PIECE 524288 //512KB