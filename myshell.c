#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

char line[1024] = {0};
char* arguments[50];
char* pipeargs[50];
int done = 0;
int input = 0;
int arglength;
int emptyline = 0;

const char s[2] = " ";
char *token;
int i = 0;
int len;

int isIOOut = 0; // ">"
int IOOutSpot;
FILE * outFile;
int isIOIn = 0; // "<"
int IOInSpot;
FILE * inFile;
int isPipe = 0; // "|"
int pipeSpot;
int pipeReady = 0;
int isBackground = 0;
int progRunninginBackground = 0;

int childProcesses[10];
int howManyChildren = 0;
int posOfSuspendedChild;

void copypipeargs() {
    int n = 0;
    for(int j = (pipeSpot + 1); arguments[j] != NULL; j++) {
        pipeargs[n] = arguments[j];
        n++;
    }
    pipeargs[n] = NULL;
}

void waitonchildren() {
    int k = 0;
    while(childProcesses[k] != NULL) {
        if(waitpid(childProcesses[k], NULL, 0) != childProcesses[k] &&
           childProcesses[k] != 0) {
            k++;
        }
        else {
            childProcesses[k] = 0;
            howManyChildren--;
            k++;
        }
    }
}

void resumechild() {
    if(posOfSuspendedChild == NULL) {
        printf("No suspended program detected, nothing was resumed.\n");
    }
    else {
        kill(childProcesses[i], SIGCONT);
        printf("Resumed execution of child process.\n");
    }
}

char *trimwhitespace(char *str) {
  char *end;

  while(strcmp(str, " ") == 0) str++;

  if(*str == 0)
    return str;

  end = str + strlen(str) - 1;
  while(end > str && strcmp(end, " ") == 0) end--;

  *(end+1) = 0;

  return str;
}

extern "C" void sig_kill(int signo) {
    if(howManyChildren == 0) {
        printf("No background processes detected, no program is killed.\n");
    }
    else {
        for(int i = 0; i < 10; i++) {
            if(childProcesses[i] != 0 && childProcesses[i] != NULL) {
                kill(childProcesses[i], SIGKILL);
                break;
                }
            }
    }
    printf("%s", "shell>");
}

extern "C" void sig_suspend(int signo) {
    if(howManyChildren == 0) {
        printf("No background processes detected, no program is suspended.\n");
    }
    else {
        for(int i = 0; i < 10; i++) {
            if(childProcesses[i] != 0 && childProcesses[i] != NULL) {
                kill(childProcesses[i], SIGTSTP);
                posOfSuspendedChild = i;
                break;
                }
            }
    }
    printf("%s", "shell>");
}

void execcommand() {

    int pid = fork();

    if(pid < 0) {
        printf("Error: forking child process failed.\n");
    }
    else if(pid == 0) {
        if(isBackground) {
        }
        if(isIOOut) {
            outFile = fopen(arguments[IOOutSpot + 1], "w");
            arguments[IOOutSpot] = NULL;
            dup2(fileno(outFile), fileno(stdout));
            fclose(outFile);
        }
        if(isIOIn) {
            inFile = fopen(arguments[IOInSpot + 1], "w");
            arguments[IOInSpot] = NULL;
            dup2(fileno(inFile), fileno(stdin));
            fclose(inFile);
        }
        if(isPipe) {
            outFile = fopen("pipeOut.txt", "w");
            arguments[pipeSpot] = NULL;
            dup2(fileno(outFile), fileno(stdout));
            fclose(outFile);
            pipeReady = 1;
        }
        if(execvp(arguments[0], arguments) != 0) {
        printf("Error: failed to execute command.\n");
        }
        }
    else {
        if(isPipe && pipeReady) {
            int pid2 = fork();
            if(pid2 == 0) {
                outFile = fopen("pipeOut.txt", "r");
                dup2(fileno(outFile), fileno(stdin));
                fclose(outFile);
                copypipeargs();
                execvp(arguments[pipeSpot + 1], pipeargs);
                printf("Error: unrecognized command.\n");
            }
            else {
                while(waitpid(pid2, NULL, 0) != pid2);
            }
        }
        if(isBackground != 1) {
            while(waitpid(pid, NULL, 0) != pid);
            if(isIOOut) {
                printf("Output written to file.\n");
            }
            if(isIOIn) {
                printf("File used as input.\n");
            }
            input = 0;
        }
        else {
            int k = 0;
            while(childProcesses[k] == NULL) {
                k++;
            }
            childProcesses[k] = pid;
            printf("Running program in the background\n");
            progRunninginBackground = 1;
            howManyChildren++;
        }
    }
}

int main() {

    while(!done) {

        (void) signal(SIGINT, sig_kill);
        (void) signal(SIGTSTP, sig_suspend);

        input = 0;
        printf("%s", "shell>");
        fflush(stdout);

        fgets(line, 1024, stdin);

        if(line[0] == '\n') emptyline = 1;

        if(emptyline != 1) {
        len = strlen(line);
        if(line[len - 1] == '\n') line[len - 1] = 0;

        len = strlen(line);
        if(line[len - 1] == '&') {
            isBackground = 1;
            line[len - 1] = 0;
        }

        token = strtok(line, s);

        while(token != NULL) {
            token = trimwhitespace(token);
            arguments[i] = token;
            i++;
            token = strtok(NULL, s);
            }
        arguments[i] = NULL;
        i = 0;

        if(strcmp(arguments[0], "") == 0) input = 0;
        else if(strcmp(arguments[0], "exit") == 0) input = 1;
        else if(strcmp(arguments[0], "resume") == 0) input = 2;
        else input = 3;
        }

        if(!done && input != 0) {

            for(i; arguments[i] != NULL; i++) {
                if(strcmp(arguments[i], ">") == 0) {
                    isIOOut = 1;
                    IOOutSpot = i;
                }
                if(strcmp(arguments[i], "<") == 0) {
                    isIOIn = 1;
                    IOInSpot = i;
                }
                if(strcmp(arguments[i], "|") == 0) {
                    isPipe = 1;
                    pipeSpot = i;
                }
            }

            switch(input) {
            case 1: done = 1;
                    break;
            case 2: resumechild();
                    break;
            case 3: execcommand();
                    break;
            default: break;
            }
        }

            emptyline = 0;
            i = 0;
            isIOIn = 0;
            IOInSpot = NULL;
            isIOOut = 0;
            IOOutSpot = NULL;
            isPipe = 0;
            pipeSpot = NULL;
            pipeReady = 0;
            isBackground = 0;
            memset(arguments, 0, sizeof arguments);
            memset(pipeargs, 0, sizeof pipeargs);
    }

    if(howManyChildren != 0) printf("Waiting on background processes.\n");
    while(howManyChildren != 0) {
        waitonchildren();
    }

    printf("Exiting shell...\n");

    return 0;
}
