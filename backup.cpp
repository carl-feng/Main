#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <fcntl.h>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <sys/stat.h>
#include <dirent.h>

using namespace std;

bool is_dir(const char *path)
{
    struct stat statbuf;
    if(lstat(path, &statbuf) ==0)
    {
        return S_ISDIR(statbuf.st_mode) != 0;
    }
    return false;
}

bool is_file(const char *path)
{
    struct stat statbuf;
    if(lstat(path, &statbuf) ==0)
        return S_ISREG(statbuf.st_mode) != 0;
    return false;
}

bool is_special_dir(const char *path)
{
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

void get_file_path(const char *path, const char *file_name,  char *file_path)
{
    strcpy(file_path, path);
    if(file_path[strlen(path) - 1] != '/')
        strcat(file_path, "/");
    strcat(file_path, file_name);
}

void delete_file(const char *path)
{
    DIR *dir;
    dirent *dir_info;
    char file_path[PATH_MAX];

    if(is_file(path))
    {
        remove(path);
        return;
    }
    if(is_dir(path))
    {
        if((dir = opendir(path)) == NULL)
        {
            return;
        }
        while((dir_info = readdir(dir)) != NULL)
        {
            get_file_path(path, dir_info->d_name, file_path);
            if(is_special_dir(dir_info->d_name))
                continue;
            delete_file(file_path);
        }
        rmdir(path);
    }
}

bool backup_pic(string path)
{
    char cmd[1024] = {0};
    string filename = path.substr(path.find_last_of('/')+1);
    string date = filename.substr(0, filename.find('-'));
    string dir = "/mnt/tf/";
    dir += date.substr(0, 6);
    if(access(dir.c_str(), 0) == -1)
    {
        mkdir(dir.c_str(), 0777);
    }
    snprintf(cmd, sizeof(cmd), "cp %s %s", path.c_str(), dir.c_str());
    return system(cmd) == 0;
}

bool delete_expired_pic()
{
#define NUM 2
    int months[NUM];
    time_t timep;
    const char* root = "/mnt/tf/";
    time(&timep);
    struct tm *pTM = localtime(&timep);
    for(int i = 0; i < NUM; i++)
    {
        if((pTM->tm_mon + 1 - i) <= 0)
            months[i] = pTM->tm_mon + 1 + 13;
        else
            months[i] = pTM->tm_mon + 1 - i;
    }

    DIR *dp;
    struct dirent *entry;
    
    if((dp = opendir(root)) == NULL) {
        return false;
    }
    while((entry = readdir(dp)) != NULL) {
        if(entry->d_type == 4) {
            if(strcmp(".",entry->d_name) == 0 ||
                strcmp("..",entry->d_name) == 0)
                continue;
            string dir = entry->d_name;
            if(dir.size() == 6)
            {
                int month = atoi(dir.substr(4, 2).c_str());
                bool bFound = false;
                for(int i = 0; i < NUM; i++)
                {
                   if(months[i] == month)
                   {
                       bFound = true;
                       break;
                   }
                }
                if(bFound) 
                {
                   continue;
                }
            }
            string fullpath = root;
            fullpath += dir;
            delete_file(fullpath.c_str());
        }
    }
    closedir(dp);
    return true;
}

/*int main()
{
	const char* file = "/root/Main/20150508-220304.jpg";
	bool ret = backup_pic(file);
	if(ret)
		printf("backup success.\n");
	else
		printf("backup failed.\n");

	delete_expired_pic();

	return 0;
}*/
