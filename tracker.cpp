#include "header.h"
using namespace std;

struct fileInfo{
    string filename;
    int fileSize;
    map<int,string> pieces_sha;
    map<int,set<pair<int,string>>> pieceUserMap;
    string fileSha;
};

inline bool operator<(const fileInfo& left, const fileInfo& right)
{
  return left.fileSize < right.fileSize;
}

struct UserDetails{
    string username;
    string password;
    set<string> groupID;
    string IP_ADDR;
    int PORT;
};

struct GroupDetails{
    string groupID;
    string adminID; // admin username
    set<string> usernames;
    set<string> pendingRequests;
    //set<struct fileInfo> shareableFile;
    map<string,struct fileInfo> fileDetails; //filename , filestruct
    //map<string,set<string>> fileUserMap;//filename->set{users
};

map<string,struct UserDetails> user_login; // userID,struct
map<string,struct GroupDetails> groups_info; //grpID, struct
set<string> groupID;
set<string> loggedInUsers;

void printError(string message)
{
    perror(message.c_str());
    exit(-1);
}

void printUser(string userID){
    UserDetails usrDetails = user_login[userID];
    for(auto grpID : usrDetails.groupID){
        cout<<grpID<<" ";
    }
    cout<<endl;
    cout<<usrDetails.username<<endl;
    cout<<usrDetails.password<<endl;
}

void printfileInfo(struct fileInfo file){
    cout<<file.filename<<endl;
    cout<<file.fileSha<<endl;
    cout<<file.fileSize<<endl;
    map<int,string> mp = file.pieces_sha;
    for(auto i : mp){
        cout<<i.first<<" "<<i.second<<endl;
    }
}

void listShareableFiles(string grpID){
    for(auto files : groups_info[grpID].fileDetails){
        clog<<files.first<<endl;
    }
}


void sendCmdStatus(string message, int sockfd){
    char sendmsg[MSG_SIZE];
    memset(sendmsg,0,sizeof sendmsg);
    strcpy(sendmsg,message.c_str());
    ssize_t write_status = write(sockfd, sendmsg, strlen(sendmsg));
    if (write_status < 0){
        printError("Cannot write to Client");
    }
}

void listRequests(string grpID,int sockfd){
    string grps = "";
    cout<<groups_info[grpID].pendingRequests.size();
    for(auto request : groups_info[grpID].pendingRequests){
        grps += request + ",";
        grps = request + "," + grps;
    }
    cout<<grps<<endl;
    if(grps == ""){
        cout<<"sending reply for empty\n";
        sendCmdStatus("-1",sockfd);
    }else{
        cout<<"sending reply " + grps<<endl;
        sendCmdStatus(grps.substr(0,grps.length()-1),sockfd);
    }
}

void listGroup(int sockfd){
    string grps = "";
    for(auto grp : groups_info){
        grps = grp.first + "," + grps;
    }
    if(grps == ""){
        sendCmdStatus("-1",sockfd);
    }else{
        sendCmdStatus(grps.substr(0,grps.length()-1),sockfd);
    }
}

bool isUser(string userID){
    return user_login.find(userID) != user_login.end();
}

int acceptRequest(string grpID, string userID, string currUserID){
    //validate if curr user is admin
    if(groups_info[grpID].adminID != currUserID || (!isUser(userID))){
        return -1; // Not a admin or not a user in system
    }
    groups_info[grpID].usernames.insert(userID);
    groups_info[grpID].pendingRequests.erase(userID);
    user_login[userID].groupID.insert(grpID);
    printUser(userID);
    return 0;
}

bool validateGroup(string grpID){
    // validate group exist or not
    if(groups_info.find(grpID) != groups_info.end()){
        return false;// group already exists
    }
    return true;
}

int createGroup(string grpID, string username){
    
    if(!validateGroup(grpID)){
        return -1;
    }

    //create group
    struct GroupDetails grpInfo;
    grpInfo.adminID = username;
    grpInfo.groupID = grpID;
    grpInfo.usernames.insert(username);
    groups_info[grpID] = grpInfo;
    user_login[username].groupID.insert(grpID);
    printUser(username);
    return 1;
}

int createUser(string userId, string password,string IP_ADDR,string PORT){
    if(user_login.find(userId) != user_login.end()){
        return 0;
    }

    struct UserDetails details;
    details.username = userId;
    details.password = password;
    //details.groupID = "NONE";
    details.IP_ADDR = IP_ADDR;
    details.PORT = stoi(PORT);
    user_login[userId] = details;

    return 1;
}

bool isMember(string grpID, string userID){
    return (groups_info[grpID].usernames.find(userID) != groups_info[grpID].usernames.end());
}

bool ValidateLogin(string userID, string password){
    if((user_login.find(userID) == user_login.end())
       || (password != user_login[userID].password)){
        return false;
    }
    return true;
}

