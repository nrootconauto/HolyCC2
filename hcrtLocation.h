#pragma once
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#define HCRT_LOC_LOCAL "/home/tc/projects/holycc2/HolyCRT/"
#define HCRT_LOC_INSTALLED "share/HolyCRT"
__attribute__((always_inline)) inline char *HCRTFile(const char *file) {
		long len=snprintf(NULL, 0, "%s/%s", HCRT_LOC_LOCAL,file);
		char local[len+1];
		sprintf(local, "%s/%s", HCRT_LOC_LOCAL,file);

		len=snprintf(NULL, 0, "%s/%s", HCRT_LOC_INSTALLED,file);
		char installed[len+1];
		sprintf(installed, "%s/%s", HCRT_LOC_INSTALLED,file);

		printf("LOCAL:%s,INST:%s\n",local,installed);
		
		if(0==access(local, F_OK))
				return strcpy(calloc(strlen(local)+1, 1), local);

		if(0==access(installed, F_OK))
				return strcpy(calloc(strlen(installed)+1, 1), installed);

		fprintf(stderr, "HCRT file\"%s\" not found\n",  file);
		abort();
}
