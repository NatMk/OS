/*
 * Name: Natnael Kebede
 * ID #: 1001149004
 * Programming Assignment 5
 * This program implements the beginning of a dropbox-lite file system
 * and provides the user with 5MB of drive space.
*/

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* 5 MB = 5 * 1024 * 1024 */
#define FILESYSTEM_SIZE 5242880 /* 5 MB Disk Space for the file system */
#define MAX_FILE_SIZE 98304 /* Files in bytes the file system supports */
#define MAX_FILES_NUMBER 128 /* Bytes the file system shall support */
#define SYSTEM_BLOCK_SIZE 2048 /* Block size the file system shall support*/
#define MAX_FILENAME_LENGTH 255 /* Size of file names */
#define MAX_DATE_LENGTH 20 /* length of date string */
/* number of blocks according to FILESYSTEM_SIZE and SYSTEM_BLOCK_SIZE */
#define BLOCKS_NUMBER FILESYSTEM_SIZE / SYSTEM_BLOCK_SIZE
/* max length of command name that may be entered by user */
#define MAX_COMMAND_NAME_LENGTH 10
#define INVALID_FILE_PARTITION -1 /* default value of file partition */

/* structure to represent file in system */
typedef struct file
{
  int size; /*file size in bytes */
  char * name; /* file name */
  char cdate[MAX_DATE_LENGTH]; /* date string */
} file_t;

/* structure to indexed block */
typedef struct block
{
  int fpart; /* index of the file part */
  file_t * file; /* poitner to the file object */
} block_t;

/* memory allocated for storing files content */
char filesystem[BLOCKS_NUMBER][SYSTEM_BLOCK_SIZE];
block_t btable[BLOCKS_NUMBER]; /* index table to store blocks */

void initializeBlocks();
char * getLine( FILE * stream );

/* functions to process related command */
void processPutCommand(const char * filename);
void processGetCommand(const char * source, const char * dest);
void processDelCommand(const char * filename, int verbose);
void processListCommand();
void processDFCommand();

void freeBlocks();
int getFreeSpaceNumber();
long getFileSize(const char * file);
int getNextFreeBlock();
int validateFileName(const char * filename);
void fillDate(char date[MAX_DATE_LENGTH]);
int getNumberOfFiles();
block_t * findNextFileBlock(const char * filename, int filePart);

int main()
{

  int running = 1; /* flag variable to indicate if main loop is running */
  char * input = NULL; /* pointer to input string */
  char commandName[MAX_COMMAND_NAME_LENGTH]; /* buffer to store command name */
  int pos; /* variable to store input position after command name */
  /* buffers to store files names for get command */
  char dest[MAX_FILENAME_LENGTH], source[MAX_FILENAME_LENGTH];

  initializeBlocks();  /* initialize block values as expected */

  do
  {
    printf("mfs> "); /* indicates readyness for user command input */
    input = getLine( stdin ); /* gets user input and stores it in input */

    if ( !input ) /* if the user keeps hitting enter key keep looping */
    {
      continue;
    }

    pos = 0;
    /* parse input text for command name */
    sscanf( input, " %10s %n", commandName, &pos );

    /* check for valid commands: put and get then call methods*/
    if ( strcmp( commandName, "put" ) == 0 )
    {
      processPutCommand( input + pos ); /*pass arguments for processing */
    }
    else if ( strcmp( commandName, "get" ) == 0 )
    {
      source[0] = '\0'; /* clear file names */
      dest[0] = '\0';

      /* try parse arguments */
      if ( sscanf( input + pos, "%s %s", source, dest) == 1 )
      {
        processGetCommand( source, "" );
      }
      else
      {
        processGetCommand( source, dest );
      }
    }
    /* check for valid commands: del, list and df then call methods*/
    else if ( strcmp( commandName, "del" ) == 0 )
    {
      processDelCommand(input + pos, 1);
    }
    else if ( strcmp( commandName, "list" ) == 0 )
    {
      processListCommand();
    }
    else if ( strcmp( commandName, "df" ) == 0 )
    {
      processDFCommand();
    }
    /* commands to end the program: quit or exit */
    else if ( ( strcmp( commandName, "exit" ) == 0 ) 
	    || ( strcmp(commandName, "quit") == 0) )
    {
      running = 0;  /* quit program */
    }
    else
    {
	printf("invalid command\n");/* in case the user types other commands*/
    }
    free(input); /* deallocate input string buffer */
  } 
  while(running);
  freeBlocks();/* empty blocks */
  return 0;
}