bool isLoggedIn(string userID){
    return loggedInUsers.find(userID) != loggedInUsers.end();
}

void logout(string& username){
    loggedInUsers.erase(username);
    username = "";
}

int joinGroup(string grpID,string userID){
    if((!validateGroup(grpID)) || (userID != "")){
        groups_info[grpID].pendingRequests.insert(userID);
        return 1;
    }
    return -1;
}

int leaveGroup(string grpID,string userID){
    //validate user , group
    if(!isUser && validateGroup(grpID)){
        return -1;
    }

    //update user_Struct group , remove username from group_struct
    user_login[userID].groupID.erase(grpID);
    groups_info[grpID].usernames.erase(userID);
    printUser(userID);
    return 1;
}

string calculatSHA(string msg){
    const int len = msg.length();
    unsigned char dataC[PIECE+1];
    strcpy((char *)dataC,msg.c_str());
    unsigned char hash[SHA_DIGEST_LENGTH];
    char outputBuffer[50];
    string res_hash = "";
    unsigned char *res = SHA1(dataC,len,hash);
    for(int i=0; i<SHA_DIGEST_LENGTH; i++){
       sprintf(outputBuffer,"%02x",res[i]);
       res_hash += outputBuffer;
       memset(outputBuffer,'\0',sizeof(outputBuffer));
    }
    return res_hash;
}

int getFileSize(string filename){
  char file[1024];
  strcpy(file,filename.c_str());
  struct stat st;
  stat(file,&st);
  return st.st_size;//Bytes
}

int uploadFile(string filePath,string grpID,string userID,int sockfd){
    
    if((validateGroup(grpID)) || (!isUser(userID))){
        sendCmdStatus("group/user does not exist",sockfd);
        return -1;
    }
    
    if(!isMember(grpID,userID)){
        sendCmdStatus("You are not part of " + grpID + "  group",sockfd);
        return -1;
    }

    if(groups_info[grpID].fileDetails.find(filePath) != groups_info[grpID].fileDetails.end()){
        sendCmdStatus("File already exist in group",sockfd);
        return -1;
    }

    FILE *fp = fopen(filePath.c_str(),"rb");
    int fileSize = getFileSize(filePath);
    int numPieces = 1;

    struct fileInfo file_info;
    file_info.filename = filePath;
    file_info.fileSize = fileSize; 
    
    string fileHash = "";
    char buffer[PIECE];
    int offset = PIECE;
    while (true)
    { 
      fread(buffer, sizeof(char),PIECE,fp); 
      string hash = calculatSHA(string(buffer));
      file_info.pieces_sha[numPieces++] = hash;
      fileHash += hash;
      memset(buffer,'\0',sizeof(buffer));
      if(ftell(fp) >= fileSize)break;
    }
    for(int i=1;i<numPieces;i++){
        file_info.pieceUserMap[i].insert({0,userID});
    }
    file_info.fileSha = fileHash;
    groups_info[grpID].fileDetails[filePath] = file_info;
    fclose(fp);
    fp = NULL;
    return 1;
}
string peerSelection(int chunkNum,string filename, string grpID){
    clog<<"Selecting peer for file " + filename + " and chunk " + to_string(chunkNum)<<endl;
    struct fileInfo file_info = groups_info[grpID].fileDetails[filename];
    auto reqPeer = file_info.pieceUserMap[chunkNum].begin();
    pair<int,string> peer = *(reqPeer);
    
    string userID = peer.second;
    int requests = peer.first + 1;
    string ip = user_login[peer.second].IP_ADDR;
    int port = user_login[peer.second].PORT;

    file_info.pieceUserMap[chunkNum].erase(reqPeer);
    file_info.pieceUserMap[chunkNum].insert({requests,userID});
    clog<<"Selected Peer with ip and port : "<<ip<<":"<<port<<endl;
    return ip + ":" + to_string(port);
}

vector<string> splitString(string msg){
    vector<string> tokens;
    int len = msg.length();
    string str = "";
    for(int i=0;i<len;i++){
        if(msg[i] == ' '){
            tokens.push_back(str);
            str = "";
        }else{
            str += msg[i];
        }
    }
    tokens.push_back(str);
    return tokens;
}

