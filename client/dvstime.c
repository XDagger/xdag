/* insert DVS:time version label; T14.301-T14.301; $DVS:time$;

1. Put pattern into text file:
	... T??.??? ... $DVS:time$ ...
2. Apply this program
	dvstime file.txt

*/

#include <stdio.h>
#include <string.h>
#include <utime.h>
#include <sys/stat.h>

#define SEC_1970_2000	946684800
#define SEC_2000_T0	133080623
#define SEC_T0_T1	31556952

int main(int argc, char **argv) {
	FILE *f;
	char buf[4096], *ptr;
	struct stat s;
	struct utimbuf u;
	long pos;
	if (argc != 2 || stat(argv[1], &s) || !(f = fopen(argv[1], "r+"))) {
		printf("Usage: %s file\n", argv[0]);
		return 1;
	}
	while ((pos = ftell(f)) >= 0 && fgets(buf, 4096, f)) {
		if ((ptr = strstr(buf, "$DVS:time$"))) {
			while (ptr >= buf && *ptr != 'T') ptr--;
			if (ptr < buf) continue;
			fseek(f, pos + (ptr - buf) + 1, SEEK_SET);
			fprintf(f, "%6.3lf", (double)(s.st_mtime - SEC_1970_2000 - SEC_2000_T0) / SEC_T0_T1);
			fclose(f);
			u.actime = s.st_atime;
			u.modtime = s.st_mtime;
			utime(argv[1], &u);
			return 0;
		}
	}
	fclose(f);
	printf("%s: pattern T??.??? $DVS:time$ not found.\n", argv[0]);
	return 0;
} 

