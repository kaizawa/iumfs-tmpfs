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
/**************************************************************
 * iumfs
 * 
 * 擬似ファイルシステム IUMFS
 *
 *  更新履歴:  
 *    2006/03/03 Solaris 9 で mount コマンド実行時に panic する問題を修正
 *    
 **************************************************************/
#include <sys/modctl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/kmem.h>
#include <sys/stropts.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/ksynch.h>
#include <sys/pathname.h>
#include <sys/file.h>
#include <sys/systm.h>
#include <sys/varargs.h>
#include <vm/pvn.h>
#include <stddef.h>
// OpenSolaris の場合必要
#ifdef OPENSOLARIS
#include <sys/vfs_opreg.h>
#endif
#include "iumfs.h"

/*
 *  現在使用しているデータ型
 *
 *    ファイルのオフセット値 : offset_t
 *    ノード番号             : ino_t
 *    vattr 構造体           : vattr_t
 *
 *  このポリシーが正しいかどうかが不明。
 *  再検討の余地有り！
 */

#include "iumfs.h"

static mntopts_t        iumfs_optproto;
static dev_t            iumfs_dev; // デバイス番号
static int              iumfs_fstype; // ファイルシステムタイプ

#ifdef SOL10
extern struct vnodeops *iumfs_vnodeops;
extern fs_operation_def_t iumfs_vnode_ops_def_array[];
#else
extern struct vnodeops iumfs_vnodeops;
#endif

/*
 *VFSOPS プロトタイプ宣言
 */
#ifdef SOL10
static int  iumfs_init(int , char *);
#else 
static int  iumfs_init(struct vfssw *, int);
static int  iumfs_reserved(vfs_t *, vnode_t **, char *);
#endif
static int  iumfs_mount(vfs_t *, vnode_t *, struct mounta *, struct cred *);
static int  iumfs_unmount(vfs_t *, int, struct cred *);
static int  iumfs_root(vfs_t *, vnode_t **);
static int  iumfs_statvfs(vfs_t *, struct statvfs64 *);
static int  iumfs_sync(vfs_t *, short , struct cred *);
static int  iumfs_vget(vfs_t *, vnode_t **, struct fid *);
static int  iumfs_mountroot(vfs_t *, enum whymountroot);
static void iumfs_freevfs(vfs_t *);

/* 関数のプロトタイプ宣言 */
static int  iumfs_create_fs_root(vfs_t *, vnode_t **, vnode_t *, struct cred *);

/*
 * このファイルシステムでサーポートする VFS オペレーション
 */
#ifdef SOL10
/*
 * Solaris 10 の場合、vfsops 構造体は vn_make_ops() にて得る。
 * OpenSolaris の場合さらに fs_operation_def_t の func メンバが union に代わっている
 */
static struct vfsops  *iumfs_vfsops;
#ifdef OPENSOLARIS
fs_operation_def_t iumfs_vfs_ops_def_array[] = {
    { VFSNAME_MOUNT,    {&iumfs_mount}}, // mount(2) システムコールに対応
    { VFSNAME_UNMOUNT,  {&iumfs_unmount}}, // umount(2)システムコールに対応
    { VFSNAME_ROOT,     {&iumfs_root}}, // ファイルシステムの root direcotry を返す
    { VFSNAME_SYNC,     {(fs_generic_func_p)&iumfs_sync}}, // 変更されたページをディスクに書き込む
    { VFSNAME_STATVFS,  {&iumfs_statvfs}},  //ファイルシステムの統計情報を返す
    { VFSNAME_VGET,     {&iumfs_vget}},      // 指定されたファイル ID から vnode を返す
    { VFSNAME_MOUNTROOT,{&iumfs_mountroot}}, // システムのルートにファイルシステムをマウントする。
    { VFSNAME_FREEVFS,  {(fs_generic_func_p)&iumfs_freevfs}},   // -- 用途不明 --
    { NULL, {NULL}}
};
#else
fs_operation_def_t iumfs_vfs_ops_def_array[] = {
    { VFSNAME_MOUNT,    &iumfs_mount}, // mount(2) システムコールに対応
    { VFSNAME_UNMOUNT,  &iumfs_unmount}, // umount(2)システムコールに対応
    { VFSNAME_ROOT,     &iumfs_root}, // ファイルシステムの root direcotry を返す
    { VFSNAME_SYNC,     (fs_generic_func_p)&iumfs_sync}, // 変更されたページをディスクに書き込む
    { VFSNAME_STATVFS,  &iumfs_statvfs},  //ファイルシステムの統計情報を返す
    { VFSNAME_VGET,     &iumfs_vget},      // 指定されたファイル ID から vnode を返す
    { VFSNAME_MOUNTROOT,&iumfs_mountroot}, // システムのルートにファイルシステムをマウントする。
    { VFSNAME_FREEVFS,  (fs_generic_func_p)&iumfs_freevfs},   // -- 用途不明 --
    { NULL, NULL}
};
#endif //ifdef OPENSOLARIS
#else
/*
 * Solaris 9 の場合、vfsops 構造体はファイルシステムが確保し、直接参照できる。
 */
static struct vfsops  iumfs_vfsops = {
    &iumfs_mount,       // vfs_mount     mount(2) システムコールに対応
    &iumfs_unmount,     // vfs_unmount   umount(2)システムコールに対応 
    &iumfs_root,        // vfs_root      ファイルシステムの root direcotry を返す 
    &iumfs_statvfs,     // vfs_statvfs   ファイルシステムの統計情報を返す
    &iumfs_sync,        // vfs_sync      変更されたページをディスクに書き込む
    &iumfs_vget,        // vfs_vget      指定されたファイル ID から vnode を返す
    &iumfs_mountroot,   // vfs_mountroot システムのルートにファイルシステムをマウントする。
    &iumfs_reserved,    // vfs_reserved  -- 用途不明 --
    &iumfs_freevfs      // vfs_freevs.   -- 用途不明 --
};
#endif

#ifdef SOL10
// Solaris 10 の場合、vfssw 構造体は直接作成しない。
static vfsdef_t iumfs_vfsdef = {
    VFSDEF_VERSION, // int  structure version, must be first
    "iumfs",	    // char  filesystem type name 
    &iumfs_init,    // int init routine 
    0,		    // int filesystem flags 
    &iumfs_optproto // mntopts_tmount options table prototype 
};

#else
/*
 * ファイルシステムタイプスイッチテーブル
 *  /usr/include/sys/vfs.h
 */
