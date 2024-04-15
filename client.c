#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define PORTNUM 7654
#define BUFF_LMT 1024
#define IS_TEXT_RESPONSE 1
#define IS_FILE_RESPONSE 2
#define TAR_FILE_DIR "/home/ubuntu/w24project/"
#define TAR_FILE_PATH "/home/ubuntu/w24project/temp.tar.gz"

int connectToServer(const char *ipAddr, int port);
void processClientResponse(int csd);
void processFileResponse(int csd, long int totalBytesToRead);
int receiveData(int csd, char *serverResponse, int totalBytesToRead, int readInChunks);
void requestToMirror(int mirrorNumber, char *ipAddr, int portNumber);

int responseType;

int receiveData(int csd, char *serverResponse, int totalBytesToRead, int readInChunks)
{
    if (readInChunks)
    {
        int bytes_read = 0;
        int total_bytes_read = 0;
        int readBytes = 10;

        memset(serverResponse, 0, sizeof(serverResponse));
        char buffer[readBytes];
        while (total_bytes_read <= totalBytesToRead)
        {
            memset(buffer, 0, sizeof(buffer));
            bytes_read = recv(csd, buffer, readBytes, 0);
            if (bytes_read < 0)
            {
                perror("Error reading server response");
                return -1;
            }
            else if (bytes_read == 0)
            {
                printf("Server closed connection.\n");
                return -1;
            }
            buffer[bytes_read] = '\0';
            strcat(serverResponse, buffer);
            total_bytes_read += bytes_read;
        }
        return 0;
    }
    else
    {
        memset(serverResponse, 0, sizeof(serverResponse));
        int bytes_read = recv(csd, serverResponse, totalBytesToRead, 0);
        if (bytes_read < 0)
        {
            perror("Error reading server response");
            return -1;
        }
        else if (bytes_read == 0)
        {
            printf("Server closed connection.\n");
            return -1;
        }
        serverResponse[bytes_read] = '\0';
        return 0;
    }
}

void processFileResponse(int csd, long int totalBytesToRead)
{
    if (access(TAR_FILE_DIR, F_OK) == -1)
    {
        mkdir(TAR_FILE_DIR, 0777);
    }

    // printf("HERE--2\n");
    int fd = open(TAR_FILE_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("Error Creating file");
        return;
    }

    int bytes_read = 0;
    int total_bytes_read = 0;
    int readBytes = 512;

    char buffer[readBytes];
    // printf("Total Bytes To Read: %ld\n", totalBytesToRead);
    while (total_bytes_read < totalBytesToRead)
    {
        memset(buffer, 0, sizeof(buffer));
        bytes_read = recv(csd, buffer, readBytes, 0);
        // printf("Here --3: Received %d bytes\n", bytes_read);
        if (bytes_read < 0)
        {
            perror("Error reading server response");
            return;
        }
        else if (bytes_read == 0)
        {
            printf("Server closed connection.\n");
            return;
        }
        write(fd, buffer, bytes_read);
        total_bytes_read += bytes_read;
        // printf("Here --4: Total %d bytes\n", total_bytes_read);
    }

    close(fd);
}

void requestToMirror(int mirrorNumber, char *ipAddr, int portNumber)
{
    int csd = connectToServer(ipAddr, portNumber);
    if (csd < 0)
    {
        printf("Failed to connect to server.\n");
        return;
    }

    processClientResponse(csd);

    close(csd);
}

void processClientResponse(int csd)
{
    printf("Connected successfully to the server!\n");

    char serverResponse[BUFF_LMT];
    if (receiveData(csd, serverResponse, sizeof(serverResponse), 0) < 0)
    {
        return;
    }

    // Handle Mirror
    if (serverResponse[0] == '*')
    {
        close(csd);
        int mirrorNumber = 0;
        char ipAddr[16];
        int portNumber = 0;
        sscanf(serverResponse, "*%d/%d/%s", &mirrorNumber, &portNumber, ipAddr);
        requestToMirror(mirrorNumber, ipAddr, portNumber);
    }

    printf("****************************************\n");
    printf("Server's response:\n%s\n", serverResponse);
    printf("****************************************\n");

    while (1)
    {
        char clientRequest[BUFF_LMT];
        printf("\n$ ");
        if (fgets(clientRequest, sizeof(clientRequest), stdin) == NULL)
        {
            perror("Error reading user input");
            return;
        }
        clientRequest[strcspn(clientRequest, "\n")] = '\0';

        if (send(csd, clientRequest, strlen(clientRequest), 0) < 0)
        {
            perror("Error sending client request");
            return;
        }

        if (strcmp(clientRequest, "quitc") == 0)
        {
            break;
        }

        if (receiveData(csd, serverResponse, sizeof(serverResponse), 0) < 0)
        {
            return;
        }

        if (serverResponse[0] == '*')
        {
            long int responseRecvSize = 0;
            int rtype = 0;
            sscanf(serverResponse, "*%d/%ld", &rtype, &responseRecvSize);
            responseType = rtype == 1 ? IS_TEXT_RESPONSE : 0;
            char *response = "H_OK";

            if (send(csd, response, strlen(response), 0) < 0)
            {
                perror("Error sending client request");
                return;
            }

            if (responseType == IS_TEXT_RESPONSE)
            {
                if (receiveData(csd, serverResponse, responseRecvSize, 1) < 0)
                {
                    return;
                }
                printf("%s", serverResponse);
            }
            else
            {
                // printf("HERE--1\n");
                processFileResponse(csd, responseRecvSize);
                printf("File received successfully.\n");
            }
        }
    }
}

int connectToServer(const char *ipAddr, int port)
{
    int portNum = port == 0 ? PORTNUM : port;
    int csd = socket(AF_INET, SOCK_STREAM, 0);
    if (csd < 0)
    {
        perror("Error in socket generation");
        return -1;
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(portNum);

    if (inet_pton(AF_INET, ipAddr, &serverAddress.sin_addr) <= 0)
    {
        perror("Invalid Address or Address not supported");
        close(csd); // Close the socket before returning
        return -1;
    }

    if (connect(csd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("Failed in establishing connection");
        close(csd); // Close the socket before returning
        return -1;
    }

    return csd;
}

int main(int argc, char *argv[])
{
    char ipAddr[16];

    if (argc < 2)
    {
        printf("Synopsis: %s <ipAddr>\n", argv[0]);
        return 1;
    }

    strcpy(ipAddr, argv[1]);

    int csd = connectToServer(ipAddr, 0);
    if (csd < 0)
    {
        printf("Failed to connect to server.\n");
        return 1;
    }

    processClientResponse(csd);

    close(csd);
    return 0;
}