#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	ssize_t bytes_written;
	int fd;

	//logging
	openlog("aesd-assignment-2", LOG_PID, LOG_USER);

	syslog(LOG_DEBUG, "Writing <string> %s to <file> %s",argv[2],argv[1]);

	//check the number of arguments
	if(argc != 3)
	{
		syslog(LOG_ERR, "Not sufficient arguments or too many arguments");
		closelog();
		return 1;
	}
	
	//create a file inside the directory
	fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 777);
	if(fd == -1)
	{
		syslog(LOG_ERR, "Cannot open file");
		closelog();
		return 1;
	}
	bytes_written = write(fd, argv[2], strlen(argv[2]));

	if(bytes_written == -1)
	{
		syslog(LOG_ERR, "Not able to write");
		closelog();
		return 1;
	}
	//clean up
	close(fd);
	closelog();

	return 0;



}