static struct vfssw iumfs_vfssw = { 
    "iumfs", 	          // vsw_name   ファイルシステムタイプを示す名前
    &iumfs_init,          // vsw_init   初期化ルーチン 
    &iumfs_vfsops,        // vsw_vfsops このファイルシステムの操作ベクタ
    0,                    // vsw_flag   フラグ 
    &iumfs_optproto       // vsw_optproto マウントオプションテーブルのプロトタイプ 
};
#endif

/* 
 * ファイルシステムの linkage 構造体
 * ファイルシステム固有の情報を知らせるために使う。
 * vfssw へのポインターが入っている
 */
static struct modlfs iumfs_modlfs  = {
    &mod_fsops,           // fsmodops
    "pseudo file system", // fs_linkinfo
#ifdef SOL10
    &iumfs_vfsdef         // fs_vfsdef
#else    
    &iumfs_vfssw          // fs_vfssw
#endif    
};

/* 
 * kernel module を kernel に load する _init() 内の
 * mod_install によって使われる。
 * modlfs へのポインターが入っている
 */
static struct modlinkage modlinkage = {
    MODREV_1,		     // modules system の revision 固定値
    {
        (void *)&iumfs_modlfs,   // ファイルシステムの linkage 構造体 
        NULL                     // NULL terminate
    }
};

/*************************************************************************
 * _init(9e), _info(9e), _fini(9e)
 * 
 * ローダブルカーネルモジュールのエントリーポイント
 *************************************************************************/
int
_init() 
{ 
    int err;
    
    err = mod_install(&modlinkage);
    return (err);
}

int
_info(struct modinfo *modinfop) 
{
    return mod_info(&modlinkage, modinfop);
}

int
_fini() 
{
    int err;
    
    err = mod_remove(&modlinkage);
    return(err);
}

/*****************************************************************************
 * bebug_print()
 *
 * デバッグ出力用関数
 *
 *  引数：
 *           level  :  エラーの深刻度。cmn_err(9F) の第一引数に相当
 *           format :  メッセージの出力フォーマットcmn_err(9F) の第二引数に相当
 * 戻り値：
 *           なし。
 *****************************************************************************/
void
debug_print(int level, char *format, ...)
{ 
    va_list     ap;
    char        buf[MAX_MSG];

    va_start(ap, format);
    vsprintf(buf, format, ap);    
    va_end(ap);
    cmn_err(level, "%s", buf);
}    

/************************************************************************
 * iumfs_init() 
 *
 * 初期化ルーチン
 *************************************************************************/
static int
#ifdef SOL10
iumfs_init(int fstype, char *fsname)
#else
iumfs_init(struct vfssw *iumfs_vfssw, int fstype)
#endif
{
    int   err;
    
    DEBUG_PRINT((CE_CONT,"iumfs_init called\n"));
    DEBUG_PRINT((CE_CONT,"iumfs_init: fstype = %d(0x%x)\n", fstype, fstype));

    iumfs_fstype = fstype;

#ifdef SOL10
    do {
        err = vfs_setfsops(fstype, iumfs_vfs_ops_def_array, &iumfs_vfsops);
        if(err)
            break;
        err = vn_make_ops(fsname, iumfs_vnode_ops_def_array, &iumfs_vnodeops);
        if(err)
            break;        
    } while(FALSE);

    if(err){
        if(iumfs_vfsops != NULL){
            vfs_freevfsops_by_type(fstype);
        }
        if(iumfs_vnodeops != NULL){
            vn_freevnodeops(iumfs_vnodeops);
        }        
    }
#endif
    // filesystem id に使うデバイス番号を決める。
    // TODO:
    // システムでまだ使われていない Major device 番号をさがさなければならない。
    // 今は 500 を割り当てている・・かなり適当。衝突の可能性がある。
    iumfs_dev = makedevice(500, 0);
    
    return(err);
}

/************************************************************************
 * iumfs_mount()  VFS オペレーション
 *
 * マウントルーチン
 * 
 *  vfsp   : kernel が確保した、これからマウントする新しいファイルシステム
 *           の為の vfs 構造体のポインタ
 *  mvnode : ディレクトリマウントポイントの vnode
 *  mntarg : mount の引数（注: ユーザ空間のデータ）
 *  cr     : ユーザクレデンシャル(UID, GID 等）
 *
 *  戻り値
 *    正常時   : 0
 *    エラー時 : 0 以外
 * 
 *************************************************************************/
static int
iumfs_mount(vfs_t *vfsp, vnode_t *mvnode, struct mounta *mntarg, struct cred *cr)
{
    iumfs_t  *iumfsp  = NULL;
    vnode_t  *rootvp  = NULL;
    int       err     = 0;
    
    DEBUG_PRINT((CE_CONT,"iumfs_mount called\n"));
    
    DEBUG_PRINT((CE_CONT,"iumfs_mount: vfs_count = %d\n", vfsp->vfs_count));
    if(mntarg->flags & MS_SYSSPACE){
        DEBUG_PRINT((CE_CONT,"iumfs_mount: MS_SYSSPACE flag is set\n"));
    } else {
        DEBUG_PRINT((CE_CONT,"iumfs_mount: MS_SYSSPACE flag is not set\n"));
    }
    DEBUG_PRINT((CE_CONT,"iumfs_mount: fsid 0x%x 0x%x \n",vfsp->vfs_fsid.val[0],vfsp->vfs_fsid.val[1]));

    /*
     * iumfs_init() 内で決めたデバイス番号（メジャー＋マイナーデバイス番号）
     * をファイルシステム ID として使う。
     */
    vfsp->vfs_fsid.val[0] = (int)getmajor(iumfs_dev);
    vfsp->vfs_fsid.val[1] = (int)getminor(iumfs_dev);

    DEBUG_PRINT((CE_CONT,"iumfs_mount: fsid 0x%x 0x%x \n",
                 vfsp->vfs_fsid.val[0], vfsp->vfs_fsid.val[1]));    

    // 途中で break するためだけの do-while 文
    do {
        /*
         * ファイルシステムのプライベートデータ構造体を確保
         */
        iumfsp = (iumfs_t *)kmem_zalloc(sizeof(iumfs_t), KM_NOSLEEP);
        if(iumfsp == NULL){
            cmn_err(CE_CONT, "iumfs_mount: kmem_zalloc failed");
            err = ENOMEM;
            break;
        }
        /*
         * ロックを初期化
         */
        mutex_init(&(iumfsp->iumfs_lock), NULL, MUTEX_DEFAULT, NULL);
        mutex_init(&(iumfsp->node_list_head.i_lock), NULL, MUTEX_DEFAULT, NULL);        

        /*
         * vfs 構造体にファイルシステムのプライベートデータ構造体をセット
         */
        vfsp->vfs_data = (char *)iumfsp;

        /*
         * ファイルシステムのルートディレクトリを作成
         */
        if((err = iumfs_create_fs_root(vfsp, &rootvp, mvnode, cr)) != SUCCESS)
            break;

	vfsp->vfs_fstype = iumfs_fstype;
	vfsp->vfs_bsize = 0;
        
    } while(FALSE);

    /*
     * エラーが発生した場合には確保したリソースを解放し、エラーを返す
     */
    if(err){
        if (iumfsp != NULL){
            if(rootvp != NULL){
                iumfs_free_all_node(vfsp);
            }
            // ロックを削除し、確保したメモリを開放
            mutex_destroy(&(iumfsp->iumfs_lock));
            mutex_destroy(&(iumfsp->node_list_head.i_lock));
            kmem_free(iumfsp, sizeof(iumfs_t));
            vfsp->vfs_data = (char *)NULL;
        }
    }
    return(err);
}

