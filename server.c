// Used Professor Ranga's Code
// Required Libraries
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
#include <fcntl.h>

// Macros Used
#define PORTNUM 7654
#define BUFF_LMT 1024
#define sourcePath "/home/ubuntu"
#define tarFilePath "/var/tmp/tarFilesStorageDirectory"
#define IS_TEXT_RESPONSE 1
#define IS_FILE_RESPONSE 2
#define CON_SERVER 0
#define CON_MIRROR_1 1
#define CON_MIRROR_2 2
#define MIRROR_1_PORT 7655
#define MIRROR_2_PORT 7656
#define MIRROR_IP_ADDRESS "127.0.0.1"

// Directory Structure for sorting
typedef struct
{
    char name[1024];
    time_t ctime;
} Directory;

// Global Variables
Directory *directories = NULL;
int count = 0;
int size = 0;

char filename[256];
char textResponse[1024];
char *textResponseArray[1024];
int indexCntForTextResponse = 0;
char fileResponse[100];
char tar_command[1024];
char ext1[10], ext2[10], ext3[10];
char date[15];
int cmdCase;
int size1, size2;
int responseType;
int fileFound;
int clientNumber = 0;

// Function to receive data from client
// Function to receive data from client
int receiveData(int csd, char *buffer, int bufferSize)
{
    // reset buffer
    memset(buffer, 0, sizeof(buffer));
    // receive data from client
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

// Function to send data to client
int sendData(int csd, char *data, int sendInChunks)
{
    // Sending data in chunks
    if (sendInChunks)
    {
        int sendBytes = 10;
        int totalBytes = strlen(data);
        int bytesSent = 0;
        // calculating total bytes sent
        while (totalBytes > 0)
        {
            // sending data to client
            bytesSent += send(csd, data + bytesSent, sendBytes, 0);
            if (bytesSent < 0)
            {
                perror("Error sending data to client");
                return -1;
            }
            // calculating remaining bytes
            totalBytes -= sendBytes;
        }
    }
    else
    {
        // sending data to client normally
        if (send(csd, data, strlen(data), 0) < 0)
        {
            perror("Error sending data to client");
            return -1;
        }
    }
    // reset data
    memset(data, 0, strlen(data));
    return 0;
}

// Function to execute tar command
char *execTarCommand(char *tar_command)
{
    // Create a directory to store tar files
    if (access(tarFilePath, F_OK) == -1)
    {
        mkdir(tarFilePath, 0777);
    }

    // Preparing tar command
    char command[2024];
    strcpy(command, "tar -cf ");
    strcat(command, tarFilePath);
    strcat(command, "/temp.tar.gz");
    strcat(command, " ");
    strcat(command, tar_command);
    strcat(command, " 2>/dev/null");

    // Executing tar command
    if (system(command) != 0)
    {
        perror("Error executing tar command");
        return NULL;
    }

    // return tar file path
    return "/var/tmp/tarFilesStorageDirectory/temp.tar.gz";
}

// Compare function for qsort
int compare_directories(const void *a, const void *b)
{
    // Cast to Directory type
    const Directory *dir1 = (const Directory *)a;
    const Directory *dir2 = (const Directory *)b;
    // Sort by creation time
    return (dir1->ctime - dir2->ctime);
}

// Function to add directory to the array
void add_directory(const char *name, const struct stat *sb)
{
    // Allocate more memory if necessary
    if (count >= size)
    {
        // Double the size of the array
        int newSize = size ? size * 2 : 10; // Initially 10, double when full
        Directory *temp = realloc(directories, newSize * sizeof(Directory));
        if (temp == NULL)
        {
            fprintf(stderr, "Memory allocation failed\n");
            return;
        }
        // Update the array and size
        directories = temp;
        size = newSize;
    }

    // Copy the directory name and creation time
    if (strlen(name) >= sizeof(directories[count].name))
    {
        fprintf(stderr, "Directory name too long to copy: %s\n", name);
        return; // Skip this directory or handle error as appropriate
    }

    // Copy the directory name and creation time
    strcpy(directories[count].name, name);
    directories[count].ctime = sb->st_ctime;
    count++;
}

// This function is used by the nftw() to traverse all the file in the path.
static int traverse(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
    // switch case to handle different commands
    switch (cmdCase)
    {
        // Case 0: dirlist -a
    case -1:
        if (tflag == FTW_D)
        {
            if (tflag == FTW_D && ftwbuf->level == 1)
            {
                char *dName = strdup(fpath + ftwbuf->base);
                add_directory(dName, sb);
            }
        }
        break;
        // Case 0: dirlist -a
    case 0:
        if (tflag == FTW_D)
        {
            if (tflag == FTW_D && ftwbuf->level == 1)
            {
                textResponseArray[indexCntForTextResponse] = strdup(fpath + ftwbuf->base);
                indexCntForTextResponse++;
            }
        }
        break;
        // Case 1: w24fn
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
        // Case 2: w24fz
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
        // Case 3: w24ft
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
        // Case 4: w24fdb
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
        // Case 5: w24fda
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
        // Default Case
    }
    return 0;
}

// Function to handle client request
void handleClientRequest(char *cmd)
{
    // Switch case to handle different commands
    if (strstr(cmd, "dirlist") != NULL)
    {
        // Check if the command is -a or -t
        cmdCase = 0;
        if (strstr(cmd, "-a") != NULL)
        {
            cmdCase = 0;
        }
        else if (strstr(cmd, "-t") != NULL)
        {
            cmdCase = -1;
        }

        // Resetting the variables
        indexCntForTextResponse = 0;
        memset(textResponseArray, 0, sizeof(textResponseArray));

        // Traverse the source path
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

            // Create a single string from the array
            strcpy(textResponse, "");
            for (int i = 0; i < indexCntForTextResponse; i++)
            {
                strcat(textResponse, textResponseArray[i]);
                strcat(textResponse, "\n");
            }
        }

        // Sorting the array by creation time
        if (strstr(cmd, "-t") != NULL)
        {
            // Sort the directories by creation time
            qsort(directories, count, sizeof(Directory), compare_directories);
            // Output sorted directories
            strcpy(textResponse, "");
            for (int i = 0; i < count; i++)
            {
                strcat(textResponse, directories[i].name);
                strcat(textResponse, "\n");
            }

            count = 0;
        }

        // Setting the response type
        responseType = IS_TEXT_RESPONSE;
    }
    else if (strstr(cmd, "w24fn") != NULL)
    {
        // Check if the command is -a or -t
        cmdCase = 1;
        fileFound = 0;
        indexCntForTextResponse = 0;
        memset(textResponseArray, 0, sizeof(textResponseArray));

        // Get the filename
        sscanf(cmd, "%*s %s", filename);
        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        // Check if the file is found
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

        // Setting the response type
        responseType = IS_TEXT_RESPONSE;
    }
    else if (strstr(cmd, "w24fz") != NULL)
    {
        //
        cmdCase = 2;
        fileFound = 0;
        memset(tar_command, 0, sizeof(tar_command));
        sscanf(cmd, "%*s %d %d", &size1, &size2);

        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        // Check if the file is found
        if (fileFound == 0)
        {
            strcpy(textResponse, "No files found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }

        // Execute the tar command
        char *res = execTarCommand(tar_command);
        if (res != NULL)
        {
            strcpy(fileResponse, res);
            responseType = IS_FILE_RESPONSE;
        }
    }
    else if (strstr(cmd, "w24ft") != NULL)
    {
        // Check if the command is -a or -t
        cmdCase = 3;
        // Resetting the variables
        fileFound = 0;
        // Resetting the variables
        memset(tar_command, 0, sizeof(tar_command));
        memset(ext1, 0, sizeof(ext1));
        memset(ext2, 0, sizeof(ext2));
        memset(ext3, 0, sizeof(ext3));

        //  Get the extensions and store them in the variables
        sscanf(cmd, "%*s %s %s %s", ext1, ext2, ext3);

        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        // Check if the file is found
        if (fileFound == 0)
        {
            strcpy(textResponse, "No files found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }

        char *res = execTarCommand(tar_command);
        if (res != NULL)
        {
            strcpy(fileResponse, res);
            responseType = IS_FILE_RESPONSE;
        }
    }
    else if (strstr(cmd, "w24fdb") != NULL)
    {
        // Check if the command is -a or -t
        cmdCase = 4;
        fileFound = 0;
        memset(tar_command, 0, sizeof(tar_command));
        memset(date, 0, sizeof(date));

        // Get the date and store it in the variable
        sscanf(cmd, "%*s %s", date);

        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        // Check if the file is found
        if (fileFound == 0)
        {
            strcpy(textResponse, "No files found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }

        // Execute the tar command
        char *res = execTarCommand(tar_command);
        if (res != NULL)
        {
            strcpy(fileResponse, res);
            responseType = IS_FILE_RESPONSE;
        }
    }
    else if (strstr(cmd, "w24fda") != NULL)
    {
        // Check if the command is -a or -t
        cmdCase = 5;
        fileFound = 0;
        memset(tar_command, 0, sizeof(tar_command));
        memset(date, 0, sizeof(date));

        // Get the date and store it in the variable
        sscanf(cmd, "%*s %s", date);

        // Traverse the source path
        if (nftw(sourcePath, traverse, 20, FTW_PHYS) == -1)
        {
            perror("nftw");
            exit(1);
        }

        // Check if the file is found
        if (fileFound == 0)
        {
            strcpy(textResponse, "No files found");
            responseType = IS_TEXT_RESPONSE;
            return;
        }

        // Execute the tar command
        char *res = execTarCommand(tar_command);
        if (res != NULL)
        {
            strcpy(fileResponse, res);
            responseType = IS_FILE_RESPONSE;
        }
    }
}

void textResponseHandler(int csd)
{
    // Sending Header First: resonseType/lengthOfResponse
    char header[10];
    sprintf(header, "*%d/%ld", responseType, strlen(textResponse));
    if (sendData(csd, header, 0) < 0)
    {
        return;
    }

    // Getting Client Response (H_OK)
    char clientRequest[BUFF_LMT];
    if (receiveData(csd, clientRequest, sizeof(clientRequest)) < 0)
    {
        return;
    }
    printf("Client Header Response: %s\n", clientRequest);

    // sending data to client if client request is OK
    if (strcmp(clientRequest, "H_OK") == 0)
    {
        if (sendData(csd, textResponse, 1) < 0)
        {
            return;
        }
    }
}

void fileResponseHandler(int csd)
{
    // Open the file
    int fd = open(fileResponse, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr, "File open operation failed !\n");
        exit(1);
    }

    // Getting File Size
    long int fileSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    printf("File Size: %ld\n", fileSize);
    // Sending Header First: resonseType/lengthOfResponse
    char header[10];
    sprintf(header, "*%d/%ld", responseType, fileSize);
    if (sendData(csd, header, 0) < 0)
    {
        return;
    }

    // Getting Client Response (H_OK)
    char clientRequest[BUFF_LMT];
    if (receiveData(csd, clientRequest, sizeof(clientRequest)) < 0)
    {
        return;
    }
    printf("Client Header Response: %s\n", clientRequest);

    // sending data to client if client request is OK
    if (strcmp(clientRequest, "H_OK") == 0)
    {
        // printf("HERE--1\n");
        int readBytes = 512;
        char readBuffer[readBytes];
        int totalBytesRead = 0;
        int bytesRead = 0;
        while (totalBytesRead < fileSize)
        {
            memset(readBuffer, 0, sizeof(readBuffer));
            bytesRead = read(fd, readBuffer, sizeof(readBuffer));
            if (send(csd, readBuffer, bytesRead, 0) < 0)
            {
                perror("Error sending data to client");
                return;
            }
            // printf("HERE -- 3: Bytes Read: %d\n", bytesRead);
            totalBytesRead += bytesRead;
            // printf("HERE -- 4: Total Bytes Read: %d\n", totalBytesRead);
        }
        printf("File Sent Successfully\n");
    }

    close(fd);
}

// Function to handle client request
void crequest(int csd)
{
    // Sending Connection Success Message
    char cmd[BUFF_LMT];
    strcpy(cmd, "Connected Successfully\nEnter Commands below:");

    if (sendData(csd, cmd, 0) < 0)
    {
        return;
    }

    // Receiving Client Request in loop
    while (1)
    {
        // Getting Client Request
        char clientRequest[BUFF_LMT];

        if (receiveData(csd, clientRequest, sizeof(clientRequest)) < 0)
        {
            return;
        }
        printf("Client: %s\n", clientRequest);

        // Check if client has disconnected
        if (strcmp(clientRequest, "quitc") == 0)
        {
            printf("Client Has Disconnected Successfully\n");
            close(csd);
            return;
        }

        // Handle Client Request
        handleClientRequest(clientRequest);
        if (responseType == IS_TEXT_RESPONSE)
        {
            textResponseHandler(csd);
        }
        else
        {
            // strcpy(fileResponse, "/var/tmp/tarFilesStorageDirectory/temp.txt");
            fileResponseHandler(csd);
        }
    }
}

// Function to send client to mirror
void sendToMirror(int csd, int servName)
{
    // Sending Header First: resonseType/lengthOfResponse
    int port = (servName == CON_MIRROR_1) ? MIRROR_1_PORT : MIRROR_2_PORT;
    char header[30];
    // mirrorNumber/portNumber/ipAddress
    sprintf(header, "*%d/%d/%s", servName, port, MIRROR_IP_ADDRESS);
    if (sendData(csd, header, 0) < 0)
    {
        return;
    }
    printf("Client Forwarded to Mirror %d Successfully.\n", servName);
}

// Function to load balance the clients
int loadBalancing(int clientNum)
{
    static int i = -1;
    if (clientNum <= 3)
    {
        return CON_SERVER;
    }
    else if (clientNum >= 4 && clientNum <= 6)
    {
        return CON_MIRROR_1;
    }
    else if (clientNum >= 7 && clientNum <= 9)
    {
        return CON_MIRROR_2;
    }
    else
    {
        i++;
        // Consecutively allocating the clients to the servers and mirrors
        return i % 3;
    }
}

int main(int argc, char *argv[])
{
    int sd, csd, portNumber, status;
    // struct sockaddr_in servAdd;
    socklen_t len;
    struct sockaddr_in servAdd;

    // Establish Socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Error creating socket");
        exit(1);
    }

    // Initialize socket
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
        // accept connection
        csd = accept(sd, (struct sockaddr *)NULL, NULL);
        if (csd < 0)
        {
            perror("Error accepting connection");
            continue; // continue waiting for next connection
        }

        // Incrementing the client number
        clientNumber += 1;
        printf("Client %d has connected Successfully\n", clientNumber);

        // Load Balancing
        int serverNameToConnect = loadBalancing(clientNumber);

        // Forking the process and checking for the server to connect
        if (serverNameToConnect == CON_MIRROR_1)
        {
            if (!fork())
            {
                sendToMirror(csd, CON_MIRROR_1);
                close(csd);
                exit(0);
            }
            waitpid(0, &status, WNOHANG);
        }
        else if (serverNameToConnect == CON_MIRROR_2)
        {
            if (!fork())
            {
                sendToMirror(csd, CON_MIRROR_2);
                close(csd);
                exit(0);
            }
            waitpid(0, &status, WNOHANG);
        }
        else
        {
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
}