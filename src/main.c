/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Simple driver program for ush's parser
 *
 *  Author...........: Siddhartha Kollipara (skollip)
                       Vincent W. Freeh
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "parse.h"
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>

void pipeHandler(Cmd c);
void fileOutHandler(Cmd c);
void parentPipeHandler(Cmd c);
void runCommand(Cmd c);
void runSetenv(Cmd c);
void runUnsetenv(Cmd c);
void runPwd(Cmd c);
void runEcho(Cmd c);
void runWhere(Cmd c);
void runCd(Cmd c);
void runNice(Cmd c);
void runExecute(Cmd c);

extern char **environ;
int pipeX[2];
int pipeY[2];
int pipeFlag;
char hostname[25];
char* host;
char* upath;

static void prPipe(Pipe p)
{
  int i = 0;
  Cmd c;
  
  pipeFlag = 0;
  
  if(pipe(pipeX) == -1) {
    perror("PipeX");
  }
  if(pipe(pipeY) == -1) {
    perror("PipeY");
  }

  if ( p == NULL )
    return;

  for ( c = p->head; c != NULL; c = c->next ) {
    runCommand(c);
  }
  
  prPipe(p->next);
}

int main(int argc, char *argv[])
{
  Pipe p;
  char* hostname;
  int ush;
  
  int stin = dup(0);
  int stout = dup(1);
  int sterr = dup(2);
  
  hostname = getenv("HOME");
  strcat(hostname, "/.ushrc");
  if((ush = open(hostname, O_RDONLY)) > 0) {
      dup2(ush, 0);
    if(ush > 2) {
      close(ush);
    }
    p = parse();
    prPipe(p);
    freePipe(p);
  }
  
  dup2(stin, 0);
  dup2(stout, 1);
  dup2(sterr, 2);
  
  char *host = "AllYourBase";

  while ( 1 ) {
    //printf("%s%% ", host);
    fflush(stdout);
    p = parse();
    prPipe(p);
    freePipe(p);
    
    dup2(stin, 0);
    dup2(stout, 1);
    dup2(sterr, 2);
    
    char c = getchar();
    if(c == EOF) {
        exit(0);
    } else {
        ungetc(c, stdin);
    }
  }
}

void runCommand(Cmd c) {
    
    if(strcmp(c->args[0], "setenv") == 0) {
        runSetenv(c);
    } else if(strcmp(c->args[0], "logout") == 0) {
        exit(0);
    } else if(strcmp(c->args[0], "unsetenv") == 0) {
        runUnsetenv(c);
    } else if(strcmp(c->args[0], "pwd") == 0) {
        runPwd(c);
    } else if(strcmp(c->args[0], "echo") == 0) {
        runEcho(c);
    } else if(strcmp(c->args[0], "where") == 0) {
        runWhere(c);
    } else if(strcmp(c->args[0], "cd") == 0) {
        runCd(c);
    } else if(strcmp(c->args[0], "nice") == 0) {
        runNice(c);
    } else {
        runExecute(c);
    }
}

void pipeHandler(Cmd c) {
    if(c->in == Tpipe || c->in == TpipeErr) {
        if(pipeFlag == 0) {
            close(pipeX[1]);
            dup2(pipeX[0], 0);
            close(pipeX[0]);
        } else {
            close(pipeY[1]);
            dup2(pipeY[0], 0);
            close(pipeY[0]);
        }
    }
    
    if(c->out == Tpipe || c->out == TpipeErr) {
        if(c->out == TpipeErr) {
            dup2(1, 2);
        }
        
        if(pipeFlag == 1) {
            close(pipeX[0]);
            dup2(pipeX[1], 1);
            close(pipeX[1]);
        } else {
            close(pipeY[0]);
            dup2(pipeY[1], 1);
            close(pipeY[1]);
        }
    }
}

void fileOutHandler(Cmd c) {
    int stout = 0;
    
    if(c->out == ToutErr || c->out == TappErr) {
        dup2(1, 2);
    }
    if(c->out == Tout || c->out == ToutErr) {
        stout = open(c->outfile, O_WRONLY | O_TRUNC | O_CREAT);
        dup2(stout, 1);
        close(stout);
    }
    if(c->out == Tapp) {
        stout = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT);
        dup2(stout, 1);
        close(stout);
    }
}