/************************************************************************
 * iumfs_unmount()  VFS オペレーション
 *
 * アンマウントルーチン
 *************************************************************************/
static int
iumfs_unmount(vfs_t *vfsp, int val, struct cred *cred)
{
    iumfs_t    *iumfsp;
    vnode_t    *rootvp, *vp;
    iumnode_t  *inp, *previnp;
    
    DEBUG_PRINT((CE_CONT,"iumfs_unmount called\n"));

    iumfsp = VFS2IUMFS(vfsp);
    rootvp = VFS2ROOT(vfsp);

    /*
     * iumnode のリンクリストを廻り、利用されている(v_count != 1) vnode
     * があれば EBUSY を返す。この時点ではまだフリーしない。
     * （一部をフリーしてしまった後で、利用中の vnode があることが分かって
     * しまうのを避けるための動作）
     */
    previnp = &iumfsp->node_list_head;
    mutex_enter(&(previnp->i_lock));
    while(previnp->next){
        inp = previnp->next;
        mutex_enter(&(inp->i_lock));
        vp = IUMNODE2VNODE(inp);
        if(vp->v_count != 1){
            /*
             * まだ利用されている vnode がある、ロックを開放し、EBUSY を返す。
             */
            DEBUG_PRINT((CE_CONT,"iumfs_unmount: vp->v_count = %d\n",vp->v_count ));
            mutex_exit(&(inp->i_lock));
            mutex_exit(&(previnp->i_lock));
            return(EBUSY);
        }
        mutex_exit(&(previnp->i_lock));
        previnp = inp;
    }
    mutex_exit(&(previnp->i_lock));
    
    /*
     * 全ての vnode が利用されていないのが分かった。
     * このまま全ての vnode を開放する。
     */
    iumfs_free_all_node(vfsp);
    
    // ファイルシステム型依存のファイルシステムデータ（iumfs 構造体）を解放
    mutex_destroy(&(iumfsp->iumfs_lock));    
    kmem_free(iumfsp, sizeof(iumfs_t));    
    return(SUCCESS);    
}

/************************************************************************
 * iumfs_root()  VFS オペレーション
 *
 * ファイルシステムのルートの vnode を返す。
 *************************************************************************/
static int
iumfs_root(vfs_t *vfsp, vnode_t **vpp)
{
    iumfs_t   *iumfsp;
    int        err = SUCCESS;
    vnode_t   *rootvp;
    
    DEBUG_PRINT((CE_CONT,"iumfs_root called\n"));

    if((iumfsp = (iumfs_t *)vfsp->vfs_data) == NULL){
        cmn_err(CE_CONT, "iumfs_root: vfsp->vfs_data is NULL\n");
        return(EINVAL);
    }

    mutex_enter(&(iumfsp->iumfs_lock));
    rootvp = iumfsp->rootvnode;
    
    // 途中で break するためだけの do-while 文
    do{
        if(rootvp == NULL){
            /*
             * ファイルシステムのルートディレクトリの vnode が無い。
             * ルートディレクトリはマウント時に作成されているはず。
             */
            cmn_err(CE_CONT, "iumfs_root: root vnode doesn't exit!!\n");
            err = ENOENT;
            break;
        }

        DEBUG_PRINT((CE_CONT,"iumfs_root: rootvp->v_count = %d\n",rootvp->v_count ));

        // vnode を返すので、参照カウントをあげる
        VN_HOLD(rootvp);
        
        // 引数として渡された vnode のポインタに新しいファイルシステムのルートの vnode をセット
        *vpp = rootvp;
    
    } while(0);
    
    mutex_exit(&(iumfsp->iumfs_lock));
    
    return(err);
}

/************************************************************************
 * iumfs_statvfs()  VFS オペレーション
 *
 * 
 *************************************************************************/
static int
iumfs_statvfs(vfs_t *vfsp, struct statvfs64 *statvfs)
{
    DEBUG_PRINT((CE_CONT,"iumfs_statvfs called\n"));            

    return(ENOTSUP);    
}

/************************************************************************
 * iumfs_sync()  VFS オペレーション
 *
 *  常に 0 （成功）を返す
 *************************************************************************/
static int
iumfs_sync(vfs_t *vfsp, short val, struct cred *cred)
{
//    DEBUG_PRINT((CE_CONT,"iumfs_sync is called\n"));                

    return(SUCCESS);    
}

/************************************************************************
 * iumfs_vget()  VFS オペレーション
 *
 * 
 *************************************************************************/
static int
iumfs_vget(vfs_t *vfsp, vnode_t **vnode, struct fid *fid)
{
    DEBUG_PRINT((CE_CONT,"iumfs_vget called\n"));                    

    return(ENOTSUP);    
}

/************************************************************************
 * iumfs_mountroot()  VFS オペレーション
 *
 * 
 *************************************************************************/
static int
iumfs_mountroot(vfs_t *vfsp, enum whymountroot whymountroot)
{
    DEBUG_PRINT((CE_CONT,"iumfs_mountroot called\n"));                        

    return(ENOTSUP);    
}

#ifndef SOL10
/************************************************************************
 * iumfs_reserved()  VFS オペレーション
 *
 * 
 *************************************************************************/
static int
iumfs_reserved(vfs_t *vfsp, vnode_t **vnode, char *strings)
{
    DEBUG_PRINT((CE_CONT,"iumfs_reserved called\n"));                            

    return(ENOTSUP);    
}
#endif

/************************************************************************
 * iumfs_freevfs()  VFS オペレーション
 *
 * 
 *************************************************************************/
static void
iumfs_freevfs(vfs_t *vfsp)
{
    DEBUG_PRINT((CE_CONT,"iumfs_freevfs called\n"));
    return;
}

