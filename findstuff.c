#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <bits/sigaction.h>

#define blue "\x1b[34m"
#define normal "\x1b[0m"

int fd[2];
int reading;

/* Function declarations */
void ffid(const char *, char *, int, char *, struct timeval);                                              /* find file in directory */
void ffis(const char *, char *, int, char *, struct timeval);                                              /* find file in subdirectories too */
void ffishelper(const char *, const char *, char *, int *);                                /* helper to recursively search subdirectories */
void ftid(const char *, char *, char *, int, int, char *, struct timeval);                                 /* find text in directory */
void ftis(const char *, const char *, const char *, int, int, const char *, struct timeval);               /* find text in subdirectories */
void ftishelper(const char *, const char *, const char *, int, int, const char *, char *); /* helper to recursively search subdirectories */

void signalhandler(int);
void removequotes(char *);

int main()
{
    int i;
    int j;
    int pid;
    struct stat filestat;
    char input[1000] = "";
    char command[256] = "";
    char argument[512] = "";
    char* argumentq;
    int *children;
    char **tasks;
    int *nchild;
    int serial;
    size_t arglen;
    char currdir[256] = "";
    struct sigaction new_action, old_action;
    int save_stdi;
    char buffer[256] = "";
    char inputtype[256] = "";
    char *extracted;
    char *colon;
    char *dashs;
    int len;
    int status;
    struct timeval starttime;
    int waitstat;

    argumentq = NULL;
    extracted = NULL;
    colon = NULL;
    dashs = NULL;
    inputtype[0] = '.';

    save_stdi = dup(STDIN_FILENO);

    new_action.sa_handler = signalhandler;
    sigemptyset(&new_action.sa_mask);
    sigaddset(&new_action.sa_mask, SIGTERM);
    new_action.sa_flags = 0;
    sigaction(SIGUSR1, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGUSR1, &new_action, NULL);
    }

    if (pipe(fd) == -1)
    { /* create pipe */
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    children = mmap(NULL, 10 * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | 0x20, -1, 0);
    tasks = (char **)mmap(NULL, 10 * sizeof(char *), PROT_READ | PROT_WRITE, MAP_SHARED | 0x20, -1, 0);
    nchild = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | 0x20, -1, 0);

    while (1)
    {
        for (i = 0; i < 10; i++)
        {
            if (children[i] != 0)
            {
                waitstat = waitpid(children[i], &status, WNOHANG);
                if(waitstat != 0){
                    children[i] = 0;
                }
            }
        }
        getcwd(currdir, sizeof(currdir));
        printf(blue "findstuff$ " normal);
        fflush(0);
        read(STDIN_FILENO, input, 1000);
        if (reading == 1)
        {
            read(STDIN_FILENO, input, 1000);
            fprintf(stderr, "%s\n", input);
            memset(input, 0, strlen(input));
            reading = 0;
            dup2(save_stdi, STDIN_FILENO);
        }
        else
        {
            fflush(0);
            sscanf(input, "%s %s", command, argument);
            if (strcmp(command, "find") == 0)
            {
                if (strlen(argument) == 0)
                {
                    printf("Enter something to search for.\n");
                    continue;
                }
                else if (*nchild < 10)
                {
                    *nchild = *nchild + 1;
                    if (fork() == 0)
                    {
                        gettimeofday(&starttime, NULL);
                        for (i = 0; i < 10; i++)
                        {
                            if (children[i] == 0)
                            {
                                children[i] = getpid();
                                tasks[i] = argument;
                                break;
                            }
                        }
                        arglen = strlen(argument); /* surrounded by double quotes search for text*/
                        if (arglen > 2 && argument[0] == '"' && argument[arglen - 1] == '"')
                        {
                            removequotes(argument);
                            /* printf("%s\n", argument); */
                            if (strstr(input, " -f") != NULL)
                            { /* if f flag set, only search certain file types */
                                if (strstr(input, " -s") != NULL)
                                { 
                                    colon = strchr(input, ':');
                                    extracted = colon + 1;
                                    for (j = 0; extracted[j] != ' ' && extracted[j] != '\0'; j++)
                                    {
                                    }
                                    extracted[j] = '\0';
                                    snprintf(inputtype, sizeof(inputtype), ".%s", extracted);
                                    ftis(currdir, argument, inputtype, 1, (i + 1), tasks[i], starttime);
                                }
                                else
                                { 
                                    colon = strchr(input, ':');
                                    extracted = colon + 1;
                                    snprintf(inputtype, sizeof(inputtype), ".%s", extracted);
                                    len = strlen(inputtype);
                                    inputtype[len - 1] = '\0';
                                    ftid(currdir, argument, inputtype, 1, (i + 1), tasks[i], starttime);
                                }
                            }
                            else
                            { /* f flag not set */
                                if (strstr(input, " -s") != NULL)
                                { /* if s flag set, search all directories */
                                    ftis(currdir, argument, "", 0, (i + 1), tasks[i], starttime);
                                }
                                else
                                {
                                    ftid(currdir, argument, "", 0, (i + 1), tasks[i], starttime);
                                }
                            }
                        }
                        else
                        { /* search for file */
                            if (strstr(input, " -s") != NULL)
                            { /* if s flag set, search all directories */
                                ffis(currdir, argument, (i + 1), tasks[i], starttime);
                            }
                            else
                            {
                                ffid(currdir, argument, (i + 1), tasks[i], starttime);
                            }
                        }
                        memset(argument, 0, strlen(argument));
                        tasks[i] = NULL;
                        *nchild = *nchild - 1;
                        exit(0); /* end child */
                    }
                }
                else
                {
                    printf("Unable to create child. Already at max children: 10.\n");
                }
            }
            else if (strcmp(command, "list") == 0)
            {
                for (i = 0; i < 10; i++)
                {
                    if (children[i] != 0)
                    {
                    waitstat = waitpid(children[i], &status, WNOHANG);
                    if(waitstat != 0){
                        children[i] = 0;
                    }
                    }
                }
                printf("Running child processes:\n");
                for (i = 0; i < 10; i++)
                {
                    if (children[i] != 0)
                    {
                        printf("Child: %d PID: %d Currently finding %s\n", (i + 1), children[i], tasks[i]);
                    }
                }
            }
            else if (strcmp(command, "kill") == 0)
            {
                serial = atoi(argument);
                if (serial < 1 || serial > 10)
                {
                    printf("Invalid child. Number must be between 1 and 10.\n");
                }
                if (children[serial - 1] == 0)
                {
                    printf("Child %d not currently active.\n", serial);
                }
                else
                {
                    kill(children[serial - 1], SIGKILL);
                    tasks[serial - 1] = NULL;
                    *nchild = *nchild - 1;
                    printf("Child %d has been killed.\n", serial);
                }
            }
            else if ((strcmp(command, "q") == 0) || strcmp(command, "quit") == 0)
            {
                /* quit program and all child processes immediately */
                for (i = 0; i < 10; i++)
                {
                    if (children[i] != 0)
                    {
                        kill(children[i], SIGKILL);
                    }
                }
                /* free allocated mem */
                munmap(children, 10 * sizeof(int));
                munmap(tasks, 10 * sizeof(char *));
                munlock(nchild, sizeof(int));
                wait(0);
                return 0;
            }
            else
            {
                printf("Invalid Input\n");
            }
        }
    }
    return 0;
}