void pipeParentHandler(Cmd c) {
    if(c->next != NULL) {
        if((c->in == Tnil) && (c->out == Tpipe || c->out == TpipeErr)) {
            close(pipeY[1]);
            pipeFlag = 1;
        } else if(c->in == Tpipe || c->in == TpipeErr) {
            if(pipeFlag == 0) {
                close(pipeY[1]);
                pipeFlag = 1;
            } else {
                close(pipeX[1]);
                pipeFlag = 0;
            }
        }
    }
    if(c->next != NULL) {
        if((c->in == Tpipe || c->in == TpipeErr) && (c->out == Tpipe || c->out == TpipeErr)) {
            if(pipeFlag == 0) {
                if(pipe(pipeY) == -1) {
                    perror("pipeY");
                }
            } else {
                if(pipe(pipeX) == -1) {
                    perror("pipeX");
                }
            }
        }
    }
}

int findCommand(char* c) {
    char* path;
    char delimiter[] = ":";
    struct stat s;
    char command[200];
    char* singlePath;
    char compstr[500];
    int flag = 0;
    
    path = getenv("PATH");
    strcpy(command,"/");
    strcat(command, c);
    
    singlePath = strtok(path, delimiter);
    
    while(singlePath != NULL) {
        strcpy(compstr, singlePath);
        strcat(compstr, command);
        if(stat(compstr, &s) == 0) {
            printf("%s\n", compstr);
            flag = 1;
        }
        singlePath = strtok(NULL, delimiter);
    }
    return flag;
}

void runSetenv(Cmd c) {
    
    pid_t child = fork();
    int stin = 0;
    int n = 0;
    char l;
    char val[200];
    char var[200];
    char word[200];
    
    if(child == 0) {
        pipeHandler(c);
        fileOutHandler(c);
        
        if(c->in == Tpipe || c->in == TpipeErr || c->in == Tin) {
            
            if(c->in == Tin) {
                stin = open(c->infile, O_RDONLY);
            }
            
            while(read(stin, &l, 1) > 0 && l !=  ' ') {
                val[n] = l;
                n++;
            }
            
            strcpy(var, val);
            n = 0;
            
            while(read(stin, &l, 1) > 0 && l != '\n' && l != '\0') {
                val[n] = l;
                n++;
            }
            
            strcpy(word, val);
            if(setenv(var, word, 1) != 0) {
                perror("Set");
            }
            
            if(c->in == Tin) {
                close(stin);
            }
        } else {
            if(c->args[1] == NULL) {
                char** env;
                for(env = environ; *env != NULL; ++env) {
                    printf("%s\n", *env);
                }
            } else {
                if(setenv(c->args[1], c->args[2], 1) != 0) {
                    perror("Set");
                }
            }
        }
        exit(0);
    } else {
        wait();
        pipeParentHandler(c);
    }
}

void runUnsetenv(Cmd c) {
    
    int stin = 0;
    char l;
    char val[200];
    int n = 0;
    
    if(c->in == Tin) {
        stin = open(c->infile, O_RDONLY);
        
        while(read(stin, &l, 1) > 0 && l !=  '\n' && l != '\0') {
            val[n] = l;
            n++;
        }
        
        close(stin);
        
        if(unsetenv(val) != 0) {
            perror("Unset");
        }
    } else {
        if(c->args[1] != NULL) {
            if(unsetenv(c-> args[1]) != 0) {
                perror("Unset");
            }
        }
    }
}

void runPwd(Cmd c) {
    pid_t child = fork();
    char pwd[2000];
    
    if(child == 0) {
        pipeHandler(c);
        fileOutHandler(c);
        
        if(getcwd(pwd, 2000) != NULL) {
            printf("%s\n", pwd);
        }
        exit(0);
    } else {
        wait();
        pipeParentHandler(c);
    }

}

