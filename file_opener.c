#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
	// Open a file
	int fd = open("/mnt/smol.img/bee.txt", O_RDONLY);
	if (fd == -1) {
		perror("Failed to open file");
		exit(EXIT_FAILURE);
	}

	printf("File opened successfully\n");

	while (1) {
		// Do nothing, just loop infinitely
	}

	// Close the file (This part will never be reached in an infinite loop)
	close(fd);

	return 0;
}