/*
 * Copyright (C) 2005-2010 Kazuyoshi Aizawa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Test program for iumfs file system module
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>

#define BUF 8192
#define NUM_TARGET 4
#define TEST_FILE "/var/tmp/iumfsmnt/testdir/testfile"
#define TEST_DIR  "/var/tmp/iumfsmnt/testdir"
#define TEST_TEXT "testtext"

void getattr_test();
void readdir_test();
void creat_test();
void read_test();
void write_test();
void mkdir_test();
void rmdir_test();
void unlink_test();
void seek_test();

int
main(int argc, char *argv[]){

    if(argc != 2){
        goto err;
    }

    if(strcmp( argv[1], "getattr") == 0 ){
        getattr_test();
        exit(0);
    } else if(strcmp( argv[1], "readdir") == 0){
        readdir_test();
        exit(0);        
    } else if(strcmp( argv[1], "creat") == 0){
        creat_test();
        exit(0);        
    } else if(strcmp( argv[1], "read") == 0){
        read_test();
        exit(0);        
    } else if(strcmp( argv[1], "write") == 0){
        write_test();
        exit(0);        
    } else if(strcmp( argv[1], "mkdir") == 0){
        mkdir_test();
        exit(0);        
    } else if(strcmp( argv[1], "rmdir") == 0){
        rmdir_test();
        exit(0);        
    } else if(strcmp( argv[1], "unlink") == 0){
        unlink_test();
        exit(0);        
    } else if(strcmp( argv[1], "seek") == 0){
        seek_test();
        exit(0);        
    }
  err:
    printf("Usage: %s [getattr|readdir|open|read|write|mkdir|rmdir|rm|seek]\n", argv[0]);
    exit(1);
}

void creat_test(){
    int fd = 0;

    fd = creat(TEST_FILE, 0666);
    if ( fd < 0){
        printf("creat_test: creat(%s): %s\n", TEST_FILE, strerror(errno));                
        exit(1);
    }
    printf("creat_test: success\n");    
}

void getattr_test(){
    struct stat st[1];

    if ((stat(TEST_DIR, st)) < 0){
        printf("getattr_test: stat(%s): %s\n", TEST_DIR, strerror(errno));        
        exit(1);
    }

    if(S_ISDIR(st->st_mode) == 0 ){
       printf("getattr_test: %s is not recognized as a directory\n", TEST_DIR);
       exit(1);
   }

   printf("getattr_test: success\n");
}

void readdir_test(){
    DIR *dirp = NULL; 
    struct dirent *direntp = NULL;
    int dot_found = 0;
    int dotdot_found = 0;
    int testfile_found = 0;

    if( (dirp = opendir(TEST_DIR)) == NULL){
        printf("readdir_test: opendir(%s): %s\n", TEST_DIR, strerror(errno));
        exit(1);
    }

    printf("readdir_test: found entries: ");
    
    do {
        if ((direntp = readdir(dirp)) != NULL) {
            printf("[\"%s\"] ", direntp->d_name);
            if(strcmp(direntp->d_name, ".") == 0){
                dot_found = 1;
            }else if(strcmp(direntp->d_name, "..") == 0){
                dotdot_found = 1;
            }else if(strcmp(direntp->d_name, "testfile") == 0){
                testfile_found = 1;
            }
        }
    } while (direntp != NULL);
    printf("\n");

    if( !dot_found || !dotdot_found || !testfile_found ){
        printf("readdir_test: can not find  ");
        if(!dot_found )
            printf(" [\".\"] ");
        if(!dotdot_found )
            printf(" [\"..\"] ");
        if(!testfile_found )
            printf(" [\"%s\"]", TEST_FILE);
        printf("\n");
        printf("readdir_test: failed\n");
        exit(1);
    }
    
    printf("getattr_test success\n");    
}
    
void read_test(){
    int fd = 0, i;
    ssize_t cnt = 0;
    char buf[BUF];
    struct stat st[1];

    
    fd = open(TEST_FILE, O_RDONLY);
    if ( fd < 0){
        printf("read_test: open(%s): %s\n", TEST_FILE, strerror(errno));                
        exit(1);
    }

    if ((fstat(fd, st)) < 0){
        printf("read_test: fstat(%s): %s\n", TEST_FILE, strerror(errno));                            
        exit(1);
    }
    
    
    if((cnt = pread(fd, buf, BUF, 0)) < 0){
        printf("read_tesst: pread(%s): %s\n", TEST_FILE, strerror(errno));
    }
    
    if(cnt != sizeof(TEST_TEXT)) {
        printf("read_test: read %zd bytes of data. (!= %zd bytes. Wrong size!!)\n", cnt, sizeof(TEST_TEXT));
	for(i = 0 ; i < cnt ; i++){ 
            printf("buf[%d]= 0x%0x \n", i, buf[i]);            
	}        
        exit(1);
    }

    if(strncmp(buf, TEST_TEXT, strlen(TEST_TEXT)) != 0){
        printf("read_test: read wrong test\n");
	for(i = 0 ; i < cnt ; i++){ 
            printf("buf[%d]= 0x%0x \n", i, buf[i]);
	}
        printf("read_test: %s\n", buf);
        exit(1);
    }
    printf("read_test: success.\n");    
}

void write_test(){
    int fd;
    ssize_t cnt = 0;
    struct stat st[1];

    fd = open(TEST_FILE, O_RDWR);
    if ( fd < 0){
        printf("write_test: open(%s): %s\n", TEST_FILE, strerror(errno));                
        exit(1);
    }

    if ((fstat(fd, st)) < 0){
        printf("write_test: fstat(%s): %s\n", TEST_FILE, strerror(errno));                            
        exit(1);
    }
    
    if((cnt = write(fd, TEST_TEXT, sizeof(TEST_TEXT))) < 0){
        printf("write_test: write(%s): %s\n", TEST_FILE, strerror(errno));
	exit(1);
    }
    
    printf("write_test: success.\n");    
}

void mkdir_test(){
    if (mkdir(TEST_DIR, 0777) < 0){
        printf("mkdir_test: mkdir(%s):  %s", TEST_DIR,  strerror(errno));
        exit(1);
    }
    printf("mkdir_test: success.\n");    
}

void rmdir_test(){
    if (rmdir(TEST_DIR) < 0){
        printf("rmdir_test: rmdir(%s): %s", TEST_DIR, strerror(errno));
        exit(1);
    }
    printf("rmdir_test: success.\n");    
}
void unlink_test(){
    if (unlink(TEST_FILE) < 0){
        printf("unlink_test: rmdir(%s): %s", TEST_DIR, strerror(errno));
        exit(1);
    }
    printf("unlink_test: success.\n");    
}
void seek_test(){
    int fd = 0, i;
    ssize_t cnt = 0;
    char buf[BUF];
    struct stat st[1];

    
    fd = open(TEST_FILE, O_RDONLY);
    if ( fd < 0){
        printf("seek_test: open(%s): %s\n", TEST_FILE, strerror(errno));                
        exit(1);
    }

    if ((fstat(fd, st)) < 0){
        printf("seek_test: fstat(%s): %s\n", TEST_FILE, strerror(errno));                            
        exit(1);
    }
    
    
    if((cnt = lseek(fd, 6, SEEK_SET)) < 0){
        printf("seek_test: lseek(%s): %s\n", TEST_FILE, strerror(errno));
    }
    
    if(cnt != 6) {
        printf("seek_test: lseek returns with %zd . (!= 6)\n", cnt);
        exit(1);
    }    

    if((cnt = read(fd, buf, 1)) < 0){
        printf("seek_test: read(%s): %s\n", TEST_FILE, strerror(errno));
    }    
    
    if(cnt != 1) {
        printf("seek_test: read %zd bytes of data. (!= 1 bytes)\n", cnt);
	for(i = 0 ; i < cnt ; i++){ 
            printf("buf[%d]= 0x%0x \n", i, buf[i]);            
	}        
        exit(1);
    }

    if(strncmp(buf, "x", 1) != 0){
        printf("seek_test: read wrong test\n");
	for(i = 0 ; i < cnt ; i++){ 
            printf("buf[%d]= 0x%0x \n", i, buf[i]);
	}
        printf("seek_test: %s\n", buf);
        exit(1);
    }
    printf("seek_test: success.\n");    
}
