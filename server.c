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

#define PORTNUM 8080
#define BUFF_LMT 1024
#define sourcePath "/home/bhatt6b/Desktop"
#define IS_TEXT_RESPONSE 1

int cmdCase;
char textResponse[1024];
char **textResponseArray;
int indexCntForTextResponse = 0;
int responseType;

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

    return 0;
}

// This function is used by the nftw() to traverse all the file in the path.
static int traverse(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
    if (cmdCase == 0)
    {
        if (tflag == FTW_D)
        {
            textResponseArray[indexCntForTextResponse] = strdup(fpath + ftwbuf->base);
            indexCntForTextResponse++;
        }
    }

    return 0;
}

void handleClientRequest(char *cmd)
{
    if (strstr(cmd, "dirlist") != NULL)
    {
        cmdCase = 0;
        indexCntForTextResponse = 0;
        textResponseArray = (char **)malloc(1024 * sizeof(char *));
        if (textResponseArray == NULL)
        {
            perror("Error in allocating memory dynamically");
            exit(1);
        }

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
        free(textResponseArray);
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
    handleClientRequest("dirlist -a");
}
