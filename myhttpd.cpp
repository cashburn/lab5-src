#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

int main( int argc, char **argv) {
	int c;
	FILE * fp = fopen("http-root-dir/htdocs/index.html", "r");
	if (fp) {
		while ((c = getc(fp)) != EOF)
			putchar(c);
		fclose(fp);
	}
}