/************************************************************************
 * iumfs_alloc_node()
 *
 *   新しい vnode 及び iumnode を確保する。
 *
 * 引数:
 *     vfsp : vfs 構造体
 *     vpp  : 呼び出し側から渡された vnode 構造体のポインタのアドレス
 *     flag : 作成する vnode のフラグ(VROOT, VISSWAP 等)
 *     type : 作成する vnode のタイプ(VDIR, VREG 等)
 *
 * 戻り値
 *    正常時   : SUCCESS(=0)
 *    エラー時 : 0 以外
 *
 ************************************************************************/
int
iumfs_alloc_node(vfs_t *vfsp, vnode_t **nvpp, uint_t flag, enum vtype type)
{
    vnode_t    *vp;
    iumnode_t  *inp;
    iumfs_t    *iumfsp;

    DEBUG_PRINT((CE_CONT,"iumfs_alloc_node is called\n"));

    iumfsp = VFS2IUMFS(vfsp);

    // vnode 構造体を確保
#ifdef SOL10
    // Solaris 10 では直接 vnode 構造体を alloc してはいけない。
    vp = vn_alloc(KM_NOSLEEP);
#else
    // Solaris 9 ではファイルシステム自身で vnode 構造体を alloc する。
    vp = (vnode_t *)kmem_zalloc(sizeof(vnode_t), KM_NOSLEEP);
#endif    
    
    //ファイルシステム型依存のノード情報(iumnode 構造体)を確保
    inp = (iumnode_t *)kmem_zalloc(sizeof(iumnode_t), KM_NOSLEEP);
    
    /*
     * どちらかでも確保できなかったら ENOMEM を返す
     */
    if(vp == NULL || inp == NULL ){
        cmn_err(CE_CONT, "iumfs_alloc_node: kmem_zalloc failed\n");
        if(vp != NULL)
#ifdef SOL10
            vn_free(vp);
#else        
            kmem_free(vp, sizeof(vnode_t));
#endif            
        if(inp != NULL)
            kmem_free(inp, sizeof(iumnode_t));                          
        return(ENOMEM);
    }

    /*
     * 確保した vnode を初期化
     * VN_INIT マクロの中で、v_count の初期値を 1 にセットする。
     * これによって、ファイルシステムの意図しないタイミングで iumfs_inactive()
     * が呼ばれてしまうのを防ぐ。
     */
    VN_INIT(vp, vfsp, type, 0);
    
    // ファイルシステム型依存の vnode 操作構造体のアドレスをセット
#ifdef SOL10
    vn_setops(vp, iumfs_vnodeops);
#else        
    vp->v_op = &iumfs_vnodeops;
#endif
    
    // v_flag にフラグをセット
    vp->v_flag &= flag;

    /*
     * 確保した iumnode を初期化 (IN_INIT マクロは使わない）
     */
    mutex_init(&(inp)->i_lock, NULL, MUTEX_DEFAULT, NULL);
    inp->vattr.va_mask     = AT_ALL;
    inp->vattr.va_uid      = 0;
    inp->vattr.va_gid      = 0;
    inp->vattr.va_blksize  = BLOCKSIZE;
    inp->vattr.va_nlink    = 1;
    inp->vattr.va_rdev     = 0;
#ifdef SOL10
#else    
    inp->vattr.va_vcode    = 1;
#endif
    /*
     * vattr の va_fsid は dev_t(=ulong_t), これに対して vfs の
     * vfs_fsid は int 型の配列(int[2])を含む構造体。
     *
     *          | 32bit Kernel | 64bit Kernel
     * ---------+--------------+------------
     * va_fsid  |     32 bit   |      64 bit
     * vfs_fsid |     64 bit   |      64 bit
     *
     * どうすればいいのだろう？ とりあえず、*vfs_fsid.val
     * (仮の major device 番号)を va_fsid としておく。
     */
//    inp->vattr.va_fsid  = vfsp->vfs_fsid.val[0];
    inp->vattr.va_fsid  = (ulong_t)*(vfsp->vfs_fsid.val);
    inp->vattr.va_type  = type;
    inp->vattr.va_atime = \
    inp->vattr.va_ctime = \
    inp->vattr.va_mtime = iumfs_get_current_time();
    
    DEBUG_PRINT((CE_CONT,"iumfs_alloc_node: va_fsid = %d\n", inp->vattr.va_fsid));

    /*
     * vnode に iumnode 構造体へのポインタをセット
     * 逆に、iumnode にも vnode 構造体へのポインタをセット
     */
    vp->v_data = (caddr_t)inp;
    inp->vnode = vp;

    /*
     * ノード番号（iノード番号）をセット。単純に１づつ増やしていく。
     */
    mutex_enter(&(iumfsp->iumfs_lock));
    inp->vattr.va_nodeid = ++(iumfsp->iumfs_last_nodeid);
    mutex_exit(&(iumfsp->iumfs_lock));    

    DEBUG_PRINT((CE_CONT,"iumfs_alloc_node: new nodeid = %d \n", inp->vattr.va_nodeid));

    //新しい iumnode をノードのリンクリストに新規のノードを追加
    iumfs_add_node_to_list(vfsp, vp);

    // 渡された vnode 構造体のポインタに確保した vnode のアドレスをセット
    *nvpp = vp;
    
    return(SUCCESS);
}

/************************************************************************
 * iumfs_free_node()
 *
 *   指定された vnode および iumnode を解放する
 *
 *     1. iumnode に関連づいたリソースを解放
 *     2. iumnode 構造体を解放
 *     3. vnode 構造体を解放
 *
 *   これが呼ばれるのは、iumfs_inactive() もしくは iumfs_unmount() 経由の
 *   iumfs_free_all_node() だけ。つまり、v_count が 1（未参照状態）である
 *   事が確かな場合だけ。
 *
 * 引数:
 *
 *     vp: 解放する vnode 構造体のポインタ
 *
 * 戻り値:
 *     無し
 *
 ************************************************************************/
