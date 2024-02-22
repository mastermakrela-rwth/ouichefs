// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	// Open a file
	int fd = open("/mnt/smol.img/bee.txt", O_RDWR);

	if (fd == -1) {
		perror("Failed to open file");
		exit(EXIT_FAILURE);
	}

	printf("File opened successfully\n");

	// Do nothing, just loop infinitely
	while (1)
		;

	// Close the file (This part will never be reached in an infinite loop)
	close(fd);

	return 0;
}