void ffid(const char *location, char *argument, int childnum, char *childtask, struct timeval starttime)
{
    int found;
    char path[512];
    DIR *directory;
    struct dirent *entry;
    char result[1000] = "";
    char numstring[3];
    struct timeval endtime;
    double elapsedtime;
    int hours, minutes, seconds, milliseconds, totalseconds;
    char hourstring[3];
    char minstring[3];
    char secstring[3];
    char msecstring[3];

    sprintf(numstring, "%d", childnum);
    strcat(result, "\nChild ");
    strcat(result, numstring);
    strcat(result, " finished finding ");
    strcat(result, childtask);
    strcat(result, ".\n");

    found = 0;
    directory = opendir(location);
    getcwd(path, sizeof(path));
    if (directory == NULL)
    {
        printf("Cannot open directory.\n");
        exit(1);
    }
    while ((entry = readdir(directory)) != NULL)
    {
        if (strcmp(entry->d_name, argument) == 0)
        {
            strcat(path, "/");
            strcat(path, entry->d_name);
            strcat(path, "\n");
            strcat(result, path);
            found += 1;
        }
    }
    if (found == 0)
    {
        strcat(result, "Nothing found.\n");
    }
    /*sleep(60);*/
    gettimeofday(&endtime, NULL);
    elapsedtime = (endtime.tv_sec - starttime.tv_sec) * 1000.0;
    elapsedtime += (endtime.tv_usec - starttime.tv_usec) / 1000.0; 
    totalseconds = (int)(elapsedtime / 1000);
    hours = totalseconds / 3600;
    minutes = (totalseconds % 3600) / 60;
    seconds = totalseconds % 60;
    milliseconds = (int)(elapsedtime) % 1000;

    sprintf(hourstring, "%d", hours);
    sprintf(minstring, "%d", minutes);
    sprintf(secstring, "%d", seconds);
    sprintf(msecstring, "%d", milliseconds);

    strcat(result, "Elapsed time: ");
    strcat(result, hourstring);
    strcat(result, ":");
    strcat(result, minstring);
    strcat(result, ":");
    strcat(result, secstring);
    strcat(result, ":");
    strcat(result, msecstring);
    strcat(result, "\n");
 
    closedir(directory);
    close(fd[0]);
    write(fd[1], result, 1000);
    close(fd[1]);
    kill(getppid(), SIGUSR1);
}