void
iumfs_free_node(vnode_t *vp)
{
    iumnode_t  *inp;     // ファイルシステム型依存のノード情報(iumnode構造体)
    vnode_t    *rootvp;  // ファイルシステムのルートディレクトリの vnode。
    iumfs_t    *iumfsp;  // ファイルシステム型依存のプライベートデータ構造体
    vfs_t      *vfsp;    // ファイルシステム構造体
    int         err;
    
    DEBUG_PRINT((CE_CONT,"iumfs_free_node is called\n"));    

    iumfsp = VNODE2IUMFS(vp);
    vfsp   = VNODE2VFS(vp);
    inp    = VNODE2IUMNODE(vp);

    DEBUG_PRINT((CE_CONT,"iumfs_free_node: vp->v_count = %d\n", vp->v_count));
    
    //
    // 最初にノードリンクリストから iumnode をはずす。
    // 仮に、ノードリストに入っていなかったとしても、（ありえないはずだが）
    // vnode のフリーは行う。
    //
    if((err = iumfs_remove_node_from_list(vfsp, vp)) != 0){
        DEBUG_PRINT((CE_CONT,"iumfs_free_node: can't find vnode in the list. Free it anyway.\n"));
    }

    // debug 用
    rootvp = VNODE2ROOT(vp);    
    if(rootvp != NULL && VN_CMP(rootvp, vp) != 0){
        DEBUG_PRINT((CE_CONT,"iumfs_free_node: rootvnode is being freed\n"));
        mutex_enter(&(iumfsp->iumfs_lock));
        iumfsp->rootvnode = NULL;
        mutex_exit(&(iumfsp->iumfs_lock));        
    }

    // もし iumnode にデータ（ディレクトリエントリ等）
    // を含んでいたらそれらも解放する。
    if(inp->data != NULL){
        kmem_free(inp->data, inp->dlen);
    }
    
    // iumnode を解放
    mutex_destroy(&(inp)->i_lock);    
    kmem_free(inp, sizeof(iumnode_t));

    // vnode を解放
#ifdef SOL10
    vn_free(vp);
#else
    mutex_destroy(&(vp)->v_lock);        
    kmem_free(vp, sizeof(vnode_t));
#endif                

    return;
}

/************************************************************************
 * iumfs_create_fs_root()
 *
 *   ファイルシステムのルートを作る。
 *   ファイルシステムのマウント時に iumfs_mount() から呼ばれる。
 *   
 *    o ルートディレクトリのをさす vnode の作成
 *    o ルートディレクトリのディレクトリエントリを作成
 *
 * 引数:
 *
 *     vfsp  : vfs 構造体
 *     vpp   : 呼び出し側から渡された vnode 構造体のポインタのアドレス
 *     mvp   : ディレクトリマウントポイントの vnode 構造体のポインタ
 *
 ************************************************************************/
int
iumfs_create_fs_root(vfs_t *vfsp, vnode_t **rootvpp, vnode_t *mvp, struct cred *cr)
{
    int          err = SUCCESS;
    iumfs_t     *iumfsp;
    vnode_t     *rootvp;

    DEBUG_PRINT((CE_CONT,"iumfs_create_fs_root is called\n"));

    iumfsp = VFS2IUMFS(vfsp);

    /*
     * ディレクトリを作成する。
     * iumfs_make_directory() の第３引数は、新規作成するディレクトリの親ディレクトリ
     * の vnode を指定するのだが、ここではマウントポイントのディレクトリの vnode を
     * 渡している。このため、マウント後のファイルシステムルートディレクトリの「..」
     * は実際の親ディレクトリの inode を示していない。実際問題 cd .. でも問題は
     * 発生しないので、あえてマウントポイントの親ディレクトリの情報を探すことはしない
     * でおく。
     */
    if ((err = iumfs_make_directory(vfsp, rootvpp, mvp, cr)) != SUCCESS){
        cmn_err(CE_CONT, "iumfs_create_fs_root: failed to create directory\n");
        return(err);
    }
    rootvp = *rootvpp;
    /*
     * ファイルシステムのルートディレクトリの vnode をセット
     */
    iumfsp->rootvnode = rootvp;

    return(SUCCESS);    
}


/*****************************************************************************
 * iumfs_add_iumnode_list()
 *
 * iumfs ファイルシステムプライベートデータ構造からリンクしている、
 * ファイルシステムのノードリストに指定された vnode を追加する。
 *
 *
 *  引数：
 *      vfsp   : vfs 構造体 
 *      newvp  : ノードリストに追加する vnode のポインタ
 *  
 * 戻り値：
 *          常に 0
 *****************************************************************************/
int
iumfs_add_node_to_list(vfs_t *vfsp, vnode_t *newvp)
{ 
    iumnode_t *newinp, *headinp, *previnp, *inp;
    iumfs_t   *iumfsp;

    DEBUG_PRINT((CE_CONT,"iumfs_add_node_to_list called\n"));

    newinp  = VNODE2IUMNODE(newvp);
    iumfsp  = VFS2IUMFS(vfsp);
    headinp = &iumfsp->node_list_head;

    previnp = headinp;
    mutex_enter(&(previnp->i_lock));
    while (previnp->next != NULL){
        inp = previnp->next;
        mutex_enter(&(inp->i_lock));
        mutex_exit(&(previnp->i_lock));
        previnp = inp;
    }
    previnp->next = newinp;
    mutex_exit(&(previnp->i_lock));
        
    return(SUCCESS);
}

/*****************************************************************************
 * iumfs_remove_node_from_list()
 * 
 * iumfs ファイルシステムプライベートデータ構造からリンクしている、
 * ファイルシステムのノードリストから指定された vnode を取り除く。
 *
 *  引数：
 *      vfsp   : vfs 構造体 
 *      rmvp  : ノードリストから取り除く vnode のポインタ
 *      
 * 戻り値：
 *          成功: SUCCESS(=0)
 *          失敗: 0 以外
 *****************************************************************************/
int
iumfs_remove_node_from_list(vfs_t *vfsp, vnode_t *rmvp)
{
    iumnode_t *rminp;    // これから削除する vnode のノード情報
    iumnode_t *headinp;  // ファイルシステムのノードリストのヘッド
    iumnode_t *inp;      // 操作用のノード情報のポインタ
    iumnode_t *previnp;  // 操作用のノード情報のポインタ
    iumfs_t   *iumfsp;

    DEBUG_PRINT((CE_CONT,"iumfs_remove_node_from_list called\n"));

    rminp   = VNODE2IUMNODE(rmvp);
    iumfsp  = VFS2IUMFS(vfsp);
    headinp = &iumfsp->node_list_head;
    
    previnp = headinp;
    mutex_enter(&(previnp->i_lock));

    while(previnp->next){
        inp = previnp->next;
        mutex_enter(&(inp->i_lock));
        if(inp == rminp){
            previnp->next = inp->next;
            mutex_exit(&(inp->i_lock));
            mutex_exit(&(previnp->i_lock));
            return(SUCCESS);
        }
        mutex_exit(&(previnp->i_lock));
        previnp = inp;
    }
    mutex_exit(&(previnp->i_lock));    
    
    cmn_err(CE_CONT,"iumfs_remove_node_from_list: cannot find node\n");
    return(ENOENT);
}

