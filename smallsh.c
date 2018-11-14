#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include<fcntl.h>


int max_num_arg =0;
int exit_shell = 0;
int last_exit_status = 0;

void cleanUpProgram(char *args[], int *argLoc) {
  for(int i = 0; i < max_num_arg; i++) {
    free(args[i]);
  }
  exit_shell = 1;
}

int checkBuiltIn (char *cmd, char *args[], int *argLoc) {
  if(strstr(cmd, "exit") != NULL && (int)(strstr(cmd, "exit") - cmd) == 0) {
    cleanUpProgram(args, argLoc);
    return 1;
  }
  else if (strstr(cmd, "cd") != NULL && (int)(strstr(cmd, "cd") - cmd) == 0) {
    if(*(argLoc + 0) == 1) {
      chdir(getenv("HOME"));
    }
    else {
      chdir(args[1]);
    }
    return 1;
  }
  else if (strstr(cmd, "status") != NULL && (int)(strstr(cmd, "status") - cmd) == 0) {
    printf("exit value %d\n", last_exit_status);
    return 1;
  } else if (strstr(cmd, "#") != NULL && (int)(strstr(cmd, "#") - cmd) == 0) {
    return 1;
  }
  else {
    return 0;
  }
}

void clearTrailingChars(char *str) {
  /*Clear any leading spaces or new lines*/
  char *p = strchr(str, '\n');
  if(p != NULL){ *p=0; }
}

int * getArgs(char *cmd, char *args[]) {
  char tempString[128];
  char *remainingArgs;
  static int argLoc[3]={0, 0, 0}; //[0]: number of arguments. [1]: location of '<'. [2]: location of '>'
  int indexStart = 0, indexEnd =  0, init = 0;

  argLoc[0] = 0; argLoc[1] = 0; argLoc[2] = 0;
  remainingArgs = strchr(cmd, ' ');
  do {
    memset(tempString , '\0', sizeof(tempString));
    if(init != 0) {
      indexStart = (int)(remainingArgs-cmd) + 1;
      remainingArgs = strchr(remainingArgs+1, ' ');
    }

    if(remainingArgs == NULL) { indexEnd = (int)(strchr(cmd, '\0')- cmd);  }
    else { indexEnd = (int)(remainingArgs-cmd); }

    for(int j = 0; indexStart < indexEnd; j++) {
      tempString[j] = cmd[indexStart];
      indexStart++;
    }

    if(indexStart == indexEnd || init == 0) {
      if(tempString[0] != '\0') {
        if(strcmp(tempString, "<") == 0) {
          argLoc[1] = argLoc[0];
        } else if (strcmp(tempString, ">") == 0) {
          argLoc[2] = argLoc[0];
        }

        args[argLoc[0]] = (char *) malloc(sizeof(tempString)+1);
        strcpy(args[argLoc[0]], tempString);
        argLoc[0] = argLoc[0] + 1;
        init=1;
      }
    }
  } while (remainingArgs != NULL);

  if(max_num_arg < argLoc[0]) { max_num_arg = argLoc[0]; }

  // for(int k = 0; k < max_num_arg; k++) {
  //   printf("ARG[%d]: %s\n", k, args[k]);
  // }
  return argLoc;
}

void removeArg(char *args[], int loc) {
  memset(args[loc] , '\0', 128);
  args[loc] = NULL;
}

void checkRedirection (char *args[], int *argLoc) {
    if(strcmp(args[*(argLoc + 1)], "<") == 0) {
       int fd = open(args[*(argLoc + 1) + 1], O_RDONLY);

       if(fd != -1) {
         dup2(fd, 0);
       }
       removeArg(args, *(argLoc + 1));
       removeArg(args, *(argLoc + 1) + 1);
    }
    if(strcmp(args[*(argLoc + 2)], ">") == 0) {
      int fd = open(args[*(argLoc + 2) + 1], O_CREAT | O_TRUNC | O_WRONLY, 0755);

      if(fd != -1) {
        dup2(fd, 1);
      }
      removeArg(args, *(argLoc + 2));
      removeArg(args, *(argLoc + 2) + 1);
    }
}

void checkBackground(char *args[], int lastLoc) {
  if(strcmp(args[lastLoc], "&") == 0) {
    setpgid(0, 0);
    removeArg(args, lastLoc);
  }
}


void runChildExec(char *args[], int *argLoc) {
  pid_t spawnPid = -5;
  int childExitStatus = -5;
  spawnPid = fork();
  switch (spawnPid) {
      case -1: {
        perror("Hull Breach!\n"); exit(1); break;
      }
    case 0: {
      checkRedirection(args, argLoc);
      checkBackground(args, *(argLoc + 0) -1);
      execvp(args[0], args);
      perror("CHILD: exec failure!\n");
      exit(2); break;
    }
    default: {
      if(strcmp(args[*(argLoc + 0)-1], "&") != 0) {
        pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
      } else
          printf("background pid is %d\n", spawnPid);
      break;
    }
  }
}

void runShell () {
  int *argLoc; //POINTER to array of argument locations; [0]: number of arguments. [1]: location of '<'. [2]: location of '>'
  char command[2048];
  char *arguments[512];

  while(exit_shell != 1) {
    memset(command, '\0', sizeof(command));
    memset(arguments , '\0', sizeof(arguments));

    printf(": ");
    fgets(command, sizeof(command), stdin);

    clearTrailingChars(command);
    argLoc = getArgs(command, arguments);

    //printf("numArg: %d\nLoc <: %d\nLoc >: %d\n", *(argLoc + 0), *(argLoc + 1), *(argLoc + 2));
    if (checkBuiltIn(command, arguments, argLoc) != 1) {
        runChildExec(arguments, argLoc);
    }
  }
}

void main () {
  runShell();
}