/* Function: initializeBlocks
 * Parameter(s): none
`* Returns: none
 * Description: This function allows proper initialization
 * of the btable variable 
 */
void initializeBlocks()
{
  int i = 0;
  for(; i < BLOCKS_NUMBER; ++i)
  {
    btable[i].fpart = INVALID_FILE_PARTITION;
    btable[i].file = NULL;
  }
}
/* Function: freeBlocks
 * Parameter(s): none
`* Returns: none
 * Description: This function allows to free occupied memory by blocks 
 */
void freeBlocks()
{
  int i = 0;
  for(; i < BLOCKS_NUMBER; ++i)
  {
    if (btable[i].file && btable[i].fpart == 0)
    {
      free(btable[i].file->name);
      free(btable[i].file);
    }
  }
}
/* Function: getLine
 * Parameter(s): stream - A file ponter representing the file we get data from
`* Returns: A char * representing the string than has been reallocated in size 
 * Description: This function allows to get line from stream. 
 * The returned pointer should then be freed 
 */
char * getLine( FILE * stream )
{
  const int LINE_SIZE = 32;
  char * str = NULL;
  int ch, size = 0;
  size_t len = 0;
  
  while( EOF != ( ch = fgetc( stream ) ) && ch != '\n' )
  {
      if( len + 1 >= size ) 
      {
          str = realloc( str, sizeof( char )*( size += LINE_SIZE ) );

          if( !str ) 
          {
	    return str;
          }
      }
      str[len++] = ch;
  }
  
  if ( str ) 
  {
    str[len++] = '\0';
    str = realloc( str, sizeof( char ) * len );
  }
  return str;
}
/* Function: getFreeSpaceNumber
 * Parameter(s): none
`* Returns: FILESYSTEM_SIZE - result - An integer value represnting 
 * the freed amount
 * Description: This function allows to get an amount of free space 
 */
int getFreeSpaceNumber()
{
  int i = 0;
  int result = 0;
  for (; i < BLOCKS_NUMBER; ++i)
  {
    result += btable[i].file ? SYSTEM_BLOCK_SIZE : 0;
  }

  return  FILESYSTEM_SIZE - result;
}

/* Function: getFileSize
 * Parameter(s): file - A const char that represents a pointer to the 
 * file being processed.
`* Returns: reult - an integer represnting the current file position for 
 * the stream pointed to by the stream. 
 * Description: This function allows to get file size in filesystem 
 */
long getFileSize(const char * file)
{
  long result = 0;
  FILE* fp = fopen( file, "r" );
  if( fp )
  {
    fseek( fp, 0 , SEEK_END );
    result = ftell( fp );
    fclose( fp );
  }
  return result;
}
/* Function: getNextFreeBlock
 * Parameter(s): none
`* Returns: an intger represnting free space sucess or failure for 
 * the next block 
 * Description: This function allows to get the index of next free block 
 */
int getNextFreeBlock()
{
  int i =0;
  for(; i < BLOCKS_NUMBER; ++i)
  {
    if ( !btable[i].file ) return i;
  }
  return -1;
}
/* Function: validateFileName
 * Parameter(s): filename - A cons char * represnting a file that 
 * will be checked for correct naming
`* Returns: an integer indicating correct file name or not
 * Description: This function allows to check if provided file name 
 * was valid or not
 */