/*****************************************************************************
 * iumfs_free_all_node()
 *
 *  ファイルシステムの全ての vnode および iumnode を開放する。
 *  全ての vnode が利用されていないと分かった場合に iumfs_unmount() から呼ばれる。
 *
 *  引数：
 *         vfsp  : vnode を開放するファイルシステムの vfs 構造体
 *          
 * 戻り値：
 *          無し
 *****************************************************************************/
void
iumfs_free_all_node(vfs_t *vfsp)
{
    vnode_t   *rmvp;     // これから削除する vnode
    iumnode_t *rminp;    // これから削除する vnode のノード情報
    iumnode_t *headinp;  // ファイルシステムのノードリストのヘッド
    iumfs_t   *iumfsp;

    DEBUG_PRINT((CE_CONT,"iumfs_free_all_node called\n"));

    rminp   = VNODE2IUMNODE(rmvp);
    iumfsp  = VFS2IUMFS(vfsp);
    headinp = &iumfsp->node_list_head;

    rminp = headinp->next;
    while( rminp != NULL){
        rmvp = IUMNODE2VNODE(rminp);
        iumfs_free_node(rmvp);
        rminp = headinp->next;
    }
    return;
}

/************************************************************************
 * iumfs_make_directory()
 *
 *   新しいディレクトリを作成する。
 *   
 *    o 新規ディレクトリ用のの vnode,iumnode の確保、初期化
 *    o 新規ディレクトリのデフォルトのエントリ（. と ..)を作成
 *
 * 引数:
 *
 *     vfsp  : vfs 構造体
 *     vpp   : 呼び出し側から渡された vnode 構造体のポインタのアドレス
 *     mvp   : ディレクトリマウントポイントの vnode 構造体のポインタ
 *     cr    : システムコールを呼び出したユーザのクレデンシャル
 *
 * 戻り値:
 *
 *     成功時   : SUCCESS(=0)
 *     エラー時 : エラー番号
 *     
 ************************************************************************/
int  
iumfs_make_directory(vfs_t *vfsp, vnode_t **vpp, vnode_t *parentvp, struct cred *cr)
{
    int         err = SUCCESS;
    iumnode_t  *newdirinp;        // 新しいディレクトリの iumnode（ノード情報）
    vnode_t    *newdirvp;         // 新しいディレクトリの vnode 
    char        curdir[] = ".";
    char        pardir[] = "..";
    vattr_t     parentvattr;

    DEBUG_PRINT((CE_CONT,"iumfs_make_directory is called\n"));

    
    // 途中で break するためだけの do-while 文
    do{
        /*
         * 親ディレクトリ vnode、属性情報を得る
         * ディレクトリエントリ中の 「..」 の情報として使う。
         */
        parentvattr.va_mask = AT_ALL;
        err = VOP_GETATTR(parentvp, &parentvattr, 0, cr
#ifdef OPENSOLARIS
        ,NULL);
#else
        );
#endif // ifdef OPENSOLARIS
        if( err != SUCCESS){
            cmn_err(CE_CONT,"iumfs_make_directory: can't get parent directory's attribute\n");
            break;
        }

        // 新しいディレクトリの vnode を作成
        err = iumfs_alloc_node(vfsp, &newdirvp, VROOT, VDIR);
        if( err != SUCCESS){
            cmn_err(CE_CONT, "iumfs_make_directory: cannot allocate vnode for new directory\n");
            break;
        }

        // 新しいディレクトリのノード情報(iumnode) を得る。
        newdirinp = VNODE2IUMNODE(newdirvp);        

        // 新しいディレクトリの vnode の属性情報をセット
        newdirinp->vattr.va_mode     = 0040755; //00755;    
        newdirinp->vattr.va_size     = 0;
        newdirinp->vattr.va_nblocks  = 1;

        /*
         * ディレクトリにデフォルトのエントリを追加する。
         * 最初にディレクトリに存在するエントリは以下の２つ
         *   .    : カレントディレクトリ
         *   ..   : 親ディレクトリ
         */
        if(iumfs_add_entry_to_dir(newdirvp, curdir, sizeof(curdir), newdirinp->vattr.va_nodeid) < 0)
            break;
        if(iumfs_add_entry_to_dir(newdirvp, pardir, sizeof(pardir), parentvattr.va_nodeid)< 0)
            break;

        // 引数として渡されたポインタに新しい vnode のアドレスをセット
        *vpp = newdirvp;

    } while (0);

    // エラー時は確保したリソースを解放する。
    if(err){
        if(newdirvp != NULL)
            iumfs_free_node(newdirvp);
    }

    return(err);
}

/***********************************************************************
 * iumfs_add_entry_to_dir()
 *
 *  ディレクトリに、引数で渡された名前の新しいエントリを追加する。
 *
 *  引数:
 *
 *     dirvp     : ディレクトリの vnode 構造体
 *     name      : 追加するディレクトリエントリ（ファイル）の名前
 *     name_size : ディレクトリエントリの名前の長さ。（最後の NULL を含む長さ）
 *     nodeid    : 追加するディレクトリエントリのノード番号(inode番号)
 *
 *  返値
 *
 *  　　正常時   : 確保したディレクトリエントリ用のメモリのサイズ
 *      エラー時 : -1 
 *
 ***********************************************************************/