void ffishelper(const char *dir_path, const char *argument, char *result, int *found)
{
    DIR *directory;
    struct dirent *entry;
    char path[512];
    struct stat statfile;

    directory = opendir(dir_path);
    if (directory == NULL)
    {
        return;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (stat(path, &statfile) == 0 && S_ISDIR(statfile.st_mode))
        {
            ffishelper(path, argument, result, found);
        }
        else if (strcmp(entry->d_name, argument) == 0)
        {
            strcat(result, path);
            strcat(result, "\n");
            (*found)++;
        }
    }
    closedir(directory);
}

void ffis(const char *location, char *argument, int childnum, char *childtask, struct timeval starttime)
{
    int found;
    char numstring[3];
    char result[1000] = "";
    struct timeval endtime;
    double elapsedtime;
    int hours, minutes, seconds, milliseconds, totalseconds;
    char hourstring[3];
    char minstring[3];
    char secstring[3];
    char msecstring[3];

    sprintf(numstring, "%d", childnum);
    strcat(result, "\nChild ");
    strcat(result, numstring);
    strcat(result, " finished finding ");
    strcat(result, childtask);
    strcat(result, ".\n");

    found = 0;
    ffishelper(location, argument, result, &found);

    if (found == 0)
    {
        strcat(result, "Nothing found.\n");
    }

    gettimeofday(&endtime, NULL);
    elapsedtime = (endtime.tv_sec - starttime.tv_sec) * 1000.0;
    elapsedtime += (endtime.tv_usec - starttime.tv_usec) / 1000.0; 
    totalseconds = (int)(elapsedtime / 1000);
    hours = totalseconds / 3600;
    minutes = (totalseconds % 3600) / 60;
    seconds = totalseconds % 60;
    milliseconds = (int)(elapsedtime) % 1000;

    sprintf(hourstring, "%d", hours);
    sprintf(minstring, "%d", minutes);
    sprintf(secstring, "%d", seconds);
    sprintf(msecstring, "%d", milliseconds);

    strcat(result, "Elapsed time: ");
    strcat(result, hourstring);
    strcat(result, ":");
    strcat(result, minstring);
    strcat(result, ":");
    strcat(result, secstring);
    strcat(result, ":");
    strcat(result, msecstring);
    strcat(result, "\n");
    close(fd[0]);
    write(fd[1], result, 1000);
    close(fd[1]);
    kill(getppid(), SIGUSR1);
}

