#!/bin/sh

# Copyright (C) 2010 Kazuyoshi Aizawa. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Test script of iumfs filesystem
# After build iumfs, run this script as root user.
#

PATH=/usr/bin:/usr/sbin:/usr/ccs/bin:/usr/local/bin:.

daemonpid=""
# Change mount point for this test, if it exists 
mnt="/var/tmp/iumfsmnt"
# Change uid if you want to run make command as non-root user
uid="root"

init (){

	if [ "$USER" != "root" ]; then
		echo "must be run as root user!"
		exit
	fi

	# Create mount point 'mnt' for test.
        if [ ! -d "${mnt}" ]; then
		mkdir ${mnt}
		if [ "$?" -ne 0 ]; then
			echo "cannot create ${mnt}"
			fini 1	
		fi
	fi

	sudo -u ${uid} ./configure
	make uninstall
	sudo -u ${uid} make clean
	sudo -u ${uid} make
	make install

	# Just in case, umount mnt 
	exec_umount_all
}

## Foce umount mnt 
exec_umount_all() {
	failcount=0
	while : 
	do
		mountexit=`mount |grep "${mnt} "`
		if [ -z "$mountexit" ]; then
			break
		fi
		umount ${mnt} > /dev/null 2>&1
		failcount=`expr $failcount + 1`
		if [ $failcount -gt 10 ]; then
			echo "too many umount fails(=10 times)"
			fini 1
		fi
	done
}

exec_mount () {
 	mount -F iumfs hoge ${mnt} 
	return $?
}

exec_umount() {
	umount ${mnt} > /dev/null 2>&1 
	return $?
}

run_test () {
	target=${1}
	cmd="exec_${target}"
	$cmd
	if [ "$?" -eq "0" ]; then
		echo "${target} test: pass"
	else
		echo "${target} test: fail"
	        exec_umount
	fi
}

exec_getattr() {
	invoke_fstest "getattr" 
	return $?
}

exec_creat() {
	invoke_fstest "creat" 
	return $?
}

exec_read() {
	invoke_fstest "read" 
	return $?
}

exec_write() {
	invoke_fstest "write" 
	return $?
}

exec_mkdir() {
	invoke_fstest "mkdir" 
	return $?
}


exec_unlink() {
	invoke_fstest "unlink" 
	return $?
}

exec_rmdir() {
	invoke_fstest "rmdir" 
	return $?
}

exec_seek(){
        invoke_fstest "seek"
        return $?
}

invoke_fstest(){
	./fstest $1
	if [ "$?" -ne "0" ]; then
	    return 1
	fi
	return 0
}

fini() {
	exec_umount_all
	rm -r ${mnt}
}

init
run_test "mount"
run_test "umount"
run_test "mount"
run_test "mkdir"
run_test "getattr"
run_test "creat"
run_test "write"
run_test "read"
run_test "seek"
run_test "unlink"
run_test "rmdir"
run_test "umount"
fini