void runEcho(Cmd c) {
    int stin = 0;
    char l;
    int n;
    pid_t child = fork();
    
    if(child == 0) {
        pipeHandler(c);
        fileOutHandler(c);
        for(n = 1; n < c->nargs - 1; n++) {
                printf("%s", c->args[n]);
                printf(" ");
        }
        printf("%s", c->args[n]);
        printf("\n");
        exit(0);
    } else {
        wait();
        pipeParentHandler(c);
    }
}

void runWhere(Cmd c) {
    pid_t child = fork();
    
    if(child == 0) {
        int stin = 0;
        int n = 0;
        char val[200];
        char l;
        char command[200];
        
        pipeHandler(c);
        fileOutHandler(c);
    
        if(c->in == Tpipe || c->in == TpipeErr || c->in == Tin) {
                
            if(c->in == Tin) {
                stin = open(c->infile, O_RDONLY);
            }
            while(read(stin, &l, 1) > 0 && l != ' ' && l != '\n' && l != '\0') {
                val[n] = l;
                n++;
            }
            if(c->in == Tin) {
                close(stin);
            }
            strcpy(command, val);
        } else {
            if(c->args[1] == NULL) {
                exit(0);
            }
            strcpy(command, c->args[1]);
        }
        if(strcmp(command, "setenv") == 0) {
            printf("%s is a shell built-in\n", command);
        } else if(strcmp(command, "logout") == 0) {
            printf("%s is a shell built-in\n", command);
        } else if(strcmp(command, "unsetenv") == 0) {
            printf("%s is a shell built-in\n", command);
        } else if(strcmp(command, "pwd") == 0) {
            printf("%s is a shell built-in\n", command);
        } else if(strcmp(command, "echo") == 0) {
            printf("%s is a shell built-in\n", command);
        } else if(strcmp(command, "where") == 0) {
            printf("%s is a shell built-in\n", command);
        } else if(strcmp(command, "cd") == 0) {
            printf("%s is a shell built-in\n", command);
        } else if(strcmp(command, "nice") == 0) {
            printf("%s is a shell built-in\n", command);
        }
        
        if(findCommand(command) == 0) {
            printf("No instances of this command are available\n");
        }
        exit(0);
    } else {
        wait();
        pipeParentHandler(c);
    }
}

void runCd(Cmd c) {
    
    pipeHandler(c);
    fileOutHandler(c);
    if(c->args[1] == NULL) {
        if(chdir("/home") < 0) {
            perror("/home");
        }
    } else {
        if(chdir(c->args[1]) < 0) {
            perror("cd");
        }
    }
}

void runNice(Cmd c) {
    int n = PRIO_PROCESS;
    pid_t child;
    int pri;
    
    if(c->nargs == 1) {
        setpriority(n, getpid(), 4);
    }
    if(c->nargs == 2) {
        pri = atoi(c->args[1]);
        
        if(pri == 0) {
            if(!isdigit(c->args[1][0])) {
                child = fork();
                if(child == 0) {
                    setpriority(n, getpid(), 4);
                    execvp(c->args[1], &c->args[1]);
                    exit(0);
                } else {
                    pri = 4;
                    wait();
                }
            }
        }
        if(pri < 0) {
            return;
        }
        setpriority(n, getpid(), pri);
    }
    if(c->nargs == 3) {
        pri = atoi(c->args[1]);
        if(pri < 0) {
            return;
        }
        child = fork();
        if(child == 0) {
            setpriority(n, getpid(), pri);
            execvp(c->args[2], &c->args[2]);
            exit(0);
        } else {
            pri = 4;
            wait();
        }
    }
}

void runExecute(Cmd c) {
    int stin;
    char l;
    char val[200];
    int n = 0;
    pid_t child = fork();
    if(child == 0) {
        pipeHandler(c);
        fileOutHandler(c);
        if(execvp(c->args[0], c->args) < 0) {
            printf("command not found\n");
        }
        exit(0);
    } else {
        wait();
        pipeParentHandler(c);
    }
}
/*........................ end of main.c ....................................*/