void ftid(const char *location, char *argument, char *filetype, int f, int childnum, char *childtask, struct timeval starttime)
{
    int found;
    char path[512];
    DIR *directory;
    struct dirent *entry;
    struct stat filestat;
    char result[1000] = "";
    FILE *file;
    char buffer[256];
    int typelen;
    int namelen;
    char *fileend;
    char numstring[3];
    struct timeval endtime;
    double elapsedtime;
    int hours, minutes, seconds, milliseconds, totalseconds;
    char hourstring[3];
    char minstring[3];
    char secstring[3];
    char msecstring[3];

    sprintf(numstring, "%d", childnum);
    strcat(result, "\nChild ");
    strcat(result, numstring);
    strcat(result, " finished finding ");
    strcat(result, childtask);
    strcat(result, ".\n");

    getcwd(path, sizeof(path));
    found = 0;
    directory = opendir(location);
    if (directory == NULL)
    {
        printf("Cannot open directory.\n");
        exit(1);
    }

    typelen = strlen(filetype);

    while ((entry = readdir(directory)) != NULL)
    { /* iterate through directory */
        snprintf(path, sizeof(path), "%s/%s", location, entry->d_name);
        if (f == 1)
        {
            namelen = strlen(entry->d_name);
            if (namelen >= typelen)
            {
                fileend = &entry->d_name[namelen - typelen];
            }
            if (strcmp(fileend, filetype) == 0)
            {
                if (stat(path, &filestat) == 0 && S_ISREG(filestat.st_mode)) /* find readable files */
                {
                    file = fopen(entry->d_name, "r"); /* open file */
                    if (file == NULL)
                    {
                        printf("Can't open file: %s\n", entry->d_name);
                        continue;
                    }
                    while (fgets(buffer, sizeof(buffer), file) != NULL)
                    {
                        if (strstr(buffer, argument) != NULL)
                        {
                            strcat(result, path);
                            strcat(result, "\n");
                            found++;
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            if (stat(path, &filestat) == 0 && S_ISREG(filestat.st_mode)) /* find readable files */
            {
                file = fopen(entry->d_name, "r"); /* open file */
                if (file == NULL)
                {
                    fprintf(stderr, "Can't open file: %s\n", entry->d_name);
                    continue;
                }
                while (fgets(buffer, sizeof(buffer), file) != NULL)
                {
                    if (strstr(buffer, argument) != NULL)
                    {
                        strcat(result, path);
                        strcat(result, "\n");
                        found++;
                        break;
                    }
                }
            }
        }
    }
    if (found == 0)
    {
        strcat(result, "Nothing found.\n");
    }
    gettimeofday(&endtime, NULL);
    elapsedtime = (endtime.tv_sec - starttime.tv_sec) * 1000.0;
    elapsedtime += (endtime.tv_usec - starttime.tv_usec) / 1000.0; 
    totalseconds = (int)(elapsedtime / 1000);
    hours = totalseconds / 3600;
    minutes = (totalseconds % 3600) / 60;
    seconds = totalseconds % 60;
    milliseconds = (int)(elapsedtime) % 1000;

    sprintf(hourstring, "%d", hours);
    sprintf(minstring, "%d", minutes);
    sprintf(secstring, "%d", seconds);
    sprintf(msecstring, "%d", milliseconds);

    strcat(result, "Elapsed time: ");
    strcat(result, hourstring);
    strcat(result, ":");
    strcat(result, minstring);
    strcat(result, ":");
    strcat(result, secstring);
    strcat(result, ":");
    strcat(result, msecstring);
    strcat(result, "\n");
    closedir(directory);
    close(fd[0]);
    write(fd[1], result, 1000);
    close(fd[1]);
    kill(getppid(), SIGUSR1);
}

void ftishelper(const char *location, const char *argument, const char *filetype, int f, int childnum, const char *childtask, char *result)
{
    DIR *directory;
    struct dirent *entry;
    struct stat filestat;
    char path[512];
    char *fileend;
    FILE *file;
    char buffer[256];
    int namelen;
    int typelen;

    typelen = strlen(filetype);
    directory = opendir(location);
    if (directory == NULL)
    {
        printf("Cannot open directory.\n");
        exit(1);
    }

    while ((entry = readdir(directory)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", location, entry->d_name);
        if (stat(path, &filestat) == 0 && S_ISDIR(filestat.st_mode))
        {
            ftishelper(path, argument, filetype, f, childnum, childtask, result);
        }
        else if (f == 1)
        {
            namelen = strlen(entry->d_name);
            if (namelen >= typelen)
            {
                fileend = &entry->d_name[namelen - typelen];
            }
            if (strcmp(fileend, filetype) == 0)
            {
                if (stat(path, &filestat) == 0 && S_ISREG(filestat.st_mode))
                {
                    file = fopen(path, "r");
                    if (file == NULL)
                    {
                        printf("Can't open file: %s\n", path);
                        continue;
                    }
                    while (fgets(buffer, sizeof(buffer), file) != NULL)
                    {
                        if (strstr(buffer, argument) != NULL)
                        {
                            strcat(result, path);
                            strcat(result, "\n");
                            break;
                        }
                    }
                    fclose(file);
                }
            }
        }
        else
        {
            if (stat(path, &filestat) == 0 && S_ISREG(filestat.st_mode))
            {
                file = fopen(path, "r");
                if (file == NULL)
                {
                    printf("Can't open file: %s\n", path);
                    continue;
                }
                while (fgets(buffer, sizeof(buffer), file) != NULL)
                {
                    if (strstr(buffer, argument) != NULL)
                    {
                        strcat(result, path);
                        strcat(result, "\n");
                        break;
                    }
                }
                fclose(file);
            }
        }
    }
    closedir(directory);
}

void ftis(const char *location, const char *argument, const char *filetype, int f, int childnum, const char *childtask, struct timeval starttime)
{
    char result[1000] = "";
    char numstring[3];
    struct timeval endtime;
    double elapsedtime;
    int hours, minutes, seconds, milliseconds, totalseconds;
    char hourstring[3];
    char minstring[3];
    char secstring[3];
    char msecstring[3];

    sprintf(numstring, "%d", childnum);
    strcat(result, "\nChild ");
    strcat(result, numstring);
    strcat(result, " finished finding ");
    strcat(result, childtask);
    strcat(result, ".\n");

    ftishelper(location, argument, filetype, f, childnum, childtask, result);

    if (strlen(result) == 0)
    {
        strcat(result, "Nothing found.\n");
    }
    gettimeofday(&endtime, NULL);
    elapsedtime = (endtime.tv_sec - starttime.tv_sec) * 1000.0;
    elapsedtime += (endtime.tv_usec - starttime.tv_usec) / 1000.0; 
    totalseconds = (int)(elapsedtime / 1000);
    hours = totalseconds / 3600;
    minutes = (totalseconds % 3600) / 60;
    seconds = totalseconds % 60;
    milliseconds = (int)(elapsedtime) % 1000;

    sprintf(hourstring, "%d", hours);
    sprintf(minstring, "%d", minutes);
    sprintf(secstring, "%d", seconds);
    sprintf(msecstring, "%d", milliseconds);

    strcat(result, "Elapsed time: ");
    strcat(result, hourstring);
    strcat(result, ":");
    strcat(result, minstring);
    strcat(result, ":");
    strcat(result, secstring);
    strcat(result, ":");
    strcat(result, msecstring);
    strcat(result, "\n");
    close(fd[0]);
    write(fd[1], result, 1000);
    close(fd[1]);
    kill(getppid(), SIGUSR1); // Perform further actions with the result as needed
}

void removequotes(char *str)
{
    int len;
    int i;
    int j;

    j = 0;
    len = strlen(str);
    for (i = 0; i < len; i++)
    {
        if (str[i] != '"')
        {
            str[j] = str[i];
            j++;
        }
    }
    str[j] = '\0';
}

void signalhandler(int sig)
{
    dup2(fd[0], STDIN_FILENO);
    reading = 1;
}
