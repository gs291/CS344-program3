#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int exit_shell = 0;
int last_exit_status = 0;

void cleanUpProgram() {
  exit_shell = 1;
}

int checkBuiltIn (char *cmd, char args[][128], int numArgs) {
  if(strstr(cmd, "exit") != NULL && (int)(strstr(cmd, "exit") - cmd) == 0) {
    cleanUpProgram();
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

int getArgs(char *cmd, char args[][128]) {
  char *remainingArgs;
  int indexStart = 0, indexEnd = 0, numArgs = 0;
  
  remainingArgs = strchr(cmd, ' ');

  while(remainingArgs != NULL) {
    indexStart = (int)(remainingArgs-cmd);
    remainingArgs = strchr(remainingArgs+1, ' ');

    if(remainingArgs == NULL) { indexEnd = (int)(strchr(cmd, '\0')- cmd);  }
    else { indexEnd = (int)(remainingArgs-cmd); }

    if(indexStart+1 != indexEnd) {
      for(int j = 0; indexStart+1 < indexEnd; j++) {
        args[numArgs][j] = cmd[indexStart+1];
        indexStart++;
      }
      numArgs++;
    }
  }
}

void runShell () {
  int numArgs = 0;
  char command[2048], arguments[512][128];

  while(exit_shell != 1) {
    memset(command, '\0', sizeof(command));
    memset(arguments , '\0', sizeof(arguments));

    printf(": ");
    fgets(command, sizeof(command), stdin);

    clearTrailingChars(command);
    printf("%s\n", command);
    numArgs = getArgs(command, arguments);

    if (checkBuiltIn(command, arguments, numArgs) != 1) {
        printf("NOT BUILT IN COMMAND\n");
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
