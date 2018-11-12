#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int max_num_arg =0;
int exit_shell = 0;
int last_exit_status = 0;

void cleanUpProgram(char *args[], int numArgs) {
  for(int i = 0; i < max_num_arg; i++) {
    free(args[i]);
  }
  exit_shell = 1;
}

int checkBuiltIn (char *cmd, char *args[], int numArgs) {
  if(strstr(cmd, "exit") != NULL && (int)(strstr(cmd, "exit") - cmd) == 0) {
    cleanUpProgram(args, numArgs);
    return 1;
  }
  else if (strstr(cmd, "cd") != NULL && (int)(strstr(cmd, "cd") - cmd) == 0) {
    if(numArgs == 0) {
      chdir("~");
    }
    else {
      chdir(args[1]);
    }
    return 1;
  }
  else if (strstr(cmd, "status") != NULL && (int)(strstr(cmd, "status") - cmd) == 0) {
    printf("exit value %d\n", last_exit_status);
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

int getArgs(char *cmd, char *args[]) {
  char tempString[128];
  char *remainingArgs;
  int numArgs = 0;
  int indexStart = 0, indexEnd =  0, init = 0;

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
        args[numArgs] = (char *) malloc(sizeof(tempString)+1);
        strcpy(args[numArgs], tempString);
        numArgs++;
        init=1;
      }
    }
  } while (remainingArgs != NULL);

for(int k = numArgs - numArgs; k < numArgs; k++) {
  printf("ARG[%d]: %s\n", k, args[k]);
}
  if(max_num_arg < numArgs) {
    max_num_arg = numArgs;
  }
  return numArgs;
}



void runShell () {
  int numArgs = 0;
  char command[2048];
  char *arguments[512];

  while(exit_shell != 1) {
    memset(command, '\0', sizeof(command));
    memset(arguments , '\0', sizeof(arguments));

    printf(": ");
    fgets(command, sizeof(command), stdin);

    clearTrailingChars(command);
    numArgs = getArgs(command, arguments);


    if (checkBuiltIn(command, arguments, numArgs) != 1) {
        execvp(arguments[0], arguments);
    }
  }


}

void main () {
  runShell();
}


void test() {
  pid_t spawnPid = -5;
  int childExitStatus = -5;
  spawnPid = fork();
  switch (spawnPid) {
      case -1: {
        perror("Hull Breach!\n"); exit(1); break;
      }
    case 0: {
      printf("CHILD(%d): Sleeping for 1 second\n", getpid());
      sleep(1);
      printf("CHILD(%d): Converting into \'ls -a\'\n", getpid());
      execlp("ls", "ls", "-a", NULL);
      perror("CHILD: exec failure!\n");
      exit(2); break;
    }
    default: {
      printf("PARENT(%d): Sleeping for 2 seconds\n", getpid());
      sleep(2);
      printf("PARENT(%d): Wait()ing for child(%d) to terminate\n", getpid(), spawnPid);
      pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
      printf("PARENT(%d): Child(%d) terminated, Exiting!\n", getpid(), actualPid);
      exit(0); break;
    }
  }
}
