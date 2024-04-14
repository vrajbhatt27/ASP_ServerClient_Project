#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <arpa/inet.h>

#define PORTNUM 7656
#define BUFF_LMT 1024
#define IS_TEXT_RESPONSE 1

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

void handleServerResponse(char *serverResponse)
{
}

void processClientResponse(int csd)
{
    printf("Connected successfully to the server!\n");

    char serverResponse[BUFF_LMT];
    if (receiveData(csd, serverResponse, sizeof(serverResponse), 0) < 0)
    {
        return;
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

        if (receiveData(csd, serverResponse, sizeof(serverResponse), 0) < 0)
        {
            return;
        }

        if (serverResponse[0] == '*')
        {
            int textResponseRecvSize = 0;
            int rtype = 0;
            sscanf(serverResponse, "*%d/%d", &rtype, &textResponseRecvSize);
            responseType = rtype == 1 ? IS_TEXT_RESPONSE : 0;
            char *response = "H_OK";

            if (send(csd, response, strlen(response), 0) < 0)
            {
                perror("Error sending client request");
                return;
            }

            if (receiveData(csd, serverResponse, textResponseRecvSize, 1) < 0)
            {
                return;
            }
            printf("%s", serverResponse);
        }
    }
}

int connectToServer(const char *ipAddr)
{
    int csd = socket(AF_INET, SOCK_STREAM, 0);
    if (csd < 0)
    {
        perror("Error in socket generation");
        return -1;
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORTNUM);

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

    int csd = connectToServer(ipAddr);
    if (csd < 0)
    {
        printf("Failed to connect to server.\n");
        return 1;
    }

    processClientResponse(csd);

    close(csd);
    return 0;
}