/*
 * AUTHOR : AHMET TURAN BULUT
 * LOGIN : aturanb
 * TITLE: CIS 415 P1
 * This is my own work except help hours and lab codes
*/

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include "p1fxns.h"
#include <sys/wait.h>
#include <sys/types.h>

volatile int usr1_handled = 0;

static void sigusr1_handler(int signal){
	if(signal == SIGUSR1){
		usr1_handled++;
	}

}

void free_args(char **args, int arg_count){
	if(args != NULL){
		for(int i = 0; i < arg_count; i++){
			if(args[i] != NULL){
				free(args[i]);
			}
		}
		free(args);
	}
}

int main(int argc, char* argv[]){
	
	if(signal(SIGUSR1, sigusr1_handler) == SIG_ERR){
		exit(1);
	}
	int *pidarr = NULL;

	int file = 0;
	char *filename = NULL;
	
	//Process Quantum
	char *p = getenv("USPS_QUANTUM_MSEC");
	
	for(int i = 1; i < argc; i++){
		//Get the -q argument
		if(p1strneq(argv[i], "-q", 2) && (argc >= 3)){
			p = argv[i+1];
		}
		//Get the file argument
		else if (argc != 1 && (i != 2)){ 
			filename = argv[i];
		}
	}

	//If file specified, use that instead of stdin
	if(filename != NULL){  
		if((file = open(filename, O_RDONLY)) < 0){
			p1perror(2, "File cannot be opened\n");
			p1perror(2, "USAGE: ./uspsv1 [-q <quantum in msec>] [workload_file]\n");
			exit(1);
		} 
	}

	//No -q supplied and no Quantum env variable
	if(p == NULL){
		p1perror(2, "Quantum undefined\n");
		exit(1);
	}
	
	char buf[BUFSIZ];
	int nprograms = 0;
	int linelength;
	
	while((linelength = p1getline(file, buf, BUFSIZ)) != 0){
		
		nprograms++;
		
		//Get rid of the newline character
		if(buf[linelength - 1] == '\n'){
			buf[linelength - 1] = '\0';
		}

		if(pidarr == NULL){
			pidarr = (int *) malloc(sizeof(int));
		}
		else{
			pidarr = (int *) realloc(pidarr, nprograms * sizeof(int));
		}
		
		char *args[256];
		char tmp[64];
		int i = 0;
		int arg_count = 0;
		
		//Fill in the **args
		int arg = 0;
		i = 0;
		while((i = p1getword(buf, i, tmp)) != -1){
			args[arg] = p1strdup(tmp);
			arg++;
		}
		args[arg] = NULL;
		
		//Start fork process
		int pid = fork();
		if(pid > 0){
			pidarr[nprograms - 1] = pid;
		}
		if(pid < 0){
			p1perror(2, "FORK FAILED\n");
			free_args(args, arg_count);
			exit(1);
		}
		if(pid == 0){
			while(!usr1_handled){
				sleep(1);
			}
			execvp(args[0], args);
			free(pidarr);
		}

		//free_args(args, arg_count);
		
		for(int i = 0; i <= arg; i++){
			free(args[i]);

		}
		
	}
	
	for(int i = 0; i < nprograms; i++){
		kill(pidarr[i], SIGUSR1);
	}

	for(int i = 0; i < nprograms; i++){
		kill(pidarr[i], SIGSTOP);
	}

	for(int i = 0; i < nprograms; i++){
		kill(pidarr[i], SIGCONT);
	}
	
	int status;
	for(int i = 0; i < nprograms; i++){
		wait(&status);
	}

	close(file);
}