int
iumfs_add_entry_to_dir(vnode_t *dirvp, char *name, int name_size, ino_t nodeid)
{
    iumnode_t  *dirinp;
    dirent64_t   *new_dentp; // 新しいディレクトリエントリのポインタ
    uchar_t    *newp;      // kmem_zalloc() で確保したメモリへのポインタ
    offset_t    dent_total;  // 全てのディレクトリエントリの合計サイズ
    int         err;

    DEBUG_PRINT((CE_CONT,"iumfs_add_entry_to_dir is called\n"));

    DEBUG_PRINT((CE_CONT,"iumfs_add_entry_to_dir: file name = \"%s\", name len = %d, nodeid = %d\n",
                 name, name_size, nodeid));

    dirinp = VNODE2IUMNODE(dirvp);

    /*
     *  ディレクトリの iumnode のデータを変更するので、まずはロックを取得
     */
    mutex_enter(&(dirinp->i_lock));    
    /*
     * 確保するディレクトリエントリ分のサイズを求める
     */
    if (dirinp->data != NULL && dirinp->dlen > 0)
        dent_total = dirinp->dlen + DIRENT64_RECLEN(name_size);
    else
        dent_total = DIRENT64_RECLEN(name_size);

    DEBUG_PRINT((CE_CONT,"iumfs_add_entry_to_dir: dent_total = %d\n", dent_total));

    /*
     * ディレクトリエントリ用の領域を新たに確保
     */
    newp = (uchar_t *)kmem_zalloc(dent_total, KM_NOSLEEP);
    if(newp == NULL){
        cmn_err(CE_CONT, "iumfs_add_entry_to_dir: kmem_zalloc failed");
        err = ENOMEM;
        goto error;
    }

    /*
     * 既存のディレクトリエントリがあれば、それを新しく確保した
     * メモリにコピーし、その後開放する。
     */
    if (dirinp->data != NULL && dirinp->dlen > 0){
        bcopy(dirinp->data, newp, dirinp->dlen);
        /*
         * 危険な感じだが、ロックを取らずにこのデータを読む関数は
         * いないので、ここでフリーしても Panic は起きない・・はず。
         */
        kmem_free(dirinp->data, dirinp->dlen);

        new_dentp = (dirent64_t *)( newp + dirinp->dlen);
    } else {
        new_dentp = (dirent64_t *) newp;
    }

    /*
     * dirent 構造体に新しいディレクトリエントリの情報をセット
     */
    new_dentp->d_ino    = nodeid;
    new_dentp->d_off    = dirinp->dlen; /* 新しい dirent のオフセット。つまり現在の総 diernt の長さ */
    new_dentp->d_reclen = DIRENT64_RECLEN(name_size);
    bcopy(name, new_dentp->d_name, name_size);

    /*
     *  ディレクトリの iumnode の "data" が新しく確保したアドレスを
     *  さすように変更。
     */
    dirinp->data = (void *) newp;
    dirinp->dlen = dent_total;
    DEBUG_PRINT((CE_CONT,"iumfs_add_entry_to_dir: new directory size = %d\n", dirinp->dlen));

    /*
     * ディレクトリのサイズも新しく確保したメモリのサイズに変更
     * 同時にアクセス時間、変更時間も変更
     */
    dirinp->vattr.va_size = dent_total;
    dirinp->vattr.va_atime = iumfs_get_current_time();
    dirinp->vattr.va_mtime = iumfs_get_current_time();    
    
    /*
     * 正常終了。最終的に確保したディレクトリエントリのサイズを返す。
     */
    mutex_exit(&(dirinp->i_lock));    
    return(dent_total);

  error:
    
    if(newp)
        kmem_free(newp, dent_total);
    mutex_exit(&(dirinp->i_lock));        
    return(-1);
}

/***********************************************************************
 * iumfs_find_nodeid_by_name
 *
 *  ディレクトリの中から、引数で渡された名前のエントリを探す。
 *
 *  引数:
 *
 *     dirinp    : ディレクトリの iumnode 構造体
 *     name      : 検索するディレクトリエントリの名前
 *
 *  返値
 *
 *  　　見つかった時       : 見つかったディレクトリエントリの nodeid
 *      見つからなかった時 :  0
 *
 ***********************************************************************/
ino_t
iumfs_find_nodeid_by_name(iumnode_t *dirinp, char *name)
{
    offset_t        offset;
    ino_t      nodeid = 0;
    dirent64_t  *dentp;

    DEBUG_PRINT((CE_CONT,"iumfs_find_nodeid_by_name is called\n"));
    
    mutex_enter(&(dirinp->i_lock));
    dentp   = (dirent64_t *)dirinp->data;
    /*
     * ディレクトリの中に、引数で渡されたファイル名と同じ名前のエントリ
     * があるかどうかをチェックする。
     */
    for( offset = 0 ; offset < dirinp->dlen ; offset += dentp->d_reclen){
        dentp = (dirent64_t *)((char *)dirinp->data + offset);
        if (strcmp(dentp->d_name, name) == 0){
            nodeid = dentp->d_ino;
            DEBUG_PRINT((CE_CONT,"iumfs_find_nodeid_by_name: found \"%s\"(nodeid = %d)\n", name, nodeid));
            break;
        }
    }
    mutex_exit(&(dirinp->i_lock));
    
    if (nodeid == 0)
        DEBUG_PRINT((CE_CONT,"iumfs_find_nodeid_by_name: cannot find \"%s\"\n", name));

    return(nodeid);
}


/***********************************************************************
 * iumfs_dir_is_empty
 *
 * 指定されたディレクトリが空であるかをチェック。
 * （空 とは「.」と「..」しかない状態を指す）
 *
 *  引数:
 *
 *     dirvp    :  確認するディレクトリの vnode 構造体
 *
 *  戻り値:
 *
 *      空だった場合       : 1
 *      空じゃなかった場合 : 0
 *
 ***********************************************************************/
int
iumfs_dir_is_empty(vnode_t *dirvp)
{
    offset_t       offset;
    ino_t   nodeid = 0;
    dirent64_t       *dentp;
    iumnode_t      *dirinp;

    DEBUG_PRINT((CE_CONT,"iumfs_dir_is_empty is called\n"));

    dirinp = VNODE2IUMNODE(dirvp);

    mutex_enter(&(dirinp->i_lock));
    dentp   = (dirent64_t *)dirinp->data;
    /*
     * ディレクトリの中に、「.」と「..」以外の名前を持ったエントリ
     * があるかどうかをチェックする。
     */
    for( offset = 0 ; offset < dirinp->dlen ; offset += dentp->d_reclen){
        dentp = (dirent64_t *)((char *)dirinp->data + offset);
        if ((strcmp(dentp->d_name, ".") == 0) || (strcmp(dentp->d_name, "..") == 0)){
            // 「.」と「..」は無視、次へ
            continue;
        } 
        // 「.」と「..」以外のなにかが見つかった。
        nodeid = dentp->d_ino;
        DEBUG_PRINT((CE_CONT,"iumfs_dir_is_empty: not empty. found nodeid = %d\n", nodeid));        
        break;
    }
    mutex_exit(&(dirinp->i_lock));

    if(nodeid == 0)
        return(1); // 空だ
    else
        return(0); // 空じゃない
}

/***********************************************************************
 * iumfs_remove_entry_from_dir()
 *
 *  ディレクトリに、引数で渡された名前の新しいエントリを削除
 *
 *  引数:
 *
 *     vp       : ディレクトリの vnode 構造体
 *     name      : 削除するディレクトリエントリ（ファイル）の名前
 *
 *  返値
 *
 *  　　正常時   : 確保したディレクトリエントリ用のメモリのサイズ
 *      エラー時 : -1 
 *
 ***********************************************************************/
