#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#define MAXPATH 256
#define MAXLEN 128

struct CPUdata {
	unsigned long long user, nice, system, idle_time;
	}; 

int proc_state_scan(int *r, int *t, int *s, int *z, int *total);
int read_cpu_data(struct CPUdata *data);

int main(void){
	struct CPUdata first, second;
	int r, t, s, z, total;
	
	proc_state_scan(&r, &t, &s, &z, &total);
	printf("Количество процессов:"
	"Total: %d, Running: %d, Sleeping: %d, Stopped: %d, Zombie: %d\n", 
	total, r, s, t, z);
	read_cpu_data(&first);
	printf("1 замер CPU(s): user %llu, nice %llu, system %llu, idle_time %llu \n",
	first.user, first.nice, first.system, first.idle_time);
	
	sleep(1);
	read_cpu_data(&second);
	printf("2 замер CPU(s): user %llu, nice %llu, system %llu, idle_time %llu \n",
	second.user, second.nice, second.system, second.idle_time);
	
	proc_state_scan(&r, &t, &s, &z, &total);
	printf("Количество процессов:"
	"Total: %d, Running: %d, Sleeping: %d, Stopped: %d, Zombie: %d\n", 
	total, r, s, t, z);
	
	

	return 0;
}

int proc_state_scan(int *r, int *t, int *s, int *z, int *total){
	
	char line[MAXLEN];
	char proc_path[MAXPATH];
	char dirpath[MAXPATH] = "/proc/";
	struct dirent *entry;
	
	*r = 0; 
	*t = 0;
	*s = 0;
	*z = 0;
	*total = 0; 
	
	DIR *dir;
	dir = opendir(dirpath);
	if (dir == NULL) {
		perror("Ошибка открытия директории");
		return 1;
	}
	
	while ((entry = readdir(dir)) != NULL){
		if (!isdigit(entry->d_name[0]))
			continue;
		snprintf(proc_path, sizeof(proc_path), "/proc/%s/status", entry->d_name);
		//printf("\nproc_path: %s", proc_path);
		
		FILE *fp;
		fp = fopen(proc_path, "r");
		if(!fp)
			continue;
			
		(*total)++;
		
		while (fgets(line, sizeof(line), fp)){
			if(strncmp(line, "State:", 6) == 0){
				char state_sym;
				
				if(sscanf(line, "State: %c", &state_sym) == 1){
					switch(state_sym){
					case 'R': (*r)++;
						break;
					case 'I':
					case 'S':
					case 'D': (*s)++;
						break;
					case 'T':
					case 't': (*t)++;
						break;
					case 'Z': (*z)++;
						break;
					default:
						break;
					}
				} 
				break;	
			}
		}
		fclose(fp);
		
	}
	closedir(dir);
	return 0;
}

int read_cpu_data(struct CPUdata *data){

	char cpu_data_path[MAXPATH] = "/proc/stat";
		
	FILE *fp;
	fp = fopen(cpu_data_path, "r");
	if (!fp){
		perror("Ошибка открытия файла");
		return -1;
	}
	
	int values = fscanf(fp, "cpu  %llu %llu %llu %llu", &data->user, &data->nice, &data->system, &data->idle_time);
	
	fclose(fp);
	
	if (values < 4){
		return -1;
	}
	else
		return 0;
}
