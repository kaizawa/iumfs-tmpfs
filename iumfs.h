/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 1986, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Copright (c) 2005-2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 * All rights reserved.
 */
/************************************************************
 * iumfs.h
 * 
 * iumfs 用ヘッダーファイル
 *
 *************************************************************/

#ifndef __IUMFS_H
#define __IUMFS_H

#define SUCCESS         0
#define TRUE            1
#define FALSE           0
#define MAX_MSG         256     // SYSLOG に出力するメッセージの最大文字数 
#define MAXNAMLEN       255     // 最大ファイル名長
#define BLOCKSIZE       512     // iumfs ファイルシステムのブロックサイズ

#ifdef DEBUG
#define  DEBUG_PRINT(args)  debug_print args
#else
#define DEBUG_PRINT(args)
#endif

#define VFS2IUMFS(vfsp)     ((iumfs_t *)(vfsp)->vfs_data)
#define VFS2ROOT(vfsp)      ((VFS2IUMFS(vfsp))->rootvnode)
#define IUMFS2ROOT(iumfsp)  ((iumfsp)->rootvnode)
#define VNODE2VFS(vp)       ((vp)->v_vfsp)
#define VNODE2IUMNODE(vp)   ((iumnode_t *)(vp)->v_data)
#define IUMNODE2VNODE(inp)  ((inp)->vnode)
#define VNODE2IUMFS(vp)     (VFS2IUMFS(VNODE2VFS((vp))))
#define VNODE2ROOT(vp)      (VNODE2IUMFS((vp))->rootvnode)
#define IN_INIT(inp) {\
              mutex_init(&(inp)->i_lock, NULL, MUTEX_DEFAULT, NULL);\
              inp->vattr.va_uid      = 0;\
              inp->vattr.va_gid      = 0;\
              inp->vattr.va_blksize  = BLOCKSIZE;\
              inp->vattr.va_nlink    = 1;\
              inp->vattr.va_rdev     = 0;\
              inp->vattr.va_vcode    = 1;\
              inp->vattr.va_mode     = 00644;\
         }

#ifdef SOL10
// Solaris 10 には VN_INIT マクロが無いので追加。
#define	VN_INIT(vp, vfsp, type, dev)	{ \
	mutex_init(&(vp)->v_lock, NULL, MUTEX_DEFAULT, NULL); \
	(vp)->v_flag = 0; \
	(vp)->v_count = 1; \
	(vp)->v_vfsp = (vfsp); \
	(vp)->v_type = (type); \
	(vp)->v_rdev = (dev); \
	(vp)->v_stream = NULL; \
}
#else
#endif

// 引数で渡された数値をポインタ長の倍数に繰り上げるマクロ
#define ROUNDUP(num)        ((num)%(sizeof(char *)) ? ((num) + ((sizeof(char *))-((num)%(sizeof(char *))))) : (num))

/*
 * ファイルシステム型依存のノード情報構造体。（iノード）
 * vnode 毎（open/create 毎）に作成される。
 * next, vattr, data, dlen については初期化以降も変更される
 * 可能性があるため、参照時にはロック(i_lock)をとらなければ
 * ならない。
 */
typedef struct iumnode
{
    struct iumnode    *next;      // iumnode 構造体のリンクリストの次の iumnode 構造体
    kmutex_t           i_lock;    // 構造体のデータの保護用のロック    
    vnode_t           *vnode;     // 対応する vnode 構造体へのポインタ
    vattr_t            vattr;     // getattr, setattr で使われる vnode の属性情報
    void              *data;      // vnode がディレクトリの場合、ディレクトリエントリへのポインタが入る
    offset_t           dlen;      // ディレクトリエントリのサイズ
} iumnode_t;

/*
 * ファイルシステム型依存のファイルシステムプライベートデータ構造体。
 * ファイルシステム毎（mount 毎）に作成される。
 */
typedef struct iumfs
{
    kmutex_t      iumfs_lock;        // 構造体のデータの保護用のロック
    vnode_t      *rootvnode;         // ファイルシステムのルートの vnode
    ino_t         iumfs_last_nodeid; // 割り当てた最後のノード番号    
    iumnode_t     node_list_head;    // 作成された iumnode 構造体のリンクリストのヘッド。
                                     // 構造体の中身は、ロック以外は参照されない。また、
                                     // ファイルシステムが存在する限りフリーされることもない。
} iumfs_t;

extern timestruc_t time; // システムの現在時

/* 関数のプロトタイプ宣言 */
void          debug_print(int , char *, ...);
int           iumfs_alloc_node(vfs_t *, vnode_t **, uint_t , enum vtype);
void          iumfs_free_node(vnode_t *);
int           iumfs_add_node_to_list(vfs_t *, vnode_t *);
int           iumfs_remove_node_from_list(vfs_t *, vnode_t *);
void          iumfs_free_all_node(vfs_t *);
int           iumfs_make_directory(vfs_t *, vnode_t **, vnode_t *, struct cred *);
int           iumfs_add_entry_to_dir(vnode_t *, char *, int, ino_t );
int           iumfs_remove_entry_from_dir(vnode_t *, char *);
ino_t         iumfs_find_nodeid_by_name(iumnode_t *, char *);
int           iumfs_dir_is_empty(vnode_t *);
vnode_t      *iumfs_find_vnode_by_nodeid(iumfs_t *, ino_t);
timestruc_t   iumfs_get_current_time();

/* Solaris 10 以外の場合の gcc 対策用ラッパー関数 */
#ifdef SOL10
#else
static void  *memcpy(void *,  const  void  *, size_t );
static int    memcmp(const void *, const void *, size_t );
#endif
    
#ifndef SOL10
/**************************************************
 * memcpy()
 *
 * gcc 対策。bcopy(9f) のラッパー
 * Solaris 10 では memcpy(9f) が追加されているので不要
 **************************************************/
static void *
memcpy(void *s1,  const  void  *s2, size_t n)
{
    bcopy(s2, s1, n);
    return(s1);
}
/**************************************************
 * memcmp()
 *
 * gcc 対策。bcmp(9f) のラッパー
 * Solaris 10 では memcmp(9f) が追加されているので不要
 **************************************************/
static int
memcmp(const void *s1, const void *s2, size_t n)
{
    return(bcmp(s1, s1, n));
}
#endif

#endif // #ifndef __IUMFS_H
