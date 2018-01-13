
/*
 Name: Natnael Kebede
 ID: 1001149004
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

//we want to split our command line up into tokens
//so we need tp define what delimits our tokens
// In this case white space will separate the tokens on our command line
#define WHITESPACE " \t\n"      
#define MAX_COMMAND_SIZE 255 // The maximum command-line size
#define MAX_NUM_ARGUMENTS 10 // Mav shell only supports ten args		
#define MAX_NUM_PID_ID 10 // Mav shell only supports upto ten pids 

int PID_Count = 0; //keeps track of all the PIDs
int PIDS[MAX_NUM_PID_ID]; //array that contains all the PIDs for the user typed commands

void Display_pids(int current_PID);

int main()
{
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  
  while( 1 )
  {
    printf ("msh> "); // Print out the msh prompt
     
     /**
      Read the command from the command line. 
      The max command that will be read is MAX_COMMAND_SIZE. 
      The while commnad will wait here until the user inputs something 
      since fgets returns NULL when there is no input.
      **/

    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    char *token[MAX_NUM_ARGUMENTS];
    int   token_count = 0;
    int   status;                               
    char *arg_ptr;                                                                                                    
    char *working_str  = strdup( cmd_str );                
    char *working_root = working_str;

    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && (token_count<MAX_NUM_ARGUMENTS))
    {
      	token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );

       	if( strlen( token[token_count] ) == 0 ) //in case the token from the string is empty
       	{
            token[token_count] = NULL;
       	}
	
	token_count++;  // keep looping until we reach the end of user input	
    }

       if (token[0] == NULL)	// if user keep hitting enter keep looping
       {
	   continue; // keep looping again with the while(1) loop
       }

       if ((strcmp(token[0],"quit") == 0) || (strcmp(token[0],"exit") == 0) ) //if the user types quit or exit 
       {
	   exit(0); //we want to end the program
       }

       if ((strcmp(token[0],"cd") == 0)) //if the user types cd command
       {  
               chdir(token[1]); // change the existing directory to the directory they specified (i.e token[1])
	       continue; // keep looping again with the while(1) loop
       }
       if ((strcmp(token[0],"history") == 0)) //if the user types the history command
       {  
      	    continue; // keep looping again with the while(1) loop   
       }

       pid_t child_pid = fork(); //creates a child and parent process
    
       int current_PID = getpid();
       Display_pids(current_PID);//process the existing pids for the cpmmands the user typed
       PID_Count ++; //updates the total pids for PIDs array 
	
       if ((strcmp(token[0],"showpids") == 0)) //if the user types the showpids command
       {  
	   int j;   
		
	  for (j = 0; j < PID_Count; j++) //loop through the array of PIDS to print out the stored PIDS
     	  {
     		printf("%d: %d\n",j,PIDS[j]); //print the PIds for upto the last 10 commands the user typed
     	  }

              continue; // keep looping again with the while(1) loop  
       }

        /**
          Allocate memory for each diectory the command the user typed should work
	  each direcory has the file path for the commands 	  
	  attach the command for each file path for execution	  
        **/
       char *Directory1 = (char*) malloc( MAX_COMMAND_SIZE);
       strncpy(Directory1, "/msh_solution/", strlen("/msh_solution/"));
       strncat(Directory1,token[0], strlen("/msh_solution/") );

       char *Directory2 = (char*) malloc( MAX_COMMAND_SIZE);
       strncpy(Directory2,"/usr/local/bin/", strlen("/usr/local/bin/"));
       strncat(Directory2,token[0], strlen("/usr/local/bin/"));

       char *Directory3 = (char*) malloc( MAX_COMMAND_SIZE);
       strncpy(Directory3,"/usr/bin/", strlen("/usr/bin/"));
       strncat(Directory3,token[0],strlen("/usr/bin/"));
	
       char *Directory4 = (char*) malloc( MAX_COMMAND_SIZE);
       strncpy(Directory4,"/bin/", strlen("/bin/"));
       strncat(Directory4,token[0],strlen("/bin/")); 
 
       if (child_pid > 0) //means a parent process has been created
       {
	  waitpid(child_pid, &status, 0); //waits for the child process 
       }
       else if (child_pid < 0) //means no process has been created
       {
           perror("Error while forking\n");
	   exit(0); //we want to end the program
       }	
       else //means child pid == 0 so we create a process
       {
	 //execv executes a file diectory we specfied for the command the user typed
       	 execv (Directory1, token);
	 execv (Directory2, token);
       	 execv (Directory3, token);
       	 execv (Directory4, token); 
         printf("Command not found\n"); //prints this in case the command the user typed doesn't exist
         exit(0); //exit the program
       }  	
	//free each allocated memory for the directory when done with the commands
        free (Directory1);
	free (Directory2);
	free (Directory3);
	free (Directory4); 
	free (working_root);
  }
	return 0; // end of main method
}

/**
  This method processes the PID numbers for the commands 
  the user typed so that we can populate the PIDs array
**/	
void Display_pids(int current_PID)
{

     if (MAX_NUM_PID_ID > PID_Count) //checks if the total PIDS for the commands the user typed is upto ten 
     {
	PIDS[PID_Count] = current_PID; //sets the existing PID number to the number we found from getpid()
     }

     if (PID_Count >= 10) //check if the we have more than 10 commands inorder to store the last 10 commands 
     {  
	int i = 1;

     	while(i < 11) //loops through max of 10 pid commands
	{
	    PIDS[i-1] = PIDS[i]; //adjust the index inorder to save the last 10 commands
	    i++; 		
	} 
     }

}



