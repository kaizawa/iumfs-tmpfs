/*
 * Copyright (C) 2010 Kazuyoshi Aizawa. All rights reserved.
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
/*****************************************************************
 * iumfs_mount.c
 *
 *  gcc iumfs_mount.c -o mount
 *
 * iumfs の為の mount コマンド。
 * /usr/lib/fs/iumfs/ ディレクトリを作り、このディレクトリ内に
 * このプログラムを「mount」として配置すれば、/usr/sbin/mount
 * にファイルシステムタイプとして「iumfs」を指定すると、この
 * プログラムが呼ばれることになる。
 *
 *  usage: # /usr/sbin/mount -F iumfs strings mount_point
 *     strings:
 *     mount_point:
 *
 ******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/errno.h>

int
main(int argc, char *argv[])
{
    if (argc != 3){
        printf("Usage: %s -F iumfs strings mount_point\n", argv[0]);
        exit(0);
    }
    
    if ( mount(argv[1], argv[2], MS_DATA, "iumfs", NULL, 0) < 0 ){        
	perror("mount");
        exit(0);
    }
    return(0);    
}
