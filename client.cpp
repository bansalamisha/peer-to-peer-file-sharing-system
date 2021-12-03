#include "header.h"
using namespace std;

struct clientInfo{
    string ip;
    string port;
};

struct downlaoadInfo{
    string ip;
    string port;
    string filename;
    string destfilePath;
    int piece;
};

int getFileSize(string filename){
  char file[1024];
  strcpy(file,filename.c_str());
  struct stat st;
  stat(file,&st);
  return st.st_size;//Bytes
}

void printError(string message){
    perror(message.c_str());
    exit(-1);
}

void printLists(string list){
    string group_name = "";
    for(int i=0;i<(int)list.length();i++){
        if(list[i] == ','){
            cout<<group_name<<endl;
            group_name = "";
        }else{
            group_name += list[i];
        }
    }
    cout<<group_name<<endl;
}

vector<string> parseCmdString(string cmd){
    vector<string> tokens;
    int len = cmd.length();
    string str = "";
    for(int i=0;i<len;i++){
        if(cmd[i] == ' '){
            tokens.push_back(str);
            str = "";
        }else{
            str += cmd[i];
        }
    }
    tokens.push_back(str);
    return tokens;
}

pair<int,string> getRequestedChunk(int chunkNum,string filename){
    char readBuf[PIECE];
    FILE *fp = fopen(filename.c_str(),"r");
    int offset = (chunkNum - 1)*PIECE;
    fseek(fp,offset,SEEK_SET);
    int n = fread(readBuf,sizeof(char),PIECE,fp);
    fclose(fp);
    fp = NULL;
    return {n,string(readBuf)};
}

void *handleRequests(void *sockfd){
        struct  clientInfo client_info;
       // string response = "Message from client with ip and port  " + client_info.ip + " " + client_info.port;
       // clog << response<<endl;
        int acceptfd = *((int *)sockfd);
        free(sockfd);
        char buffer[BUFFER];
        memset(buffer,'\0',sizeof(buffer));
        ssize_t read_status = read(acceptfd, buffer, BUFFER);
        // get msg from downloader about which chunk to download and correspoding filename
        if (read_status < 0){
                printError("Cannot read from Client");
        }
        vector<string> token = parseCmdString(string(buffer));
        int chunkNum = stoi(token[0]);
        string filename = token[1];
        int byteRead;
        string chunkData;
        tie(byteRead,chunkData) = getRequestedChunk(chunkNum,filename);
        if(byteRead < PIECE){
            chunkData = chunkData.substr(0,byteRead);
        }
        ssize_t write_status = write(acceptfd, chunkData.c_str(),chunkData.length());
        if (write_status < 0){
            printError("Cannot write to Client");
        }
    return NULL;
}

void *peerServer(void *clientServerInfo){
    
    struct clientInfo  client_info = *((struct clientInfo *)clientServerInfo);
    string port = client_info.port;
    string ip = client_info.ip;
    struct sockaddr_in serverSockAddress, clientSockAddress;
    socklen_t serverAddrSize = sizeof(serverSockAddress);
    socklen_t clientAddrSize = sizeof(serverSockAddress);
    char buffer[BUFFER] = {0};
    int opt = 1;
    //Create Socket()
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printError("Unable to create Socket");
    }

    int port_no = stoi(port);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))){
        printError("setsockopt");
    }

    memset(&serverSockAddress, '\0', sizeof(serverSockAddress)); // zero structure out
    serverSockAddress.sin_family = AF_INET;                      // match the socket() call
    //serverSockAddress.sin_addr.s_addr = INADDR_ANY; //127.2.1.1             // bind to any local address
    serverSockAddress.sin_port = htons(port_no);                 // specify port to listen on

    int inet_status = inet_pton(AF_INET,ip.c_str(),&serverSockAddress.sin_addr);
    if(inet_status < 0){
        printError("inet address not supported");
    }

    int bind_status = bind(sockfd, (struct sockaddr *)&serverSockAddress, serverAddrSize);
    if (bind_status < 0)
    {
        printError("unable to bind socket");
    }

    int listen_status = listen(sockfd, 10);
    if (listen_status < 0)
    {
        printError("unable to listen socket");
    }

    cout<<"Peer Server LISTENING\n";
    while(true){
        int acceptfd = accept(sockfd, (struct sockaddr *)&clientSockAddress, &clientAddrSize);
        //acceptfd is the socket of peer which connected to this server.
        if (acceptfd < 0)
        {
            printError("accept error");
        }

        pthread_t clientThread;
        int *acceptSockFd = (int *)malloc(sizeof(int));
        *acceptSockFd = acceptfd;
        pthread_create(&clientThread,NULL,handleRequests,acceptSockFd);
    }
    cout<<"Shutting down server\n";
    close(sockfd);
}

