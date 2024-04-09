#define _XOPEN_SOURCE 1
#define _XOPEN_SOURCE_EXTENDED 1
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ftw.h>
#include <time.h>

#define PORTNUM 7654
#define BUFF_LMT 1024
#define sourcePath "/home/bhatt6b/Desktop"
#define IS_TEXT_RESPONSE 1

int cmdCase;
char filename[256];
char textResponse[1024];
char *textResponseArray[1024];
int indexCntForTextResponse = 0;
int responseType;
int fileFound;

int receiveData(int csd, char *buffer, int bufferSize)
{
    memset(buffer, 0, sizeof(buffer));
    int bytes_read = recv(csd, buffer, bufferSize, 0);
    if (bytes_read < 0)
    {
        perror("Error reading from client");
        return -1;
    }
    if (bytes_read == 0)
    {
        printf("Client has disconnected\n");
        return -1;
    }
    buffer[bytes_read] = '\0';
    return 0;
}

int sendData(int csd, char *data)
{
    if (send(csd, data, strlen(data), 0) < 0)
    {
        perror("Error sending data to client");
        return -1;
    }
    memset(data, 0, strlen(data));
    return 0;
}

// This function is used by the nftw() to traverse all the file in the path.
static int traverse(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
    switch (cmdCase)
    {
    case 0:
        if (tflag == FTW_D)
        {
            textResponseArray[indexCntForTextResponse] = strdup(fpath + ftwbuf->base);
            indexCntForTextResponse++;
        }
        break;
    case 1:
        if (tflag == FTW_F)
        {
            if (strcmp(fpath + ftwbuf->base, filename) == 0)
            {
                fileFound = 1;
                textResponseArray[0] = "File Name: ";
                textResponseArray[1] = strdup(fpath + ftwbuf->base);

                char temp[50];
                sprintf(temp, "%ld", sb->st_size);
                textResponseArray[2] = "File Size: ";
                textResponseArray[3] = strdup(temp);

                memset(temp, 0, sizeof(temp));
                sprintf(temp, "%o", sb->st_mode);
                textResponseArray[4] = "File Permission: ";
                textResponseArray[5] = strdup(temp);

                textResponseArray[6] = "Date Created: ";
                textResponseArray[7] = ctime(&sb->st_ctime);
                return 1;
            }
        }
        break;
    }

    return 0;
}

void handleClientRequest(char *cmd)
{
    if (strstr(cmd, "dirlist") != NULL)
    {
        cmdCase = 0;
        indexCntForTextResponse = 0;
        memset(textResponseArray, 0, sizeof(textResponseArray));

        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        // Sorting the array in alphabetical order
        if (strstr(cmd, "-a") != NULL)
        {
            for (int i = 0; i < indexCntForTextResponse - 1; i++)
            {
                for (int j = 0; j < indexCntForTextResponse - i - 1; j++)
                {
                    if (strcmp(textResponseArray[j], textResponseArray[j + 1]) > 0)
                    {
                        char *temp = textResponseArray[j];
                        textResponseArray[j] = textResponseArray[j + 1];
                        textResponseArray[j + 1] = temp;
                    }
                }
            }
        }

        // Create a single string from the array
        strcpy(textResponse, "");
        for (int i = 0; i < indexCntForTextResponse; i++)
        {
            strcat(textResponse, textResponseArray[i]);
            if (i != indexCntForTextResponse - 1)
                strcat(textResponse, "\n");
        }

        responseType = IS_TEXT_RESPONSE;
    }
    else if (strstr(cmd, "w24fn") != NULL)
    {
        cmdCase = 1;
        fileFound = 0;
        indexCntForTextResponse = 0;
        memset(textResponseArray, 0, sizeof(textResponseArray));

        sscanf(cmd, "%*s %s", filename);
        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        if (fileFound == 0)
        {
            strcpy(textResponse, "File not found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }

        // Create a single string from the array
        strcpy(textResponse, "");
        for (int i = 0; i < 8; i++)
        {
            strcat(textResponse, textResponseArray[i]);
            if (i != 7 && i % 2 != 0)
                strcat(textResponse, "\n");
        }

        responseType = IS_TEXT_RESPONSE;
    }
}

void textResponseHandler(int csd)
{
    // Sending Header First: resonseType/lengthOfResponse
    char header[10];
    sprintf(header, "*%d/%ld", responseType, strlen(textResponse));
    if (sendData(csd, header) < 0)
    {
        return;
    }

    // sending data to client
    if (sendData(csd, textResponse) < 0)
    {
        return;
    }
}

void crequest(int csd)
{
    char cmd[BUFF_LMT];
    strcpy(cmd, "Connected Successfully\nEnter Commands below:");

    if (sendData(csd, cmd) < 0)
    {
        return;
    }

    while (1)
    {
        char clientRequest[BUFF_LMT];
        if (receiveData(csd, clientRequest, sizeof(clientRequest)) < 0)
        {
            return;
        }
        printf("Client: %s\n", clientRequest);

        handleClientRequest(clientRequest);
        if (responseType == IS_TEXT_RESPONSE)
        {
            textResponseHandler(csd);
        }
    }
}

int main(int argc, char *argv[])
{
    int sd, csd, portNumber, status;
    socklen_t len;
    struct sockaddr_in servAdd;

    // Establish Socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Error creating socket");
        exit(1);
    }

    servAdd.sin_family = AF_INET;
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY);
    servAdd.sin_port = htons(PORTNUM);

    // bind socket
    if (bind(sd, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0)
    {
        perror("Error binding socket");
        exit(1);
    }

    // listen socket
    if (listen(sd, 5) < 0)
    {
        perror("Error listening on socket");
        exit(1);
    }

    printf("Active on port: %d\n", PORTNUM);
    while (1)
    {
        csd = accept(sd, (struct sockaddr *)NULL, NULL);
        if (csd < 0)
        {
            perror("Error accepting connection");
            continue; // continue waiting for next connection
        }
        printf("Client has connected Successfully\n");

        // child proces will redirect client
        if (!fork())
        {
            crequest(csd);
            close(csd);
            exit(0);
        }
        waitpid(0, &status, WNOHANG);
    }
}

int main2()
{
    handleClientRequest("w24fn z.txt");
}
