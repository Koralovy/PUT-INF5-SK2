#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>

void childend(int signo) {wait(NULL);}

int recv_msg(char** buffer, size_t sizeof_buffer, int cfd);

void ack_wait(int sfd, char* ack_buf)
{
	recv_msg(&ack_buf,5,sfd);
	while(strncmp(ack_buf, "ack\n", 4)!=0)
		recv_msg(&ack_buf,5,sfd);
	free(ack_buf);
	ack_buf = (char*)malloc(sizeof(char)*5);
}

int recv_msg(char** buffer, size_t sizeof_buffer, int cfd)
{
	// free and NULL the buffer before reallocating the memory
	free(*buffer);
	*buffer = NULL;
	
	// reallocate the buffer
	*buffer = (char*)malloc(sizeof(char)*sizeof_buffer);
	
	// read in a loop - until it receives the end of a message which is a \n
	// and calculate length of the message
	int rc = read(cfd, *buffer, sizeof_buffer);
	while(memchr((void*)*buffer, '\n', rc) == NULL)
		rc += read(cfd, (*buffer)+rc, sizeof_buffer-rc);

	// return the calculated length
	return rc;
}

int send_msg(char* msg, size_t msg_size, int cfd)
{
	// write in a loop - until it sends the required amount of bytes
	int wc = write(cfd, msg, msg_size);
	while(wc != msg_size)
		wc += write(cfd, msg+wc, msg_size - wc);
	
	// return the amount of bytes sent
	return wc;
}

