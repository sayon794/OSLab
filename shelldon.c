//basic commands added
//built-ins added
//need to complete help command
//redirection added
//multi-piping added
//bg process can be created
//need to figure out how to handle zombies
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/stat.h>

#define TOKEN_SIZE 100
#define TOKEN_DELIMITERS " \t\r\n\""
//added " as a delimiter to simplify calls of echo
//bug: having to echo a string with " pairs within words

typedef struct iodata {
    int in,out;
} iodata;
//structure containing current filedescriptors for i/o streams
//may be useful for multi-level piping

char *readCmd();
char **splitCmd(char *cmd);
int runCmd(char **cmd);
int isBuiltin(char *cmd); //send first token as parameter
int runBuiltin(char **cmd,int builtinType);
int shellcd(char **cmd);
int shellecho(char **cmd);
int shellhelp(char **cmd);
void setRedirection(char **cmd,iodata *io);
int numPipes(char **cmd);
void plumber(int pipes,char **cmd,iodata io);

char *builtins[] = {"cd","echo","exit","help"};
const int nbuiltins = 4;

int main(int argc, char **argv) {
    char *cmd;
    char **args;

    printf("------Welcome to Shelldon------\n");
    printf("use \"help\" to see list of available features\n");

    while(1) {
        printf("%s=> ",getlogin());
        cmd = readCmd();
        args = splitCmd(cmd);

        if(args[0]!=NULL&&!runCmd(args)) //need to skip if blank command
            break;

        //printf("%s\n%s\n%s\n",args[0],args[1],args[2]);
        free(cmd);
        free(args);
    }

    free(cmd);
    free(args);

    return 0;
}

char *readCmd() {
    char *cmd = NULL;
    ssize_t buffersize = 0;
    getline(&cmd,&buffersize,stdin);
    //reads a line from input stream and handles memory allocation
    return cmd;
}

