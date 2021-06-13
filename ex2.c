// avi miletzky

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_INPUT 512
#define ERROR "Error in system call\n"

struct Job {
    char *comnd;
    pid_t pid;
    struct Job *next;
};

typedef struct Job Job;

Job *createJob(char *command, pid_t pId) {
    Job *newJob = (Job *) malloc(sizeof(Job));
    newJob->comnd = (char *) malloc(strlen(command) + 1);
    strcpy(newJob->comnd, command);
    newJob->pid = pId;
    newJob->next = NULL;
    return newJob;
}

Job *addJob(Job *head, char *command, pid_t pId) {
    Job *temp = head;
    if (head == NULL) {
        return createJob(command, pId);
    }
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = createJob(command, pId);
    return head;
}

void freeJob(Job *job) {
    free(job->comnd);
    free(job);
}

Job *removeJob(Job *head, pid_t pId) {
    Job *previous = head, *current = head->next;
    //In case is needed to remove the head of the Jobs list;
    if (head->pid == pId) {
        freeJob(head);
        //printf("---remove head: %d\n", pId);
        return current;
    }
    //otherwise
    while (current != NULL) {
        if (current->pid == pId) {
            previous->next = current->next;
            freeJob(current);
            //printf("---remove: %d\n", pId);
            break;
        }
        previous = current;
        current = current->next;
    }
    return head;
}

void printJobs(Job *head) {
    Job *temp = head;
    while (temp != NULL) {
        printf("%d %s\n", temp->pid, temp->comnd);
        temp = temp->next;
    }
}

Job *updateJobs(Job *head) {
    Job *temp = head;
    int state, fin, pId;
    //printf("---t:%d h:%d\n", &temp, &head);
    while (temp != NULL) {
        fin = waitpid(temp->pid, &state, WNOHANG);
        pId = temp->pid;
        temp = temp->next;
        if (fin) {
            head = removeJob(head, pId);
            //printf("---h:%d\n", &head);
        }
    }
    return head;
}

void deleteAllJobs(Job *head) {
    while (head != NULL) {
        kill(head->pid, SIGKILL);
        head = removeJob(head, head->pid);
    }
}

bool runsCdCommand(char **arguments, int sizeArgs, char *prevLocation, bool
    isFirstTime) {
    int status;
    printf("%d\n", getpid());
    sleep(0.1);
    if (sizeArgs == 1 || !strcmp(arguments[1], "~")) {
        status = chdir(getenv("HOME"));
    } else if (!strcmp(arguments[1], "-")) {
        if (isFirstTime) {
            fprintf(stderr, "OLDPWD not set\n");
            return true;
        }
        status = chdir(prevLocation);
    } else {
        status = chdir(arguments[1]);
    }
    if (status) {
        fprintf(stderr, ERROR);
    }
    return false;
}

int split(char *buffer, char delimiter[], char **arguments) {
    char *separatedArgs, *separatedByQuote, *tempPtr;
    int i, argCounter = 0;
    char tempBuf[MAX_INPUT];
    separatedArgs = strtok(buffer, delimiter);
    for (i = 0; separatedArgs != NULL; i++) {
        tempPtr = separatedArgs;
        if (separatedArgs[0] == '\"') {
            bzero(tempBuf, MAX_INPUT);
            strcpy(tempBuf, separatedArgs + 1);

            if (separatedArgs[strlen(separatedArgs) - 1] == '\"') {
                tempBuf[strlen(tempBuf) - 1] = '\0';
            } else {
                strcat(tempBuf, " ");
                separatedByQuote = strtok(NULL, "\"");
                strcat(tempBuf, separatedByQuote);
            }
            tempPtr = tempBuf;
        }
        arguments[i] = (char *) malloc(strlen(tempPtr) + 1);
        strcpy(arguments[i], tempPtr);
        argCounter++;
        //printf("%s\n", tempPtr);//----------------------------------
        separatedArgs = strtok(NULL, delimiter);
    }
    arguments[i] = NULL;
    return argCounter;
}

Job *runsCommand(Job * jobs, char **arguments, char *command, bool ampersand) {
    pid_t pidChild;
    int status;

    pidChild = fork();
    if (pidChild > 0) {
        printf("%d\n", pidChild);
        if (!ampersand) {
            pidChild = wait(&status);
        } else {
            jobs = addJob(jobs, command, pidChild);
            free(command);
        }
    }
    if (pidChild == 0) {
        execvp(arguments[0], arguments);
        fprintf(stderr, ERROR);
        exit(1);
    }
    if (pidChild == -1) {
        fprintf(stderr, ERROR);
    }
    return jobs;
}

int main() {
    char buffer[MAX_INPUT], prevLocation[MAX_INPUT], currLocation[MAX_INPUT];
    Job *jobs = NULL;
    char *command = NULL;
    char **arguments;
    char delimiter[] = " ";
    int j, k, argCounter = 0;
    bool ampersand = false, isFirstTime = true;

    bzero(prevLocation, MAX_INPUT);
    bzero(currLocation, MAX_INPUT);

    printf(">");
    fgets(buffer, MAX_INPUT, stdin);

    while (strcmp(buffer, "exit\n")) {
        if (buffer[0] != '\n') {
            buffer[strlen(buffer) - 1] = '\0';
            if (strcmp(&(buffer[(strlen(buffer) - 1)]), "&") == 0) {
                buffer[strlen(buffer) - 1] = '\0';
                ampersand = true;
                for (k = (strlen(buffer) - 1); (buffer[k] == ' '); --k) {
                    buffer[k] = '\0';
                }
                command = (char *) malloc(strlen(buffer) + 1);
                strcpy(command, buffer);
            }

            arguments = (char **) malloc(MAX_INPUT * sizeof(char *));
            argCounter = split(buffer, delimiter, arguments);

            if (!strcmp(arguments[0], "jobs")) {
                jobs = updateJobs(jobs);
                printJobs(jobs);
            } else if (!strcmp(arguments[0], "cd")) {
                getcwd(currLocation, MAX_INPUT);
                isFirstTime = runsCdCommand(arguments, argCounter,
                        prevLocation, isFirstTime);
                strcpy(prevLocation, currLocation);
            } else {
                jobs = runsCommand(jobs, arguments, command, ampersand);
            }

            //free the allocated memory.
            for (j = 0; j < argCounter; ++j) {
                free(arguments[j]);
            }
            free(arguments);
        }
        ampersand = false;
        bzero(buffer, MAX_INPUT);
        printf(">");
        fgets(buffer, MAX_INPUT, stdin);
    }
    printf("%d\n", getpid());
    deleteAllJobs(jobs);
    return 0;
}