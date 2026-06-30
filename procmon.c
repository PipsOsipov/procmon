#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define MAXPATH 256
#define MAXLEN 128
#define MAXPROC 2048

struct ProcessInfoStable{
	int pid;
	char state;
	unsigned long long virt;
	unsigned long long res;
	unsigned long long shr;
	float perc_mem;
};

struct CPUdata {
	unsigned long long user, nice, system, idle_time;
	}; 
	
struct RAMdata {
	unsigned long long mem_total, mem_free, mem_buff, mem_cache, mem_krecl; 
	};

int read_ram_data(struct RAMdata *data);
int proc_state_mem_scan(struct ProcessInfoStable process[], int *r, int *t, int *s, int *z, int *total);
int read_cpu_data(struct CPUdata *data);

int main(void){
	struct CPUdata first, second;
	struct RAMdata ram;
	struct ProcessInfoStable proc_info[MAXPROC];
	int r, t, s, z, total;
	
	
	/*proc_state_scan(&r, &t, &s, &z, &total);
	printf("Количество процессов:"
	"Total: %d, Running: %d, Sleeping: %d, Stopped: %d, Zombie: %d\n", 
	total, r, s, t, z);*/
	read_cpu_data(&first);
	/*printf("1 замер CPU(s): user %llu, nice %llu, system %llu, idle_time %llu \n",
	first.user, first.nice, first.system, first.idle_time);*/
	
	while(1){
		
		sleep(1);
		
		if(read_cpu_data(&second) != 0){
			break;
		}
		/*printf("2 замер CPU(s): user %llu, nice %llu, system %llu, idle_time %llu \n",
		second.user, second.nice, second.system, second.idle_time);*/
		proc_state_mem_scan(proc_info, &r, &t, &s, &z, &total);
		
		read_ram_data(&ram);
		
		
		printf("\033[H\033[J");
		
		printf("Tasks: Total: %d, Running: %d, Sleeping: %d, Stopped: %d, Zombie: %d\n", 
		total, r, s, t, z);
		
		unsigned int d_user   = (unsigned int)(second.user - first.user);
		unsigned int d_nice   = (unsigned int)(second.nice - first.nice);
		unsigned int d_system = (unsigned int)(second.system - first.system);
		unsigned int d_idle   = (unsigned int)(second.idle_time - first.idle_time);

		unsigned int d_total  = d_user + d_nice + d_system + d_idle;
		
		if (d_total > 0){
			double cpu_total = (1.0 - ((double) d_idle/d_total)) * 100;
			
			double cpu_user = ((double) d_user/d_total) * 100;
			
			double cpu_nice = ((double) d_nice/d_total) * 100;
			
			double cpu_system = ((double) d_system/d_total) * 100;
		
			printf("%%CPU(s): total %5.1f%% | user %5.1f%% | system %5.1f%% | nice %5.1f%% | idle %5.1f%%\n", 
			cpu_total, cpu_user, cpu_system, cpu_nice, (double)d_idle / d_total * 100.0);

		} else{
			printf("CPU(s): 0%%");
		}
		
		
		unsigned long long mem_buff_cache = ram.mem_cache + ram.mem_buff + ram.mem_krecl;
		unsigned long long mem_used = ram.mem_total - ram.mem_free - mem_buff_cache;
		printf("MiB Mem: %6.1f total, %6.1f free, %6.1f used, %6.1f buff/cache\n\n", 
		(double)ram.mem_total/1024, (double)ram.mem_free/1024, (double)mem_used/1024, (double)mem_buff_cache/1024);
		first = second;
		printf("PID	STATE	VIRT	RES	SHR	%%MEM\n");
		for (int i = 0; i<=MAXPROC; i++){
			if (proc_info[i].pid == 0)
				break;	
			printf("%d	%c	%llu	%llu	%llu	%.2f\n", 
			proc_info[i].pid, proc_info[i].state, 
			proc_info[i].virt,proc_info[i].res, 
			proc_info[i].shr, proc_info[i].perc_mem);
		}
	}
	return 0;
}

int proc_state_mem_scan(struct ProcessInfoStable process[], int *r, int *t, int *s, int *z, int *total){
	
	char line[MAXLEN];
	char proc_path[MAXPATH];
	char dirpath[MAXPATH] = "/proc/";
	int count = 0;
	char statm_path[MAXLEN];
	unsigned long long page_size = sysconf(_SC_PAGESIZE);
	unsigned long long shr_pages = 0;
	unsigned long long res_pages = 0;
	unsigned long long virt_pages = 0;
	int pid = 0;
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
			
		if (count >= MAXPROC)
			break;
		
		pid = atoi(entry->d_name);
		
		snprintf(proc_path, sizeof(proc_path), "/proc/%d/status", pid);
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
					process[count].state = state_sym;
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
						process[count].state = 'X';
						break;
					}
				} 
				break;	
			}
		}
		fclose(fp);
		
		snprintf(statm_path, sizeof(statm_path), "/proc/%d/statm", pid);
		fp = fopen(statm_path, "r");
		
		if (!fp){
			perror("Ошибка открытия файла");
			return -1;
		}
		if (fscanf(fp, "%llu %llu %llu", &virt_pages, &res_pages, &shr_pages) != 3){
			res_pages = 0;
			virt_pages= 0;
			shr_pages = 0;
		}
		fclose(fp);
		
		process[count].pid = pid;
		process[count].virt = virt_pages * (page_size/1024);
		process[count].res = res_pages * (page_size/1024);
		process[count].shr = shr_pages * (page_size/1024);
		process[count].perc_mem = 0.0f;
		
		count++;
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
int read_ram_data(struct RAMdata *data){

	char mem_path[MAXPATH] = "/proc/meminfo";
	char line[MAXLEN];

	FILE *fp;
	fp = fopen(mem_path, "r");
	if (!fp){
		perror("Ошибка открытия файла ");
		return -1;
	}
	
	memset(data, 0, sizeof(struct RAMdata));
	
	while (fgets(line, sizeof(line), fp)){
		if (strncmp(line, "MemTotal:", 9) == 0){
			sscanf(line, "MemTotal: %llu", &data->mem_total);
		} else if (strncmp(line, "MemFree:", 8) == 0){
			sscanf(line, "MemFree: %llu", &data->mem_free);
		} else if (strncmp(line, "Buffers:", 8) == 0){
			sscanf(line, "Buffers: %llu", &data->mem_buff);
		} else if (strncmp(line, "Cached:", 7) == 0){
			sscanf(line, "Cached: %llu", &data->mem_cache);
		} else if (strncmp(line, "KReclaimable:", 13) == 0){
			sscanf(line, "KReclaimable: %llu", &data->mem_krecl);
		} 
	
	}
	fclose(fp);
	return 0;
}