char **splitCmd(char *cmd) {
    int i=0,buffersize = TOKEN_SIZE;
    char **tokens = (char **) malloc (buffersize*sizeof(char *));
    char *token;

    if(!tokens) {
        perror("shelldon");
        exit(EXIT_FAILURE);
    }

    token = strtok(cmd, TOKEN_DELIMITERS);
    /*walks through the string till it encounters a null or a delimiter,
    wraps the portion it got into a string. each subsequent call is done
    from null to continue traversal from the last portion of the original
    string*/
    while(token != NULL) {
        tokens[i++] = token;

        if(i >= buffersize) {
            buffersize += TOKEN_SIZE;
            tokens = (char **) realloc(tokens, buffersize*sizeof(char *));
            if(!tokens) {
                perror("shelldon");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, TOKEN_DELIMITERS);
    }
    tokens[i] = NULL;
    return tokens;
}

int runCmd(char **cmd) {
    pid_t pid;
    int status,pipes;
    //Check if given command is a built-in
    int builtinType;
    if(builtinType=isBuiltin(cmd[0]))
        return runBuiltin(cmd,builtinType);

    iodata io = {STDIN_FILENO,STDOUT_FILENO};

    //check if background process
    int bg=0,i;
    for(i=0;cmd[i]!=NULL;i++)
        if(strcmp(cmd[i],"&")==0) {
            bg=1;
            cmd[i]=NULL;
        }

    if((pid = fork()) == 0) {
        pipes=numPipes(cmd);
        //let plumber handle the pipes
        plumber(pipes,cmd,io);
    }
    else if(pid < 0) {
        perror("shelldon");
    }
    else {
        if(!bg) //don't wait if background process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && WIFSIGNALED(status));
        /*waits for the child to return by exiting normally
        or due to some unhandled signal*/
    }
    return 1;
}

/****************Built-in Support Start**********************/
int isBuiltin(char *cmd) {
    int i=0;
    for(;i<nbuiltins;i++) {
        if(strcmp(cmd,builtins[i])==0)
            return i+1;
    }
    return 0;
}

/*1=cd
  2=echo
  3=exit
  4=help
*/
int runBuiltin(char **cmd,int builtinType) {
    if(builtinType==1)
        return shellcd(cmd);
    else if(builtinType==2)
        return shellecho(cmd);
    else if(builtinType==3)
        return 0;
    else if(builtinType==4)
        return shellhelp(cmd);
    return 1;
}

int shellcd(char **cmd) {
    if(cmd[1]==NULL) {
        if(chdir(getenv("HOME"))!=0)
            perror("shelldon");
    }
    else if(chdir(cmd[1])!=0)
        perror("shelldon");
    return 1;
}

int shellecho(char **cmd) {
    int i=1;
    while(cmd[i]!=NULL) {
        printf("%s ",cmd[i++]);
    }
    printf("\n");
    return 1;
}

int shellhelp(char **cmd) {
    printf("\n");
    printf("Features of Shelldon:\n");
    printf("Supports all commands that can be found via $PATH\n");
    printf("Supports I/O redirection with \">\",\">>\",\"<\"\n");
    printf("Supports multiple level piping with I/O redirection using \"|\"\n");
    printf("Can open processes in background by adding \"&\" in command\n");
    printf("(Does not close them upon termination, may create zombies)\n");
    printf("\n");
    printf("Built-ins:\n");
    printf("cd [directory] -> change to specified directory or home if unspecified\n");
    printf("echo [message] -> display message\n");
    printf("exit -> exits shelldon\n");
    printf("\n");
    printf("author: Sayontan Chowdhury\n");
    printf("\n");
    return 1;
}
/****************Built-in Support End**********************/


/****************I/O Redirection Start*********************/
void setRedirection(char **cmd,iodata *io) {
    int i=1;
    int oldin = io->in, oldout = io->out;
    for(;cmd[i]!=NULL;i++) {
        if(strcmp(cmd[i],">")==0) {
            io->out = open(cmd[i+1], O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
            if(io->out==-1) {
                perror("shelldon");
                exit(EXIT_FAILURE);
            }
            dup2(io->out, oldout);
            i++;
            close(io->out);
        }
        else if(strcmp(cmd[i],">>")==0) {
            io->out = open(cmd[i+1], O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
            if(io->out==-1) {
                perror("shelldon");
                exit(EXIT_FAILURE);
            }
            dup2(io->out, oldout);
            i++;
            close(io->out);
        }
        else if(strcmp(cmd[i],"<")==0) {
            io->in = open(cmd[i+1], O_RDONLY);
            if(io->in==-1) {
                perror("shelldon");
                exit(EXIT_FAILURE);
            }
            dup2(io->in, oldin);
            i++;
            close(io->in);
        }
    }
    /*Creating a temporary copy of the command which eliminates redirections
      and filenames*/
    char **temp = (char **) malloc(++i*sizeof(char *));
    int j=0;
    for(i=0;cmd[i]!=NULL;i++) {
        if(strcmp(cmd[i],">")==0 || strcmp(cmd[i],">>")==0
        || strcmp(cmd[i],"<")==0) {
            i++;
            continue;
        }
        temp[j++]=cmd[i];
    }
    temp[j]=NULL;
    //Copying back to the original command
    for(i=0;i<=j;i++) {
        cmd[i]=temp[i];
        temp[i]=NULL;
    }
    free(temp);
}

int numPipes(char **cmd) {
    int counter=0,i=0;
    for(;cmd[i]!=NULL;i++) {
        if(strcmp(cmd[i],"|")==0)
            counter++;
    }
    return counter;
}

/*the following function works recursively by letting
  the parent handle the final command of the piped statement
  and the child recursively handling the former portions.
  Uses the same idea in which strtok works to separate the commands.
*/
void plumber(int pipes,char **cmd,iodata io) {
    if(pipes==0) {  //default action for no pipes
        setRedirection(cmd,&io);
        if(execvp(cmd[0],cmd) == -1)
            perror("shelldon");
        exit(EXIT_FAILURE);
    }
    int fd[2],i,status;
    pid_t pid;
    char **parent;
    for(i=0;cmd[i]!=NULL;i++);
    while(strcmp(cmd[--i],"|")!=0); //traverse to end and find last pipe
    parent=&cmd[i+1];   //parent command is the latter portion
    cmd[i]=NULL;        //end for child command
    pipe(fd);
    if((pid=fork())==0) {
        //fprintf(stderr, "process is %d\n", getpid());
        dup2(fd[1],io.out);
        //io.out=fd[1];
        close(fd[0]);
        plumber(pipes-1,cmd,io);
    }
    else {
        //fprintf(stderr, "process is %d\n", getpid());
        dup2(fd[0],io.in);
        //io.in=fd[0];
        close(fd[1]);
        do {
            waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && WIFSIGNALED(status));
        setRedirection(parent,&io);
        if(execvp(parent[0],parent) == -1)
            perror("shelldon");
        exit(EXIT_FAILURE);
    }
}
/****************I/O Redirection End*********************/