int main(int argc, char** argv)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	// ready-to-use code from the Lab to free the socket
	int on = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
	signal(SIGCHLD, childend);

	// declare the socket address
	struct sockaddr_in saddr;
	saddr.sin_family = PF_INET;
	saddr.sin_port = htons(2137);
	saddr.sin_addr.s_addr = INADDR_ANY;
	bind(fd, (struct sockaddr*)&saddr, sizeof(saddr));

	// listen on the file descriptor
	listen(fd, 5);
	struct sockaddr_in caddr;
	socklen_t s = sizeof(caddr);
	
	// allocate the message buffer and receive length variable
	char* buffer = (char*)malloc(sizeof(char)*100);
	int rc;
	
	while(1)
	{
		// accept an incoming connection and fork
		int cfd = accept(fd, (struct sockaddr*)&caddr, &s);
		int ret = fork();
		if(ret > 0)
		{
			// in the parent process - return to accepting connections
			// and the close previous one
			close(cfd);
		}
		else if(ret == 0)
		{
			// initialize variables used during temporary work directory creation
			struct stat st = {0};
			srand(time(NULL));
			int rand_append = rand() % 1000;
			char* dir_name;
			dir_name = (char*)malloc(sizeof(char)*100);
			
			// generate a random directory name with unique 3-digit number id
			sprintf(dir_name, "./workdir%03d", rand_append);
			while(stat(dir_name, &st) != -1) 
			{
				rand_append = rand() % 1000;
				sprintf(dir_name, "./workdir%03d", rand_append);
			}

			// make the directory
			printf("making temporary directory: %s\n", dir_name);
			mkdir(dir_name, 0770);
			chdir(dir_name);
			
			// free and null the directory name buffer
			free(dir_name);
			dir_name = NULL;

			// make a files/ directory inside the workdir
			mkdir("./files", 0770);
			chdir("./files");
			
			//close the fd copied from the parent process
			close(fd);

			// receive the first message 
			// and print new connection and read count (RC) info
			rc = recv_msg(&buffer, 100, cfd);
			printf("RC: %d\n", rc);
			printf("new connection: %s\n", inet_ntoa((struct in_addr)caddr.sin_addr));
			printf(buffer);
			printf("\n");
			
			// the first command should start with 'a' - declaration of an archive
			// if it is not, kill the child 
			if(buffer[0] != 'a')
			{
				printf("Wrong command, specify archive name with a <filename>.zip\n");
			}
			else
			{
				// declare and malloc the archive name buffer
				char* name;
				name = (char*)malloc(sizeof(char)*100);

				// copy the name from the message buffer to the name buffer
				// tokenized until '\n' and without the first 2 characters
				// (which should be 'a' and ' ') 
				strcpy(name, strtok((buffer+2*sizeof(char)),(const char*)"\n"));
				printf("Archive name: %s\n",name);
				
				// send an 'ack' (acknowledged) message
				send_msg("ack\n", 5, cfd);

				// receive the next command and print the RC
				rc = recv_msg(&buffer, 100, cfd);
				printf("RC: %d\n", rc);
				
				// declare all the variables used inside the switch statement - 
				// newer gcc compilers don't even complain on -Wall about declaring in switches
				// (tested on gcc 11.1.0), but the version used on OpenSuse found on the polluks server
				// and lab computers (gcc 7.5.0) treat is as an outright error, not just a warning
				char* token;
				const char delim[] = " ";
				char* filename = NULL;
				char* last_token=NULL;
				size_t size;
				char* file = NULL;
				FILE* fp;
				char* cd_name = NULL;

				// while you have not received an archive end message - process messages
				while(strncmp(buffer, "a end\n", 6) != 0)
				{
					// send an ack for the message received at the end of previous iteration
					// and print the buffer (tokenized to a '\n') to the terminal
					send_msg("ack\n", 5, cfd);
					printf("\tbuf peek: %s, size: %d\n", strtok(buffer,(const char*)"\n"), rc);
					
					// switch on the first character - 'f' will declare a file and 'd' will declare a
					// directory change
					switch (buffer[0])
					{
					// file declaration command
					case 'f':
						// tokenize the command - it has a format "f filename size_in_bytes"
						token = strtok(buffer+2*sizeof(char), delim);
						filename = (char*)malloc(sizeof(char)*100);
						strcpy(filename, token);

						// this tokenizer loop deals with spaces inside of filenames
						// it will save the last token and not append it, since the last
						// space-deliminated token is the file size
						token = strtok(NULL, delim);
						do
						{
							if(last_token!=NULL)
							{
								strcat(filename, " ");
								strcat(filename, last_token);
							}
							last_token = token;
							token = strtok(NULL, delim);
						}while(token != NULL);

						// the aforementioned last token that contains the file size
						// converted to a size_t from a string (char*)
						size = atoi(last_token);
						printf("\t\tfile tokenized description: \n\t\t\tfilename: %s\n\t\t\tsize: %ld\n", filename, size);

						// send an 'ack' (acknowledged) message
						send_msg("ack\n", 5, cfd);

						// malloc a buffer for the file
						file = (char*)malloc(sizeof(char)*size);
						
						// read the incoming file and do so in a loop
						// in case the socket does not contain it all
						rc = read(cfd, file, size);
						while(rc!=size)
						{
							printf("\t\tdoing repeats after %d bytes\n", rc);
							rc += read(cfd, file+rc, size-rc);
						}

						// write the received file to disk
						fp = fopen(filename, "wb");
						fwrite(file, size,sizeof(char), fp);
						
						// free and NULL all the used pointers
						free(filename);
						filename = NULL;
						
						free(file);
						file=NULL;

						// close the written file
						fclose(fp);
						break;
					// directory change command
					case 'd':
						// malloc the buffer for the directory name
						// and copy the name from the message buffer
						cd_name = (char*)malloc(sizeof(char)*100);
						strcpy(cd_name, buffer+2);
						printf("\t\tchanging directory to %s\n", cd_name);
						
						// if the directory does not exist - create it
						if(stat(cd_name, &st) == -1)
							mkdir(cd_name, 0770);
						
						// switch to the requested direcotry
						chdir(cd_name);

						// free and NULL the name buffer
						free(cd_name);
						cd_name = NULL;
						break;
					
					// in case the client does not comply with the specification
					// print an error about a wrong command being sent
					default:
						printf("\t\tWrong command\n");
						break;
					}

					// at the end of the loop, after processing, receive a new message
					rc = recv_msg(&buffer, 100, cfd);
				}
				// after a 'a end' (archive end) message was received, start archive processing 
				printf("\t\"%s\" signal received, processing archive\n", strtok(buffer, (const char*)"\n"));
				
				// for debug purposes check and print the full current working directory
				char cwd[1000];
				if (getcwd(cwd, sizeof(cwd)) != NULL) 
				{
					printf("\tCurrent working dir: %s\n", cwd);
				} 
				else 
				{
					perror("getcwd() error");
				}

				// free and NULL the message buffer
				free(buffer);
				buffer = NULL;

				// send an ack about the "a end" message
				send_msg("ack\n", 5, cfd);

				// malloc the double-pointer array for argv for the zip process
				char** dst_argv = malloc(sizeof(char*) * 5);
				
				// fork the child into what will become /usr/bin/zip and the rest
				// of the processing routine
				pid_t zip_ret = fork();
				if(zip_ret > 0)
				{
					// wait for zip to finish work
					// (wait on child process to finish execution)
					wait(NULL);

					// free and NULL the zip argv buffer
					free(dst_argv);
					dst_argv = NULL;

					// allocate and create a command that will be sent to the client
					// conatining a file name and size in the same standard as before
					printf("\t%s archive created, sending\n", name);
					char* ack_buf = (char*)malloc(sizeof(char)*5);
					char* command = (char*)malloc(sizeof(char)*1000);
					
					// exit the files subdirectory
					chdir("..");
					
					// open the archive
					FILE* fp = fopen(name, "rb");
					if(fp == NULL)
					{
						printf("Archive read error, exitting\n");
						exit(-3);
					}
					// check the archive size - seek the end and calculate the distance
					fseek(fp, 0L, SEEK_END);
					size_t archive_size = ftell(fp);
					// go back to the beginning
					rewind(fp);
					
					// create the command with a format: "f archive_name archive_size"
					size_t command_size = sprintf(command, "f %s %ld\n", name, archive_size);
					printf("\tCommand: %s\t\tcommand size: %ld\n",command, command_size);
					
					// send that command anw wait for ack
					send_msg(command, command_size, cfd);
					ack_wait(cfd, ack_buf);
					printf("\tAck ack'd\n");

					// allocate a buffer and load the archive into memory 
					char* archive = (char*)malloc(sizeof(char)*archive_size);
					fread(archive, sizeof(char), archive_size, fp);
					
					// send the archive and wait for ack
					send_msg(archive, archive_size, cfd);
					ack_wait(cfd, ack_buf);
					
					// free and NULL all the utilized pointers
					free(archive);
					archive = NULL;
					free(ack_buf);
					ack_buf = NULL;
					free(command);
					command = NULL;
				}
				// If you are the child - start preparing for execve()
				else if(zip_ret == 0)
				{
					// allocate the buffers for dst_argv arguments for execve()
					char call[] = "/usr/bin/zip";
					char* path = (char*)malloc(sizeof(char)*1000);
					char dash_r[] = "-rv";
					char dot[] = "./";

					// create the relative path for the archive outside the files/ subdirectory
					int scnt = sprintf(path, "../%s",name); 
					printf("\tPre-exec sprintf byte copy num: %d\n", scnt);

					// prepare dst_argv with created variables and (as per specification) a terminating NULL
					dst_argv[0] = call;
					dst_argv[1] = path;
					dst_argv[2] = dash_r;
					dst_argv[3] = dot;
					dst_argv[4] = NULL;
					printf("\tZip execve call incoming with:\n\t\tcall: %s\n\t\tpath: %s\n\t\tdash_r: %s\n\t\tdot: %s\n",call, path, dash_r, dot);
					
					// call execve() with the path to /usr/bin/zip, the argv and no environment
					int ret = execve("/usr/bin/zip",dst_argv, NULL);

					// in case execve fails - print a message about it with exit code
					printf("This should not be printed\nExecve crashed with the status: %d\n", ret);
					exit(-3);
				}
				// if there was a fork error - exit
				else
				{
					printf("Fork error before execve call, exitting\n");
					exit(-2);
				}

				// free and NULL the archive name buffer
				free(name);
				name=NULL;
			}
			// close the socket fd end exit gracefully
			printf("\tCommunications finished, child exitting\n");
			close(cfd);
			exit(0);
		}
		else
		{
			// exit in case there was a fork error on the fork after a connection
			exit(-1);
		}
		
	}
	return 0;
}