int
iumfs_remove_entry_from_dir(vnode_t *dirvp, char *name)
{
    offset_t    offset;    
    iumnode_t  *dirinp;
    dirent64_t   *new_dentp; // 新しいディレクトリエントリのポインタ
    dirent64_t   *dentp;     // 作業用ポインタ
    uchar_t    *newp = NULL; // kmem_zalloc() で確保したメモリへのポインタ
    uchar_t    *workp;     // bcopy() でディレクトリエントリをコピーするときの作業用ポインタ
    offset_t    dent_total;  // 全てのディレクトリエントリの合計サイズ
    offset_t    remove_dent_len = 0;// 削除するエントリのサイズ

    DEBUG_PRINT((CE_CONT,"iumfs_remove_entry_from_dir is called\n"));

    DEBUG_PRINT((CE_CONT,"iumfs_remove_entry_from_dir: file name = \"%s\"\n", name));

    dirinp = VNODE2IUMNODE(dirvp);

    /*
     *  ディレクトリの iumnode のデータを変更するので、まずはロックを取得
     */
    mutex_enter(&(dirinp->i_lock));

    dentp   = (dirent64_t *)dirinp->data;
    /*
     * ディレクトリの中から引数で渡されたファイル名と同じ名前のエントリ
     * を探し、あれば、そのエントリの長さを得る。
     */
    for( offset = 0 ; offset < dirinp->dlen ; offset += dentp->d_reclen){
        dentp = (dirent64_t *)((char *)dirinp->data + offset);
        if (strcmp(dentp->d_name, name) == 0){
            remove_dent_len = dentp->d_reclen;
            break;
        }
    }
    /*
     * 削除するエントリの長さが 0 だったら、それは先のループでエントリが
     * 見つけられなかったことを意味する。その場合、エラーを返す
     */
    if (remove_dent_len == 0){
        DEBUG_PRINT((CE_CONT,"iumfs_remove_entry_from_dir: cannot find requested entry\n"));
        goto error;
    }
    
    /*
     * 新たに確保するディレクトリエントリ分のサイズを求める。
     * 計算は単純に　
     * 　新しいサイズ = 前のサイズ - 削除するエントリのサイズ
     */
    dent_total = dirinp->dlen - remove_dent_len;

    /*
     * ディレクトリエントリ用の領域を新たに確保
     */
    newp = (uchar_t *)kmem_zalloc(dent_total, KM_NOSLEEP);
    if(newp == NULL){
        cmn_err(CE_CONT, "iumfs_remove_entry_from_dir: kmem_zalloc failed");
        goto error;
    }

    /*
     * 効率が悪いが、ディレクトの中のエントリをもう一度巡り、削除する
     * エントリを除く既存のエントリを、新しく確保した領域にコピーする。
     */
    dentp   = (dirent64_t *)dirinp->data;
    workp   = newp;
    for( offset = 0 ; offset < dirinp->dlen ; offset += dentp->d_reclen){
        dentp = (dirent64_t *)((char *)dirinp->data + offset);
        if (strcmp(dentp->d_name, name) == 0){
            // 削除するエントリはコピーしない。
            continue;
        }
        // そのほかの既存エントリは新しい領域にコピー
        bcopy(dentp, workp, dentp->d_reclen);
        workp += dentp->d_reclen;
    }    

    kmem_free(dirinp->data, dirinp->dlen);
    new_dentp = (dirent64_t *)( newp + dirinp->dlen);

    /*
     *  ディレクトリの iumnode の "data" が新しく確保したアドレスを
     *  さすように変更。
     */
    dirinp->data = (void *) newp;
    dirinp->dlen = dent_total;
    DEBUG_PRINT((CE_CONT,"iumfs_remove_entry_from_dir: new directory size = %d\n", dirinp->dlen));

    /*
     * ディレクトリのサイズも新しく確保したメモリのサイズに変更
     * ディレクトリの、参照時間、変更時間も変更
     */
    dirinp->vattr.va_size  = dent_total;
    dirinp->vattr.va_atime = iumfs_get_current_time();
    dirinp->vattr.va_mtime = iumfs_get_current_time();    
    
    /*
     * 正常終了。最終的に確保したディレクトリエントリのサイズを返す。
     */
    mutex_exit(&(dirinp->i_lock));    
    return(dent_total);

  error:
    if(newp)
        kmem_free(newp, dent_total);
    mutex_exit(&(dirinp->i_lock));        
    return(-1);
}


/***********************************************************************
 * iumfs_find_vnode_by_nodeid
 *
 *  ファイルシステム毎のノード一覧のリンクリストから、指定された nodeid
 *  をもつノードを検索し、vnode を返す。
 *
 *  引数:
 *
 *     iumfsp    : ファイルシステムのプライベートデータ構造体(iumfs_t)
 *     nodeid    : 検索する nodeid
 *
 *  返値
 *
 *      見つかった場合       : vnode 構造体のアドレス
 *      見つからなかった場合 : NULL
 *
 ***********************************************************************/
vnode_t *
iumfs_find_vnode_by_nodeid(iumfs_t *iumfsp, ino_t nodeid)
{
    iumnode_t   *inp, *previnp, *headinp;
    vnode_t     *vp = NULL;

    DEBUG_PRINT((CE_CONT,"iumfs_find_vnode_by_nodeid is called\n"));    

    headinp = &iumfsp->node_list_head;

    /*
     * ノードのリンクリストの中から該当する nodeid のものを探す。
     */
    previnp = headinp;
    mutex_enter(&(previnp->i_lock));
    while(previnp->next){
        inp = previnp->next;
        mutex_enter(&(inp->i_lock));
        if(inp->vattr.va_nodeid == nodeid){
            vp = IUMNODE2VNODE(inp);
            /*
             * iumnode のロックを取得している間に、vnode の参照カウントを
             * 増加させておく。こうすることで、vnode を返答したあとで、
             * その vnode が free されてしまうという問題を防ぐ。
             */
            VN_HOLD(vp);
            DEBUG_PRINT((CE_CONT,"iumfs_find_vnode_by_nodeid: found vnode 0x%p\n", vp));
            mutex_exit(&(inp->i_lock));
            break;
        }
        mutex_exit(&(previnp->i_lock));
        previnp = inp;
    }
    mutex_exit(&(previnp->i_lock));

    if (vp == NULL)
        DEBUG_PRINT((CE_CONT,"iumfs_find_vnode_by_nodeid: cannot find vnode \n"));        

    return(vp);
}

/************************************************
 * iumfs_set_current_time
 *
 * 現在時をセットした timestruct 構造体を返す。
 * 主に vattr 構造体の va_atime, va_ctime,va_mtime
 * をセットするために使う。
 * 
 ************************************************/
timestruc_t 
iumfs_get_current_time()
{
    return(time);
}

