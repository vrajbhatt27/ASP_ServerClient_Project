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
#define sourcePath "/home/ubuntu/Desktop"
#define tarFilePath "/var/tmp/tarFilesStorageDirectory"
#define IS_TEXT_RESPONSE 1

char filename[256];
char textResponse[1024];
char *textResponseArray[1024];
int indexCntForTextResponse = 0;
char tar_command[1024];
char ext1[10], ext2[10], ext3[10];
char date[15];
int cmdCase;
int size1, size2;
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

void execTarCommand(char *tar_command)
{
    char command[2024];
    strcpy(command, "tar -cf ");
    strcat(command, tarFilePath);
    strcat(command, "/temp.tar.gz");
    strcat(command, " ");
    strcat(command, tar_command);
    strcat(command, " 2>/dev/null");

    system(command);
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
    case 2:
        if (tflag == FTW_F)
        {
            if (sb->st_size >= size1 && sb->st_size <= size2)
            {
                // printf("%s\n", fpath + ftwbuf->base);
                fileFound = 1;
                strcat(tar_command, " ");
                strcat(tar_command, fpath);
            }
        }
        break;
    case 3:
        if (tflag == FTW_F)
        {
            char *extension = strrchr(fpath + ftwbuf->base, '.');
            if (extension != NULL)
            {
                extension++;
                if (strcmp(extension, ext1) == 0 || strcmp(extension, ext2) == 0 || strcmp(extension, ext3) == 0)
                {
                    fileFound = 1;
                    strcat(tar_command, " ");
                    strcat(tar_command, fpath);
                }
            }
        }
        break;
    case 4:
        if (tflag == FTW_F)
        {
            struct tm *time = localtime(&sb->st_ctime);
            char temp[15];
            strftime(temp, 15, "%Y-%m-%d", time);
            if (strcmp(temp, date) <= 0)
            {
                fileFound = 1;
                strcat(tar_command, " ");
                strcat(tar_command, fpath);
            }
        }
        break;
    case 5:
        if (tflag == FTW_F)
        {
            struct tm *time = localtime(&sb->st_ctime);
            char temp[15];
            strftime(temp, 15, "%Y-%m-%d", time);
            if (strcmp(temp, date) >= 0)
            {
                fileFound = 1;
                strcat(tar_command, " ");
                strcat(tar_command, fpath);
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
    else if (strstr(cmd, "w24fz") != NULL)
    {
        cmdCase = 2;
        fileFound = 0;
        memset(tar_command, 0, sizeof(tar_command));
        sscanf(cmd, "%*s %d %d", &size1, &size2);

        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        if (fileFound == 0)
        {
            strcpy(textResponse, "No files found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }
        execTarCommand(tar_command);
    }
    else if (strstr(cmd, "w24ft") != NULL)
    {
        cmdCase = 3;
        fileFound = 0;
        memset(tar_command, 0, sizeof(tar_command));
        memset(ext1, 0, sizeof(ext1));
        memset(ext2, 0, sizeof(ext2));
        memset(ext3, 0, sizeof(ext3));

        sscanf(cmd, "%*s %s %s %s", ext1, ext2, ext3);

        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        if (fileFound == 0)
        {
            strcpy(textResponse, "No files found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }

        execTarCommand(tar_command);
    }
    else if (strstr(cmd, "w24fdb") != NULL)
    {
        cmdCase = 4;
        fileFound = 0;
        memset(tar_command, 0, sizeof(tar_command));
        memset(date, 0, sizeof(date));

        sscanf(cmd, "%*s %s", date);

        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        if (fileFound == 0)
        {
            strcpy(textResponse, "No files found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }

        execTarCommand(tar_command);
    }
    else if (strstr(cmd, "w24fda") != NULL)
    {
        cmdCase = 5;
        fileFound = 0;
        memset(tar_command, 0, sizeof(tar_command));
        memset(date, 0, sizeof(date));

        sscanf(cmd, "%*s %s", date);

        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        if (fileFound == 0)
        {
            strcpy(textResponse, "No files found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }

        execTarCommand(tar_command);
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

int main2(int argc, char *argv[])
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

int main()
{
    handleClientRequest("w24fdb 2024-04-14");
}