pair<string,string> getIP_PORT(string ip_port){
    string ip = "" , port = "";
    int i = 0;
    for(;i<ip_port.length() && ip_port[i] != ':';i++){
      ip += ip_port[i];
    }
    i++;
    for(;i<ip_port.length();i++){
      port += ip_port[i];
    }
    return {ip,port};
}


int establishConnection(string ip, string port){
    struct sockaddr_in serverSocketAddress;
    
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        printError("Unable to create Socket");
    }
    
    int port_no = stoi(port);

    serverSocketAddress.sin_family = AF_INET;
    serverSocketAddress.sin_port = htons(port_no);
    //serverSocketAddress.sin_addr.s_addr = INADDR_ANY;
    int inet_status = inet_pton(AF_INET,ip.c_str(),&serverSocketAddress.sin_addr);
    if(inet_status < 0){
        printError("inet address not supported");
    }
    
    int connect_status = connect(sockfd,(struct sockaddr *)&serverSocketAddress, sizeof(serverSocketAddress));
    
    if(connect_status < 0){
        printError("Unable to Connect");
    }
    clog<<"Connected to peer with ip : "<<ip<<" and port: "<<port<<endl;
    return sockfd;
}

void writeChunk(string data,int chunkNum,string filename){
    clog<<filename<<endl;
    FILE *fp = fopen(filename.c_str(),"r+");
    if(fp == NULL){
        clog<<"Error opening file\n";
    }
    int offset = (chunkNum - 1)*PIECE;
    clog<<"offset : "<<offset<<endl;
    fseek(fp,offset,SEEK_SET);
    clog<<"fseek done\n";
    fwrite(data.c_str(),sizeof(char),data.length(),fp);
    clog<<"fwrite done\n";
    fclose(fp);
    fp = NULL;
}

/*
    receive the required chunk and write it.
*/
void *downloadHandler(void *info){
    clog<<"Inside downloadHandler\n";
    struct downlaoadInfo dwnld = *((struct downlaoadInfo*)info);
    string ip = dwnld.ip;
    string port = dwnld.port;
    int piece = dwnld.piece;
    string filename = dwnld.filename;
    string destPath = dwnld.destfilePath; 
    clog<<"Establishing connection\n";
    int peerSock  = establishConnection(ip,port);
    clog<<"Connection Established\n";
    char buffer[BUFFER];
    memset(buffer,'\0',sizeof(buffer));
    string clientmsg = to_string(piece) + " " + filename ;
    clog<<"Inside download handler piece + filename: "<<clientmsg<<endl;
    ssize_t write_status = write(peerSock,clientmsg.c_str(),clientmsg.length());
    if(write_status < 0){
        printError("Cannot write to Server");
    }
    clog<<"written complete , waiting for reply\n";
    ssize_t read_status = read(peerSock,buffer,BUFFER); //get chunk bytes
    if(read_status < 0){
        printError("Cannot read Server");
    }
    clog<<"Inside download handler chunk bytes "<<endl;
    //write chunk to correct location
    writeChunk(string(buffer),piece,destPath);
    clog<<"Chunk written successfully"<<endl;
    return NULL;
}

/*
* This function will get ip and port of peer having requested chunk.
*/
void downloadFile(int trackerSockfd,int chunks,string filename,string destpath){
    vector<string> chunksSHA[chunks+1];
    for(int i=1;i<=chunks;i++){
        char buffer[BUFFER];
        string clientmsg = "2 " + to_string(i); //Request ith chunk
        ssize_t write_status = write(trackerSockfd,clientmsg.c_str(),clientmsg.length());
        if(write_status < 0){
            printError("Cannot write to Server");
        }
        ssize_t read_status = read(trackerSockfd,buffer,BUFFER); //Recieve ip and port of peer
        if(read_status < 0){
            printError("Cannot read Server");
        }
        pair<string,string> ip_info = getIP_PORT(string(buffer));
        struct downlaoadInfo download_info;
        download_info.ip   = ip_info.first;
        download_info.port = ip_info.second;
        download_info.piece = i;
        download_info.filename = filename;
        download_info.destfilePath = destpath;
        pthread_t downloadThread;
        struct downlaoadInfo *dwnld = (struct downlaoadInfo*)malloc(sizeof(downlaoadInfo));
        *dwnld = download_info;
        clog<<"Creating thread for downloadHandler\n";
        pthread_create(&downloadThread,NULL,downloadHandler,dwnld);
        void *retval;
        pthread_join(downloadThread,&retval);
        clientmsg = "1 " + to_string(i);
        write_status = write(trackerSockfd,clientmsg.c_str(),clientmsg.length());
        if(write_status < 0){
            printError("Cannot write to Server");
        }
        memset(buffer,'\0',sizeof(buffer));
        read_status = read(trackerSockfd,buffer,BUFFER); //Recieve ip and port of peer
        if(read_status < 0){
            printError("Cannot read Server");
        }
        clog<<buffer<<endl;
    }
    clog<<"Exiting downloadFile\n";
}