void handleDownload(string grpID, string userID, string filename,int sockfd){
        char buffer[BUFFER];
        //Request parameter : piece#
        /*  
            1.update piece info  ---->  1 pieceNum
            2.request piece info <- 2 pieceNum
        */
        ssize_t read_status = read(sockfd,buffer,BUFFER); //recieve request from client for peer ip-port for download + downloade pice info
        if(read_status < 0){
            printError("Cannot read Server");
        }
        string requestMsg = string(buffer);
        vector<string> tokens = splitString(requestMsg);
        int chunkNum = stoi(tokens[1]);
        if(tokens[0] == "1"){
                clog<<"Recieved request for updating piece info from user "+ userID<<endl;
                groups_info[grpID].fileDetails[filename].pieceUserMap[chunkNum].insert({0,userID});
                sendCmdStatus(tokens[1] + " piece info updated",sockfd);
        }else{
            clog<<"Recieved request for download piece# " + to_string(chunkNum) + " of file " + filename<<endl;
            string clientmsg = peerSelection(chunkNum, filename, grpID);
            ssize_t write_status = write(sockfd,clientmsg.c_str(),clientmsg.length());
            if(write_status < 0){
                printError("Cannot write to Server");
            }
        }
}
void parseCommand(string cmd,int sockfd,string &curr_user){
    vector<string> tokens;
    // int len = cmd.length();
    // string str = "";
    // for(int i=0;i<len;i++){
    //     if(cmd[i] == ' '){
    //         tokens.push_back(str);
    //         str = "";
    //     }else{
    //         str += cmd[i];
    //     }
    // }
    // tokens.push_back(str);
    tokens = splitString(cmd);
    if(tokens[0] == "./client"){
        
    }else if(tokens[0] == "create_user"){
        if(tokens.size() < 5){
            sendCmdStatus("missing arguments",sockfd);
            return;
        }
        string userID = tokens[1];
        string passwd = tokens[2];
        string IP_ADDR = tokens[3];
        string PORT = tokens[4];
        int status = createUser(userID,passwd,IP_ADDR,PORT);
        if(status == 0){
           sendCmdStatus("user already exist",sockfd);
        }else{
            sendCmdStatus("user created successfully",sockfd);
            printUser(userID);
        }
    }else if(tokens[0] == "login"){
        if(tokens.size() < 5){
            sendCmdStatus("missing arguments",sockfd);
            return;
        }
        // check if already logged in
        string userID = tokens[1];
        string passwd = tokens[2];
        if(curr_user != ""){
            sendCmdStatus("User Already Logged In",sockfd);
        }else if(ValidateLogin(userID,passwd)){
            curr_user = userID;
            loggedInUsers.insert(userID);
            sendCmdStatus("Logged in successfully",sockfd);
            // update port and ip;
        }else{
            sendCmdStatus("Incorrect ID/Password",sockfd);
        }
    }else if(tokens[0] == "create_group"){
        if(tokens.size() < 4){
            sendCmdStatus("missing arguments",sockfd);
            return;
        }
        //Check if current user is logged in
        if(isLoggedIn(curr_user)){
            string grpID = tokens[1];
            int status = createGroup(grpID,curr_user);
            if(status == -1){
               sendCmdStatus("Group Already Exist",sockfd);
            }else{
                sendCmdStatus("Group Created Successfully",sockfd);
            }
        }else{
            sendCmdStatus("Please Log in to create group",sockfd);
        }
    }else if(tokens[0] == "join_group"){
        if(tokens.size() < 4){
            sendCmdStatus("missing arguments",sockfd);
            return;
        }
        string grpID = tokens[1];
        int status = joinGroup(grpID,curr_user);
        if(!isLoggedIn(curr_user)){
            sendCmdStatus("Login to join group",sockfd);
        }
        else if(status == -1){
            sendCmdStatus("Unable to Join group",sockfd);
        }else{
            sendCmdStatus("Group join request for " + grpID  + " sent",sockfd);
            printUser(curr_user);
        }

    }else if(tokens[0] == "leave_group"){
        if(tokens.size() < 4){
            sendCmdStatus("missing arguments",sockfd);
            return;
        }
        if(!isLoggedIn(curr_user)){
            sendCmdStatus("Please login to leave group",sockfd);
            return;
        }
        string grpID = tokens[1];
        string userID = curr_user;
        int status = leaveGroup(grpID,userID);
        if(status == -1){
            sendCmdStatus("Error leaving group",sockfd);
        }else{
            sendCmdStatus("Successfully left group " + grpID, sockfd);
        }
    }else if(tokens[0] == "list_requests"){
        string grpID = tokens[1];
        listRequests(grpID,sockfd);
    }else if(tokens[0] == "accept_request"){
        if(tokens.size() < 5){
            sendCmdStatus("missing arguments",sockfd);
            return;
        }
        string grpID = tokens[1];
        string userID = tokens[2];
        if(validateGroup(grpID)){// grp does not exist
            sendCmdStatus("Group does not exist",sockfd);
        }
        int status = acceptRequest(grpID,userID,curr_user);
        if(status == -1){
            sendCmdStatus("Accept request failed",sockfd);
        }else{
            sendCmdStatus("Request accepted for user " + userID,sockfd);
        }
    }else if(tokens[0] == "list_groups"){
        if(tokens.size() < 3){
            sendCmdStatus("missing arguments",sockfd);
            return;
        }
        listGroup(sockfd);
    }else if(tokens[0] == "list_files"){
        listShareableFiles(tokens[1]);
        sendCmdStatus("files listed",sockfd);
    }else if(tokens[0] == "upload_file"){
        if(tokens.size() < 5){//cmd,path,grpid,ip,port
            sendCmdStatus("missing arguments",sockfd);
            return;
        }if(!isLoggedIn(curr_user)){
            sendCmdStatus("No user logged in !!",sockfd);
        }else{
            string filePath = tokens[1];
            cout<<"filePath : "<<filePath<<endl;
            string grpID = tokens[2]; 
            int status = uploadFile(filePath,grpID,curr_user,sockfd);
            if(status == -1){
                sendCmdStatus("Error uploading file info",sockfd);
            }else{
                sendCmdStatus("File info uploaded successfully",sockfd);
            }
        }
    }else if(tokens[0] == "download_file"){
        if(tokens.size() < 6){
            sendCmdStatus("-1",sockfd); // missing argument
            return;
        }else{
            string grpID = tokens[1];
            string filename = tokens[2];
            string destPath = tokens[3];
            if(!isLoggedIn(curr_user)){
                sendCmdStatus("-2",sockfd); //not logged in
            }
            else if(validateGroup(grpID)){ //false if grp already exist
                sendCmdStatus("-3",sockfd);// group does not exist
            }else if(groups_info[grpID].fileDetails.find(filename) == groups_info[grpID].fileDetails.end()){
                sendCmdStatus("-4",sockfd); // file does not exist
            }else if(!isMember(grpID,curr_user)){
                sendCmdStatus("-5",sockfd); // not a member of this group
            }else{
                int fileSize = groups_info[grpID].fileDetails[filename].fileSize;
                int numChunks = ceil((1.0 * fileSize/PIECE));
                sendCmdStatus(to_string(numChunks),sockfd); // download validated
                clog<<"Entering handling download"<<endl;
                handleDownload(grpID,curr_user,filename,sockfd);
            }
        }
    }else if(tokens[0] == "logout"){
        if(tokens.size() < 3){
            sendCmdStatus("missing arguments",sockfd);
            return;
        }
        if(!isLoggedIn(curr_user)){
            sendCmdStatus("No user logged in !!",sockfd);
        }else{
            logout(curr_user);
            sendCmdStatus("logged out successfully",sockfd);
        }
    }else if(tokens[0] == "show_downloads"){

    }else if(tokens[0] == "stop_sharing"){

    }else{
        sendCmdStatus("Invalid Command",sockfd);
    }
}

