#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


// define globals 
#define h_addr h_addr_list[0]
#define BUFFER_SIZE 1024
#define MAX_FILE_SIZE 65507

// function declerations
void get_res(int socket, struct sockaddr_in *clientaddr, char *filename);
void put_res(int socket, struct sockaddr_in *clientaddr, char *filename, char *filedata);
void delete_res(int socket, struct sockaddr_in *clientaddr, char *filename);
void ls_res(int socket, struct sockaddr_in *clientaddr);


int main(int argc, char **argv) {
    int  n, port_num, socket_num, optval, clientlen;
    struct sockaddr_in serveraddr, clientaddr;
    struct hostent *hostp;
    char buff[BUFFER_SIZE], filename[BUFFER_SIZE], command[BUFFER_SIZE], 
        filedata[MAX_FILE_SIZE] ,*hostaddrp;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port_num = atoi(argv[1]);
    
    
    /* create parent socket */
    if ( (socket_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Error opening socket. Exiting...");
        exit(1);
    }

    // setsockopt: configure socket to reuse address
    optval = 1;
    setsockopt(socket_num, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    /* build server's internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port_num);

    /* bind parent socket */
      if (bind(socket_num, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
          printf("Error binding the socket. Exiting...");
          exit(1);
      }

    /* main loop for servicing requests to server */
    clientlen = sizeof(clientaddr);

    while(1) {
        // reset buffer to zero
        bzero(buff, BUFFER_SIZE);
        bzero(filedata, MAX_FILE_SIZE);
        bzero(command, BUFFER_SIZE);

        /* recieve data */
        n = recvfrom(socket_num, buff, BUFFER_SIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0){
            printf("ERROR in recvfrom");
        }
        sscanf(buff, "%s %s %[\001-\377]", command, filename, filedata);

        /* get host address */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL) printf("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL) printf("ERROR on inet_ntoa\n");

        printf("server received command \"%s\" from %s (%s)\n", command, hostp->h_name, hostaddrp);

        // handle the request
        if (!strcmp(command, "ls")){
            ls_res(socket_num, &clientaddr);
        }
        else if (!strcmp(command, "get")) {
            get_res(socket_num, &clientaddr, filename);
        }
        else if (!strcmp(command, "put")) {
            put_res(socket_num, &clientaddr, filename, filedata);
        }
        else if (!strcmp(command, "delete")) {
            delete_res(socket_num, &clientaddr, filename);
        }
        else if (!strcmp(command, "exit") ) {
            printf("Exiting...\n");
            n = sendto(socket_num, "exit", BUFFER_SIZE, 0, (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0) printf("ERROR in sendto");
            exit(0);
        }
        else {
            sprintf(buff, "Command \"%s\" not found or not implemented", command);
            n = sendto(socket_num, buff, BUFFER_SIZE, 0, (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0) printf("ERROR in sendto");
        }     
    }
    return 0;
}
/*********************************************************************************/
/********************* implementations for each command **************************/

/************************* handle get command from client **************************/
void get_res(int socket, struct sockaddr_in *clientaddr, char *filename) {
    int n, clientlen, filelen;
    FILE *fp;
    clientlen = sizeof(*clientaddr);

    // make sure the file exists
    if  ( !(fp = fopen(filename, "r")) )  {
        n = sendto(socket, "DNE", MAX_FILE_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
        if (n < 0) printf("ERROR in sendto");
        return;
    }

    // read the file into buff
    fseek (fp, 0, SEEK_END);
    filelen = ftell(fp);
    fseek (fp, 0, SEEK_SET);
    char *buff[filelen];
    if (buff){
        fread (buff, 1, filelen, fp);
    }
    fclose (fp);

    // don't send if file is larger than udp max size
    if (filelen > MAX_FILE_SIZE) {
        printf("File to big to send\n");
        n = sendto(socket,"TBTG" , MAX_FILE_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
        if (n < 0) printf("ERROR in sendto");
        return;
    }
    
    // send contens of file in buff
    n = sendto(socket, buff, MAX_FILE_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
    if (n < 0) printf("ERROR in sendto");
}


/************************* handle put command from client **************************/
void put_res(int socket, struct sockaddr_in *clientaddr, char *filename, char *filedata) {
    int n, clientlen;
    FILE *fp;
    clientlen = sizeof(*clientaddr);
    char buff[BUFFER_SIZE];

    // open a new file and write to it
    fp = fopen(filename, "w");
    fprintf(fp, "%s", filedata);
    fclose(fp);

    // send message
    sprintf( buff, "\"%s\" file has been put to server", filename);
    n = sendto(socket, buff, BUFFER_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
    if (n < 0) printf("ERROR in sendto\n");
}

/************************* handle delete command from client **************************/
void delete_res(int socket, struct sockaddr_in *clientaddr, char *filename) {
    int n, clientlen;
    FILE *fp;
    clientlen = sizeof(*clientaddr);
    char finalcommand[BUFFER_SIZE] = "rm ";
    char buff[BUFFER_SIZE];
    // TODO
    // make sure you dont delete the uftp_server file, or makefile, or executable
    if (!strcmp(filename,"uftp_server.c")){
        n = sendto(socket, "don't delete the server code!", BUFFER_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
        if (n < 0) printf("ERROR in sendto");
        return;
    }
    // make sure the file exists
    if ((fp = fopen(filename, "r"))==NULL )  {
        n = sendto(socket, "DNE", BUFFER_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
        if (n < 0) printf("ERROR in sendto");
        return;
    }
    fclose(fp);
    // create command and delete
    strcat(finalcommand, filename);
    if (system(finalcommand)) {
        n = sendto(socket, "DNE", BUFFER_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
        if (n < 0) printf("ERROR in sendto");
    }

    sprintf(buff, "file \"%s\" deleted", filename);
    // send message back
    n = sendto(socket, buff, BUFFER_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
    if (n < 0) printf("ERROR in sendto");
}

/************************* handle ls command **************************/
void ls_res(int socket, struct sockaddr_in *clientaddr) {
    int n, clientlen;
    FILE *fp;
    char res[BUFFER_SIZE], buff[BUFFER_SIZE];
    clientlen = sizeof(*clientaddr);

    // run ls command and read result into response
    fp = popen("/bin/ls .", "r");
    while (fgets(res, sizeof(res), fp) != NULL) {
        strcat(buff, res);
    }
    fclose(fp);

    // send respone
    n = sendto(socket, buff, BUFFER_SIZE, 0, (struct sockaddr *) clientaddr, clientlen);
    if (n < 0) printf("ERROR in sendto");
}