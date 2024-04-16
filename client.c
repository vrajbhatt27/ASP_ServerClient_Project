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
#include <stdbool.h>
#include <ctype.h>

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
const char *knownExtensions[] = {
    "txt", "pdf", "doc", "docx", "xls", "xlsx",
    "jpg", "jpeg", "png", "gif", "bmp",
    "cpp", "c", "h", "java", "py",
    "mp3", "wav", "mp4", "avi", "mkv",
    "zip", "rar", "7z", "tar", "gz", "xslt", "tar.gz", "tar"};
const int knownExtensionsCount = sizeof(knownExtensions) / sizeof(knownExtensions[0]);

// Function to validate if the file extension is known and properly formatted
bool isValidExtension(const char *ext, bool requireDot)
{
    if (ext == NULL || *(ext + (requireDot ? 1 : 0)) == '\0')
    {
        return false;
    }

    if (requireDot && *ext != '.')
    {
        return false; // Return false if a dot is required but not present
    }

    if (*ext == '.')
    {
        ext++; // Skip the dot if present, regardless of requirement
    }

    // Check against known extensions
    for (int i = 0; i < knownExtensionsCount; i++)
    {
        if (strcasecmp(ext, knownExtensions[i]) == 0)
        {
            return true; // Extension is recognized
        }
    }
    return false; // No valid extension found
}

// Function to validate if a filename is properly formatted for Linux and has a valid extension
bool isValidFilename(const char *filename)
{
    const char *ext; // Pointer to the file extension part

    if (filename == NULL)
    {
        return false;
    }
    if (*filename == '-')
    { // Check if the filename starts with a dash
        return false;
    }

    ext = strrchr(filename, '.'); // Find the last dot in filename to assume it's the extension start
    if (ext == NULL || !isValidExtension(ext, true))
    {
        return false; // No extension or invalid extension
    }

    // Validate the filename part before the extension
    while (*filename && filename != ext)
    {
        if (*filename == '/' || *filename == '\0' || (unsigned char)*filename < ' ' ||
            *filename == ':' || *filename == '*' || *filename == '?' || *filename == '"' ||
            *filename == '<' || *filename == '>' || *filename == '|')
        {
            return false;
        }
        filename++;
    }

    return true;
}

// Function to check if the provided date is in YYYY-MM-DD format
bool isValidDate(const char *date)
{
    int year, month, day;
    if (date == NULL)
    {
        return false;
    }
    if (strlen(date) != 10)
    {
        return false;
    }
    if (date[4] != '-' || date[7] != '-')
    {
        return false;
    }
    // Parsing year, month, day
    char yearStr[5], monthStr[3], dayStr[3];
    strncpy(yearStr, date, 4);
    yearStr[4] = '\0';
    strncpy(monthStr, date + 5, 2);
    monthStr[2] = '\0';
    strncpy(dayStr, date + 8, 2);
    dayStr[2] = '\0';

    year = atoi(yearStr);
    month = atoi(monthStr);
    day = atoi(dayStr);

    // Check year range
    if (year < 1900 || year > 3000)
        return false;

    // Check month range
    if (month < 1 || month > 12)
        return false;

    // Check day range
    if (day < 1 || day > 31)
        return false;
    if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30)
        return false;
    if (month == 2)
    {
        // Check for leap year
        bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if (day > (isLeap ? 29 : 28))
            return false;
    }
    return true;
}

bool checkCommandSyntax(char *command)
{
    char *cmd, *arg1, *arg2, *endptr;
    char *temp = strdup(command); // Create a modifiable copy of the command
    bool isValid = true;          // Assume the command is valid unless proven otherwise
    cmd = strtok(temp, " ");      // Extract the command part
    char dateInput[11];           // Buffer for user input

    if (cmd == NULL)
    {
        printf("No command entered. Please enter one to proceed\n");
        isValid = false;
    }

    if (strcmp(cmd, "dirlist") == 0)
    {
        arg1 = strtok(NULL, " ");
        if (arg1 == NULL || (strcmp(arg1, "-a") != 0 && strcmp(arg1, "-t") != 0))
        {
            printf("Invalid argument for dirlist. Use 'dirlist -a' or 'dirlist -t'\n");
            isValid = false;
        }
    }
    else if (strcmp(cmd, "w24fn") == 0)
    {
        arg1 = strtok(NULL, " ");
        if (arg1 == NULL)
        {
            printf("Filename not specified. Use 'w24fn filename'\n");
            isValid = false;
        }
        else if (!isValidFilename(arg1))
        {
            printf("Invalid filename '%s'. Filenames should not start with '-', contain '/', or any control characters and should have proper extension with it.\n", arg1);
            isValid = false;
        }
    }
    else if (strcmp(cmd, "w24fz") == 0)
    {
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");
        long size1 = strtol(arg1, &endptr, 10);
        long size2 = (arg2 != NULL) ? strtol(arg2, &endptr, 10) : -1;
        if (arg1 == NULL || arg2 == NULL)
        {
            printf("Size range not fully specified. Use 'w24fz size1 size2'\n");
            isValid = false;
        }
        else if (*endptr != '\0' || atoi(arg1) < 0 || atoi(arg2) < 0 || atoi(arg1) > atoi(arg2))
        {
            printf("Invalid size range. Ensure 0 <= size1 <= size2 and that size values are valid integers.\n");
            isValid = false;
        }
    }
    else if (strcmp(cmd, "w24ft") == 0)
    {
        arg1 = strtok(NULL, " ");
        if (arg1 == NULL)
        {
            printf("At least one file extension must be specified. Use 'w24ft ext1 [ext2 ext3]'\n");
            isValid = false;
        }
        else
        {
            int count = 0;

            while (arg1 != NULL)
            {
                if (count >= 3)
                {
                    printf("Too many file extensions. Specify up to 3 file types\n");
                    isValid = false;
                    break; // Exit as soon as more than 3 extensions are processed
                }

                if (!isValidExtension(arg1, false))
                {
                    printf("Extension '%s' is not recognized. Please use a valid extension.\n", arg1);
                    isValid = false;
                    break; // Exit on the first invalid extension
                }
                count++;
                arg1 = strtok(NULL, " "); // Get the next extension
            }
        }
    }
    else if (strcmp(cmd, "w24fdb") == 0 || strcmp(cmd, "w24fda") == 0)
    {
        arg1 = strtok(NULL, " ");
        if (arg1 == NULL)
        {
            printf("Date not specified. Use '%s YYYY-MM-DD'\n", cmd);
            isValid = false;
        }
        else if (!isValidDate(arg1))
        {
            printf("Invalid date format '%s'. Please use YYYY-MM-DD format.\n", arg1);
            isValid = false;
        }
    }
    else if (strstr(command, "quitc"))
    {
        isValid = true;
    }
    else
    {
        printf("Invalid command\n");
        isValid = false;
    }

    free(temp); // Clean up the duplicated string
    return isValid;
}

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

        if (!checkCommandSyntax(clientRequest))
        {
            if (strcmp(clientRequest, "quitc") == 0)
            {
                break;
            }
            continue;
        }

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