int main(int argc, char const *argv[]){

   // ./client ip:port tracker.txt
    if(argc < 3){ // , IP , PORT
        printError("missing Port or IP");
    }
    pair<string,string> ip_info = getIP_PORT(argv[1]);
    struct clientInfo client_info;
        
    client_info.ip =  ip_info.first;
    client_info.port = ip_info.second;
    string tracker_info = argv[2];
    string serverPort = argv[2];
    //cout<<serverPort<<endl;
    pthread_t clientServerThread;
    
    struct clientInfo *client = (struct clientInfo*)malloc(sizeof(struct clientInfo));
    *client = client_info;
    pthread_create(&clientServerThread,NULL,peerServer,client);

    //for connectiing to tracker
    struct sockaddr_in serverSocketAddress;
    
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        printError("Unable to create Socket");
    }
    
    int port_no = stoi(serverPort);

    serverSocketAddress.sin_family = AF_INET;
    serverSocketAddress.sin_port = htons(port_no);
    serverSocketAddress.sin_addr.s_addr = INADDR_ANY;
    // int inet_status = inet_pton(AF_INET,argv[1],&serverSocketAddress.sin_addr);
    // if(inet_status < 0){
    //     printError("inet address not supported");
    // }

    int connect_status = connect(sockfd,(struct sockaddr *)&serverSocketAddress, sizeof(serverSocketAddress));
    
    if(connect_status < 0){
        printError("Unable to Connect");
    }
    
    cout<<"Connected to server\n";
    while(true){
        char clientmsg[MSG_SIZE];
        char buffer[BUFFER];
        memset(clientmsg,'\0',sizeof(clientmsg));
        memset(buffer,'\0',sizeof(buffer));
        string msg;
        fflush(stdout);
        cout<<"Enter Command: ";
        getline(cin,msg);
        string copy_msg = msg;
        msg = msg + " " + client_info.ip + " " + client_info.port;        
        strcpy(clientmsg,msg.c_str());
        ssize_t write_status = write(sockfd,clientmsg,strlen(clientmsg));
        vector<string> tokens = parseCmdString(copy_msg);
        if(write_status < 0){
            printError("Cannot write to Server");
        }
        clog<<"Before read\n";
        ssize_t read_status = read(sockfd,buffer,BUFFER);
        clog<<"after read\n";
        if(read_status < 0){
            printError("Cannot read Server");
        }
        if(tokens[0] == "list_requests"){
            string requests(buffer);
            if(requests == "-1"){
                clog<<"No requests !\n";
            }else{
                printLists(requests);
            }       
        }else if(tokens[0] == "list_groups"){
            string grps(buffer);
            if(grps == "-1"){
                clog<<"No groups !\n";
            }else{
                printLists(grps);
            }   
        }else if(tokens[0] == "download_file"){
            int status = stoi(string(buffer));
            if(status == -1){
                clog<<"Missing arguments\n";
            }else if(status == -2){
                clog<<"Login required to download file\n";
            }else if(status == -3){
                clog<<"Group ID does not exist\n";
            }else if(status == -4){
                clog<<"Requested file does not exist in the group\n";
            }else if(status == -5){
                clog<<"You are not a member of this group\n";
            }else{
                //get number of chunks of requested file
                string filename = tokens[2],destpath = tokens[3];
                int chunks = status;//getFileSize(filename);
                //int chunks = ceil((1.0 * fileSize/PIECE));
                clog<<"calling download file\n";
                downloadFile(sockfd,chunks,filename,destpath);
                clog<<"download file completed\n";
            }
        }else{
            cout<<"Message from server : "<<buffer<<endl;
        }
    }
    close(sockfd);
}