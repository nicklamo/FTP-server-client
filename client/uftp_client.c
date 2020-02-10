#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>
#include <errno.h>

// define globals 
#define h_addr h_addr_list[0]
#define BUFFER_SIZE 1024
#define MAX_FILE_SIZE 65507
#define SLEEP_TIME 1

// declare functions
void req_get(int socket, struct sockaddr_in *serveraddr);
void req_put(int socket, struct sockaddr_in *serveraddr);
void req_delete(int socket, struct sockaddr_in *serveraddr);
void req_ls(int socket, struct sockaddr_in *serveraddr);
void req_exit(int socket, struct sockaddr_in *serveraddr);
void printmenu();


int main(int argc, char **argv) {
    // variables
    char line[BUFFER_SIZE];
    char input[BUFFER_SIZE];
    int socket_num;
    struct hostent *server;
    struct sockaddr_in serveraddr;

    /* read command line args */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    int port_num = atoi(argv[2]);
    char *hostname = argv[1];
    printf("Port Number = %d, hostname = %s\n", port_num, hostname);

    /* create socket*/
    if ( (socket_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Error opening socket. Exiting ...");
        exit(1);
    }
    // set socket to non-blocking
    fcntl(socket_num, F_SETFL, O_NONBLOCK);

    // get server by hostname
    if ((server = gethostbyname(hostname)) == NULL) {
        printf("Bad hostname: %s\n", hostname);
        exit(0);
    }

    /* build the server's internet address */
    // zero out serveraddr struct
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    // copy address into serveraddr struct
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(port_num);

    /* get user input and make call to appropriate subroutine */
    while (1){
        printmenu();
        fgets(line, sizeof(line), stdin);
        sscanf(line, "%s", input);

        if (!strcmp(input, "get")) {
            req_get(socket_num , &serveraddr);
        }
        else if (!strcmp(input, "put")) {
            req_put(socket_num , &serveraddr);
        }
        else if (!strcmp(input, "delete")) {
            req_delete(socket_num , &serveraddr);
        }
        else if (!strcmp(input, "ls")) {
            req_ls( socket_num, &serveraddr);
        }
        else if (!strcmp(input, "exit")) {
            req_exit(socket_num, &serveraddr);
        }
        else { // send over unknown command
            int serverlen = sizeof(serveraddr);
            int n;
            n = sendto(socket_num, input, BUFFER_SIZE, 0, (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) printf("ERROR in sendto");
            n = recvfrom(socket_num, input, BUFFER_SIZE, 0, (struct sockaddr * )&serveraddr, &serverlen);
            if (n < 0) printf("ERROR in recvfrom");
            printf("res from server: %s", input);
        }
    }
    return 0;
}
/*****************************************************************************/
/*********************** Function Implementations ****************************/
/******************* routines for every client command ***********************/

/***************************** GET COMMAND ***********************************/
void req_get(int socket, struct sockaddr_in *serveraddr) {
    int n, serverlen;
    char buff[MAX_FILE_SIZE], filename[32], line[32];
    FILE *fp;
    // get filename
    printf("Enter a file to get: ");
    fgets(line, sizeof(line), stdin);
    sscanf(line, "%s", filename);

    /* send the message to the server */
    char *tmp = strdup("get ");
    strcat(tmp, filename);
    strcpy(buff, tmp);
    serverlen = sizeof(*serveraddr);
    n = sendto(socket, buff, MAX_FILE_SIZE, 0, (struct sockaddr *) serveraddr, serverlen);
    if (n < 0) {
      printf("ERROR in sendto");
      return;
    }

    // sleep for 1 second to wait for reply
    bzero(buff, MAX_FILE_SIZE);
    sleep(SLEEP_TIME);
    
    // process server reply
    n = recvfrom(socket, buff, MAX_FILE_SIZE, 0, (struct sockaddr * )serveraddr, &serverlen);
    if (n < 0) {
        printf("ERROR in recvfrom, errno = %d", errno);
        return;
    }
    if (!strcmp(buff, "DNE")){
        printf("Response from server: The file %s does not exist.\n", filename);
    }
    else if (!strcmp(buff, "TBTG")){ 
        printf("The file %s is too big to get.\n", filename);
    }
    else {
        // create file 
        fp = fopen(filename, "w");
        fprintf(fp, "%s", buff);
        fclose(fp);
        printf("Now have file \"%s\"\n", filename);
    }
}

/***************************** PUT COMMAND ***********************************/
void req_put(int socket, struct sockaddr_in *serveraddr){
    int n, serverlen, filelen;
    FILE *fp;
    char buff[MAX_FILE_SIZE], res[BUFFER_SIZE], filename[32], line[32];
    //get filename
    printf("Enter a file to put: ");
    fgets(line, sizeof(line), stdin);
    sscanf(line, "%s", filename);

    // make sure file exists
    if ( (fp=fopen(filename, "r")) == NULL) {
        printf("The file %s does not exist\n", filename);
        return;
    }
    
    // read command into buff
    strcat(buff, "put ");
    strcat(buff, filename);
    strcat(buff, " ");

    //read file
    fseek (fp, 0, SEEK_END);
    filelen = ftell(fp);
    fseek (fp, 0, SEEK_SET);
    char tmp[filelen];
    if (tmp){
        fread(tmp, 1, filelen, fp);
    }
    fclose(fp);
    if (filelen > MAX_FILE_SIZE) {
        printf("File to big to put to server\n");
        return;
    }

    // put file into buffer
    strcat(buff, tmp);
    // send file to the server
    serverlen = sizeof(*serveraddr);
    n = sendto(socket, buff, MAX_FILE_SIZE, 0, (struct sockaddr *) serveraddr, serverlen);
    if (n < 0) {
      printf("ERROR in sendto");
      return;
    }
    // sleep for 1 second to wait for reply
    sleep(SLEEP_TIME);

    bzero(buff, MAX_FILE_SIZE);

    /* print the server's reply */
    n = recvfrom(socket, res, BUFFER_SIZE, 0, (struct sockaddr * )serveraddr, &serverlen);
    if (n < 0) {
        printf("ERROR in recvfrom, errno = %d", errno);
        return;
    }
    printf("res from server: %s \n", res);
}

/***************************** DELETE COMMAND ***********************************/
void req_delete(int socket, struct sockaddr_in *serveraddr){
    int n, serverlen;
    char buff[BUFFER_SIZE], filename[32], line[32];
    // get file name to delete
    printf("Enter a file to delete: ");
    fgets(line, sizeof(line), stdin);
    sscanf(line, "%s", filename);

    /* send the message to the server */
    char *tmp = strdup("delete ");
    strcat(tmp, filename);
    strcpy(buff, tmp);
    printf("Command: %s \n", buff);
    serverlen = sizeof(*serveraddr);
    n = sendto(socket, buff, BUFFER_SIZE, 0, (struct sockaddr *) serveraddr, serverlen);
    if (n < 0) {
      printf("ERROR in sendto");
      return;
    }
    bzero(buff, BUFFER_SIZE);
    // sleep for 1 second to wait for reply
    sleep(1);
    /* print the server's reply */
    n = recvfrom(socket, buff, BUFFER_SIZE, 0, (struct sockaddr * )serveraddr, &serverlen);
    if (n < 0) {
        printf("ERROR in recvfrom, errno = %d", errno);
        return;
    }
    if (!strcmp(buff, "DNE")){
        printf("The file %s does not exists \n", filename);
    }
    else { 
        printf("Res from server: %s \n", buff);
    }
}

/***************************** LS COMMAND ***********************************/
void req_ls(int socket, struct sockaddr_in *serveraddr) {
    int n, serverlen;
    char buff[BUFFER_SIZE];

    /* send the message to the server */
    strcpy(buff, "ls");
    serverlen = sizeof(*serveraddr);
    n = sendto(socket, buff, BUFFER_SIZE, 0, (struct sockaddr *) serveraddr, serverlen);
    if (n < 0) {
      printf("ERROR in sendto");
      return;
    }
    bzero(buff, BUFFER_SIZE);
    // sleep for 1 second to wait for reply
    sleep(1);
    /* print the server's reply */
    n = recvfrom(socket, buff, BUFFER_SIZE, 0, (struct sockaddr * )serveraddr, &serverlen);
    if (n < 0) {
        printf("ERROR in recvfrom, errno = %d", errno);
        return;
    }
    printf("Res from server:\n%s", buff);
}

/******************************* EXIT COMMAND ************************************/
void req_exit(int socket, struct sockaddr_in *serveraddr){
    // send the command to exit
    int n;
    int serverlen = sizeof(*serveraddr);
    char buff[BUFFER_SIZE];
    n = sendto(socket, "exit", BUFFER_SIZE, 0, (struct sockaddr * )serveraddr, serverlen);
    if (n < 0) {
        printf("ERROR in sendto");
    }
    bzero(buff, BUFFER_SIZE);
    // sleep for 1 second to wait for reply
    sleep(1);
    n = recvfrom(socket, buff, BUFFER_SIZE, 0, (struct sockaddr * )serveraddr, &serverlen);
    if (n < 0) {
        printf("ERROR in recvfrom server hasn't exited, errno = %d", errno);
    }
    else {
        printf("Goodbye...\n");
        exit(0);
    }
}

/******************************* PRINT MENU *******************************************/
void printmenu() {
    printf("\n============= CLIENT MENU =============\n");
    printf("Options: get | put | delete | ls | exit \n");
    printf("Enter Option: ");
}