int validateFileName(const char * filename)
{
  int length = strlen( filename );
  int i, dotPos = -1;

  for ( i = 0; i < length; ++i)
  {
    if ( !isdigit( filename[i] ) && !isalpha( filename[i] ) && 
	(filename[i] == '.' && dotPos >= 0))
    {

      return 0;
    }

    /* checks for file names with alphanumeric . */
    if (dotPos < 0 && filename[i] == '.')
    {
        dotPos = i;
    }  
}

  return dotPos > 0 && dotPos + 1 < length;
}
/* Function: fillDate
 * Parameter(s): date - A char with a length to contain the date
`* Returns: none
 * Description: This function allows to fill date array with current date 
 */
void fillDate(char date[MAX_DATE_LENGTH])
{
  time_t now = time(NULL);
  struct tm * t = localtime(&now);
  strftime(date, MAX_DATE_LENGTH - 1, "%b %d %H:%M", t);
}
/* Function: processPutCommand
 * Parameter(s): filename - A const char* represnting the file that the user
 * entered with the put command
`* Returns: none
 * Description: This function allows processing of the file for the put command
 * it checks for the file validity, space availabe to store the file and the 
 * possiblity of keeping it or not
 */
void processPutCommand(const char * filename)
{
  int length, readNumber, i;
  int nextFreeBlock;
  FILE * fin;
  int freeSpace = getFreeSpaceNumber();
  int filesize = getFileSize(filename);
  file_t * file = NULL;

  /* if file exists - remove it*/
  processDelCommand(filename, 0);

  /* check free space */
  if ( freeSpace < filesize || filesize > MAX_FILE_SIZE )
  {
    printf("put error: Not enough disk space.\n");
    return;
  }

  if ( getNumberOfFiles() + 1 > MAX_FILES_NUMBER )
  {
    printf("put error: Limit of files number reached.\n");
    return;
  }

  /* check max name length */
  length = strlen( filename );
  if ( length > MAX_FILENAME_LENGTH )
  {
    printf("put error: File name too long.\n");
    return;
  }
  
  if ( !validateFileName( filename) )
  {
    printf("put error: Invalid file name.\n");
    return;
  }

  /* try open file */
  fin = fopen( filename, "r");
  if ( !fin )
  {
    printf("put error: file cannot be opened.\n");
    return;
  }

  do
  {
    /* try create file instance and file name field */
    file = malloc( sizeof ( file_t ) );
    if (!file) 
    {
      break;
    }
    file->name = malloc( sizeof( char ) * length );
    if (file->name)
    {
      strcpy( file->name, filename );
    }
    else
    {
      free( file );
      break;
    }

    i = 0;
    file->size = filesize;
    fillDate(file->cdate);
    do /* read file content block by block */
    {
      nextFreeBlock = getNextFreeBlock();
      readNumber = fread( filesystem[nextFreeBlock], 
		sizeof(char),SYSTEM_BLOCK_SIZE,fin );
      btable[nextFreeBlock].fpart = i++;
      btable[nextFreeBlock].file = file;
    }while ( readNumber == SYSTEM_BLOCK_SIZE );
 }   while ( 0 );
 
    fclose( fin );  /* close input file */
}
/* Function: findNextFileBlock
 * Parameter(s): filename - A const char* s represnting the file to be
 * processed for space availabilty and filePart- an integer represnting 
 * the size of the file for stuct block to comapre with index of file part
`* Returns: A structure of type block for success otherwise NULL
 * Description: This function allows to occupied block by file (filename) 
 * and represents N-th part 
 */
block_t * findNextFileBlock(const char * filename, int filePart)
{
  int i = 0;

  for(; i < BLOCKS_NUMBER; ++i)
  {
    if (btable[i].file && btable[i].fpart == filePart && 
	strcmp ( btable[i].file->name, filename ) == 0)
     {
        return &btable[i];
     }
  }
  return NULL;
}
/* Function: processGetCommand
 * Parameter(s): source and dest - A const char* s represnting the source 
 * and destination of the file that the user inputs for the get command
`* Returns: none
 * Description: This function allows processing of the file for the get command
 * it checks for the file validity, space availabe to store the file in 
 * the destination file or not
 */
