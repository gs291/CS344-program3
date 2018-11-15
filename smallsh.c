#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include<fcntl.h>

struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, ignore_action = {0};
sigset_t signal_set;

int cur_ps = 0, forground = 0, sign = 0;
int num_back_PID = 0;
int background_PID[512] = { 0 };
int max_num_arg =0;
int exit_shell = 0;
int last_exit_status = 0;

/*
 * Name: clean up the program
 * Description: Frees any allocated memeory, and removes and lasting background processes
 */
void cleanUpProgram(char *args[], int *argLoc) {
int i, j;

  for(i = 0; i < max_num_arg; i++) {
    free(args[i]);
  }
  for(j = 0; j < num_back_PID; j++) {
    kill(background_PID[j], SIGTERM);
  }
  exit_shell = 1;
}

/*
 * Name: check for built in command
 * Description: Handles if any of the built in functions was inputed.
 * Return: 1 for true if a built in command was inputed, else 0 for false
 */
int checkBuiltIn (char *cmd, char *args[], int *argLoc) {
  if(strstr(cmd, "exit") != NULL && (int)(strstr(cmd, "exit") - cmd) == 0) {  /*f exit is the command */
    cleanUpProgram(args, argLoc);
    return 1;
  }
  else if (strstr(cmd, "cd") != NULL && (int)(strstr(cmd, "cd") - cmd) == 0) { /*If cd was the command */
    if(*(argLoc + 0) == 1) { /*Check if only "cd" was typed*/
      chdir(getenv("HOME"));
    }
    else { /*Else cd to the path */
      chdir(args[1]);
    }
    return 1;
  }
  else if (strstr(cmd, "status") != NULL && (int)(strstr(cmd, "status") - cmd) == 0) { /*If status was the command */
    if(sign != 0) { /*If the last exit was due to a signal termiantion */
      printf("terminated by signal %d", sign);
      fflush(stdout);
    } else {
      printf("exit value %d", last_exit_status);
      fflush(stdout);
    }
    return 1;
  } else if (strstr(cmd, "#") != NULL && (int)(strstr(cmd, "#") - cmd) == 0) { /* If it is a comment */
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

/*
 * Name: get arguments
 * Description: parses the command so each "word" is a element of an array
 */
int * getArgs(char *cmd, char *args[]) {
  char tempString[128];
  char *remainingArgs;
  static int argLoc[3]={0, 0, 0}; //[0]: number of arguments. [1]: location of '<'. [2]: location of '>'
  int indexStart = 0, indexEnd =  0, init = 0, j;

  argLoc[0] = 0; argLoc[1] = 0; argLoc[2] = 0;
  remainingArgs = strchr(cmd, ' ');  /* Substring to the next space till the end */
  do {
    memset(tempString , '\0', sizeof(tempString));

    /* if init == 0, we are getting the actual command, else parse to the next space
     *  so that we know the index of one space and the following one */
    if(init != 0) {
      indexStart = (int)(remainingArgs-cmd) + 1;
      remainingArgs = strchr(remainingArgs+1, ' ');
    }

    /* If we are pasing the last element in the command */
    if(remainingArgs == NULL) { indexEnd = (int)(strchr(cmd, '\0')- cmd);  }
    else { indexEnd = (int)(remainingArgs-cmd); }

    /* Create a temporary string that grabs the content a space to the following space */
    for(j = 0; indexStart < indexEnd; j++) {
      tempString[j] = cmd[indexStart];
      indexStart++;
    }

    /* We parsed from one space to the following space */
    if(indexStart == indexEnd || init == 0) {
      if(tempString[0] != '\0') {
        /* Stores the index location of the "<" and ">" */
        if(strcmp(tempString, "<") == 0) {
          argLoc[1] = argLoc[0];
        } else if (strcmp(tempString, ">") == 0) {
          argLoc[2] = argLoc[0];
        } else if (strstr(tempString, "$$") != NULL) {
          sprintf(strstr(tempString, "$$"), "%ld", (long)getpid());
        }
        /*Allocate memeory in an array of strings such so the new parssed command/argrument gets placed into it*/
        args[argLoc[0]] = (char *) malloc(sizeof(tempString)+1);
        strcpy(args[argLoc[0]], tempString);
        argLoc[0] = argLoc[0] + 1;
        init=1;
      }
    }
  } while (remainingArgs != NULL);

  if(max_num_arg < argLoc[0]) { max_num_arg = argLoc[0]; } /*Holds the maximum amount arguments were inputed so they can be freed upon exit */

  return argLoc;
}

/*
 * Name: remove argument
 * Description: This removes or erases (not free) an element in the array of strings containing the arguments
 */
void removeArg(char *args[], int loc) {
  memset(args[loc] , '\0', 128);
  args[loc] = NULL;
}

/*
 * Name: check redirection
 * Description: checks if an input file or output file was inputed
 *                and creates a file if necessary
 * Return: Returns 1 if any redirection happens, else 0 if it failed to redirect (e.g. invalid file)
 */
int checkRedirection (char *args[], int *argLoc, int bg) {
    if(strcmp(args[*(argLoc + 1)], "<") == 0) {
       int fd = open(args[*(argLoc + 1) + 1], O_RDONLY);

       if(fd != -1) {
         dup2(fd, 0);
       } else {
         return 0;
       }
       removeArg(args, *(argLoc + 1));
       removeArg(args, *(argLoc + 1) + 1);
    } else if (bg == 1) {
      dup2(open("/dev/null", O_RDONLY), 0);
    }
    if(strcmp(args[*(argLoc + 2)], ">") == 0) {
      int fd = open(args[*(argLoc + 2) + 1], O_CREAT | O_WRONLY, 0777);

      if(fd != -1) {
        dup2(fd, 1);
      } else {
        return 0;
      }
      removeArg(args, *(argLoc + 2));
      removeArg(args, *(argLoc + 2) + 1);
    } else if (bg == 1) {
      dup2(open("/dev/null", O_WRONLY), 1);
    }
    return 1;
}

/*
 * Name: check background input
 * Description: Checks if the command is a background process accounting for the forground ^Z signal
 * Return: 0 if there will be no redirection, 1 otherwise
 */
int checkBackground(char *args[], int lastLoc) {
  if(strcmp(args[lastLoc], "&") == 0) {
    removeArg(args, lastLoc);
    if(forground == 1)
      return 0;
    else
      return 1;
  }
  return 0;
}

/*
 * Name: remove background tasks
 * Description: scans to see if any childs were left to be a zombie, and it properly disposes of it
 */
void removeBGTasks() {
  int status, i;

  for(i = num_back_PID - 1; i >= 0; i--) {
      pid_t bg = waitpid(-1, &status, WNOHANG);
      if (bg != 0 && bg != -1) {
        if(status == 15) {
          printf("background pid %d is done: terminated by signal %d\n", bg, status);
          fflush(stdout);
        } else {
          printf("background pid %d is done: exit value %d\n", bg, status);
          fflush(stdout);
      }
        background_PID[i] = 0;
        num_back_PID--;
        last_exit_status = status;
        sign = 0;
      }
  }
  pid_t bg = waitpid(-1, &status, WNOHANG);
}

/*
 * Name: run child exec()
 * Description: Creates a child process to run the exec() command on the inputed command
 */
void runChildExec(char *args[], int *argLoc) {
  pid_t spawnPid = -5;
  int childExitStatus = -5;
  int redirect, background = checkBackground(args, *(argLoc + 0) -1);

  sign=0;
  spawnPid = fork();
  switch (spawnPid) {
      case -1: {
        perror("Hull Breach!\n"); exit(1); break;
      }
    case 0: {  /* CHILD PROCESS */
      if(background == 1) { /* Does or does not create a background process */
        redirect = checkRedirection(args, argLoc, 1);
        setpgid(0, 0);
      } else
         redirect = checkRedirection(args, argLoc, 0);

      if(redirect == 1) { /* If there is a sucessfull redirect even if none were specified */
        execvp(args[0], args);
        perror("CHILD: exec failure!\n");
        fflush(stdout);
      } else {
        perror("No such file or directory\n");
        fflush(stdout);
      }
      exit(2); break;
    }
    default: {  /* PARENT PROCESS */
      if(background == 0) {
        cur_ps = spawnPid;
        sigprocmask(SIG_BLOCK, &signal_set, NULL); /* Block the ^Z signal from being reached until the forground process is done */
        pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0); /* Waits until the child completes */
        fflush(stdout);
        sigprocmask(SIG_UNBLOCK, &signal_set, NULL); /* Unblock the ^Z signal which will then envoke the handler */
        cur_ps = 0;
        last_exit_status = childExitStatus;
        if( last_exit_status >= 20) {
          last_exit_status = 1;
        }
      } else {  /* Displayes the background process and holds info upon exit */
          background_PID[num_back_PID] = getpid();
          num_back_PID++;
          printf("background pid is %d\n", spawnPid);
          fflush(stdout);
      }
      break;
    }
  }
}

/*
 * Name: run shell
 * Description: fundamental loop to create the shell
 */
void runShell () {
  int *argLoc; //POINTER to array of argument locations; [0]: number of arguments. [1]: location of '<'. [2]: location of '>'
  char command[2048];
  char *arguments[512]; /* Array of strings capable of holding 512 strings */

  while(exit_shell != 1) {
    memset(command, '\0', sizeof(command));
    memset(arguments , '\0', sizeof(arguments));

    removeBGTasks();
    printf(": ");
    fgets(command, sizeof(command), stdin);

    clearTrailingChars(command);
    argLoc = getArgs(command, arguments);

    if (checkBuiltIn(command, arguments, argLoc) != 1) { /* Checks if a build in command was inputed, else run the exec() function */
        runChildExec(arguments, argLoc);
    }
  }
}

/*
 * Name: catch SIGINT
 * Description: Signal handler to terminate any running forground process
 */
void catchSIGINT(int signo) {
  if(cur_ps != 0) {
    sign = 2;
    kill(cur_ps, SIGTERM);
    char* message = "terminated by signal 2\n";
    write(STDOUT_FILENO, message, 23);
  }
}

/*
 * Name: catch SIGTSTP
 * Description: changes the froreground mode to ignore background commands
 */
void catchSIGTSTP(int signo) {
  if(forground == 0) {
    forground=1;
    char* message = "Entering foreground-only mode (& is now ignored)\n";
    write(STDOUT_FILENO, message, 50);
  } else {
    forground = 0;
    char* message = "Exiting foreground-only mode\n";
    write(STDOUT_FILENO, message, 29);
  }
}

/*
 * Name: initialize signals
 * Description: initilizes all signal handling and blocking
 */
void initSignals() {
  sigemptyset(&signal_set);

  sigaddset(&signal_set, SIGTSTP);

  SIGINT_action.sa_handler = catchSIGINT;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;

  SIGTSTP_action.sa_handler = catchSIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;

  ignore_action.sa_handler = SIG_IGN;

  sigaction(SIGINT, &SIGINT_action, NULL);
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

void main () {
  initSignals();
  runShell();
  exit(0);
}