void *handleRequests(void *sockfd){
    int acceptfd = *((int *)sockfd);
    free(sockfd);
    char sendmsg[MSG_SIZE] = {0};
    string msg = "Hello from server" , curr_user = "";
    strcpy(sendmsg,msg.c_str());
  while(true){    
        char buffer[BUFFER];
        memset(buffer,'\0',sizeof(buffer));
        ssize_t read_status = read(acceptfd, buffer, BUFFER);

        if (read_status < 0){
                printError("Cannot read from Client");
        }
        cout << "Message from client: " << buffer << endl;

        parseCommand(string(buffer),acceptfd,curr_user);
    }
    return NULL;
}

int main(int argc, char const *argv[])
{   
    // string tracker_info = argv[1];
    // int tracker_no = atoi(argv[2]);

    // freopen(tracker_info.c_str(),"r",stdin);
    // string ip, port;
    // cin>>ip>>port;


    struct sockaddr_in serverSockAddress, clientSockAddress;
    socklen_t serverAddrSize = sizeof(serverSockAddress);
    socklen_t clientAddrSize = sizeof(serverSockAddress);
    char buffer[BUFFER] = {0};

    int opt = 1;
    if (argc < 2)
    {
        printError("no port provided");
    }
    //Create Socket()
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printError("Unable to create Socket");
    }

    int port_no = atoi(argv[1]);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))){
        printError("setsockopt");
    }

    memset(&serverSockAddress, '\0', sizeof(serverSockAddress)); // zero structure out
    serverSockAddress.sin_family = AF_INET;                      // match the socket() call
    serverSockAddress.sin_addr.s_addr = INADDR_ANY; //127.2.1.1             // bind to any local address
    serverSockAddress.sin_port = htons(port_no);                 // specify port to listen on

    // int inet_status = inet_pton(AF_INET,ip.c_str(),&serverSockAddress.sin_addr);
    // if(inet_status < 0){
    //     printError("inet address not supported");
    // }

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

    cout<<"LISTENING\n";
    while(true){
        int acceptfd = accept(sockfd, (struct sockaddr *)&clientSockAddress, &clientAddrSize);
        
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
    return 0;
}