void processGetCommand(const char * source, const char * dest)
{
  block_t * block = findNextFileBlock( source, 0);
  FILE * fout;
  int writtenBytes, bytesToWrite, i = 0;

  if ( !block )
  {
    printf( "get error: File not found.\n" );
    return;
  }

  if ( strlen(dest) == 0 )
  {
    dest = source;
  }

  fout = fopen( dest, "w" );
  if ( !fout )
  {
    printf( "get error: Cannot create destination file.\n" );
    return;
  }

  writtenBytes = 0;
  do
  {
    /* calculate number of bytes to write */
    bytesToWrite = block->file->size - writtenBytes > SYSTEM_BLOCK_SIZE ?
                   SYSTEM_BLOCK_SIZE : block->file->size - writtenBytes;

    /* write file content */
    fwrite ( filesystem[block - btable], sizeof( char ), bytesToWrite, fout );
    /* store number of written bytes to detecxt file end */
    writtenBytes += bytesToWrite;
    /* find next occupied block */
    block = findNextFileBlock( source, ++i);
  } 
  while (block);
  fclose( fout ); /* close destination file */
}
/* Function: processDelCommand
 * Parameter(s): filename - A const char* represnting the file to be processed 
 * the user inputs for the Del command and verbose - an int represnting if the 
 * is still stored or existing
`* Returns: none
 * Description: This function allows processing of the file for the del command
 * it checks for the file avilability in the file system or not
 */
void processDelCommand(const char * filename, int verbose)
{
  int i, removed = 0;
  for (i = 0; i < BLOCKS_NUMBER; ++i)
  {
    if (btable[i].file && strcmp(btable[i].file->name, filename) == 0)
    {
      if (btable[i].fpart == 0)
      {
        free(btable[i].file->name);
        free(btable[i].file);
      }
      btable[i].fpart = -1;
      btable[i].file = NULL;
      removed = 1;
    }
  }
  if (verbose && !removed)
  {
    printf("del error: File not found.\n");
  }
}
/* Function: processListCommand
 * Parameter(s): none
`* Returns: none
 * Description: This function checks for the reaming files stored in the file
 * system
 */
void processListCommand()
{
  int i, atLeastOne = 0;
  for (i = 0; i < BLOCKS_NUMBER; ++i)
  {
    if (btable[i].file && btable[i].fpart == 0)
    {
      printf("%-6d %15s %s\n", btable[i].file->size, btable[i].file->cdate, 
	     btable[i].file->name);
      atLeastOne = 1;
    }
  }
  if ( !atLeastOne )
  {
    printf("list: No files found.\n");
  }
}
/* Function: processDFCommand
 * Parameter(s): none
`* Returns: none
 * Description: This function allows processing of the file for the df command
 * it prints the number of free space availabe by subtracting from the orignal
 * allocated file system size
 */
void processDFCommand()
{
  int i, spaceOccupied = 0;
  for (i = 0; i < BLOCKS_NUMBER; ++i)
  {
    if (btable[i].file && btable[i].fpart == 0)
    {
      spaceOccupied += btable[i].file->size;
    }
  }
  printf("%d bytes free.\n", FILESYSTEM_SIZE - spaceOccupied);
}

/* Function: getNumberOfFiles
 * Parameter(s): none
`* Returns: result- an int represnting the number of stored files
 * Description: This function allows to get the number of stored files 
 */
int getNumberOfFiles()
{
  int i, result = 0;
  for (i = 0; i < BLOCKS_NUMBER; ++i)
  {
    if (btable[i].file && btable[i].fpart == 0)
    {
      result += 1;
    }
  }
  return result;
}

