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
 * iumfs_vnode
 *
 * 擬似ファイルシステム IUMFS の VNODE オペレーションの為のコード
 *
 * 変更履歴：
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
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <sys/uio.h>
#include <sys/vm.h>
#include <sys/exec.h>
// OpenSolaris の場合必要
#ifdef OPENSOLARIS
#include <sys/vfs_opreg.h>
#endif
#include "iumfs.h"


/* VNODE 操作プロトタイプ宣言 */
#ifdef SOL10
static int     iumfs_open   (vnode_t **, int , struct cred *);
static int     iumfs_close  (vnode_t *, int , int ,offset_t , struct cred *);
static int     iumfs_read   (vnode_t *, struct uio *, int , struct cred *);
static int     iumfs_write  (vnode_t *, struct uio *, int , struct cred *);
static int     iumfs_getattr(vnode_t *, vattr_t *, int ,  struct cred *);
static int     iumfs_access (vnode_t *, int , int , struct cred *);
static int     iumfs_lookup (vnode_t *, char *, vnode_t **,  struct pathname *,int ,vnode_t *, struct cred *);
static int     iumfs_readdir(vnode_t *, struct uio *,struct cred *, int *);
static int     iumfs_fsync  (vnode_t *, int , struct cred *);
static void    iumfs_inactive(vnode_t *, struct cred *);
static int     iumfs_seek   (vnode_t *, offset_t , offset_t *);
static int     iumfs_cmp    (vnode_t *, vnode_t *);
static int     iumfs_create (vnode_t *, char *, vattr_t *,  vcexcl_t , int ,
                             vnode_t **,struct cred *, int );
static int     iumfs_remove (vnode_t *, char *, struct cred *);
static int     iumfs_mkdir  (vnode_t *, char *,  vattr_t *, vnode_t **, struct cred *);
static int     iumfs_rmdir  (vnode_t *, char *, vnode_t *,struct cred *);
static int     iumfs_setattr(vnode_t *, vattr_t *, int ,struct cred *);
#else // ifdef SOL10
static int     iumfs_map    (vnode_t *, offset_t , struct as *,caddr_t *, size_t ,
                             uchar_t ,uchar_t , uint_t , struct cred *);
static int     iumfs_getpage(vnode_t *, offset_t , size_t ,uint_t *, struct page **,
                             size_t ,struct seg *, caddr_t , enum seg_rw ,struct cred *);
static int     iumfs_putpage(vnode_t *, offset_t , size_t ,int , struct cred *);
static int     iumfs_ioctl  (vnode_t *, int , intptr_t , int ,  struct cred *, int *);
static int     iumfs_setfl  (vnode_t *, int , int , struct cred *);

static int     iumfs_link   (vnode_t *, vnode_t *, char *,  struct cred *);
static int     iumfs_rename (vnode_t *, char *,  vnode_t *, char *, struct cred *);
static int     iumfs_symlink(vnode_t *, char *, vattr_t *, char *,struct cred *);
static int     iumfs_readlink(vnode_t *, struct uio *,struct cred *);
static int     iumfs_fid    (vnode_t *, struct fid *);
static void    iumfs_rwlock (vnode_t *, int );
static void    iumfs_rwunlock(vnode_t *, int );
static int     iumfs_frlock (vnode_t *, int , struct flock64 *,   int , offset_t ,
                             struct flk_callback *, struct cred *);
static int     iumfs_space  (vnode_t *, int , struct flock64 *,int , offset_t , struct cred *);
static int     iumfs_realvp (vnode_t *, vnode_t **);
static int     iumfs_addmap (vnode_t *, offset_t , struct as *,caddr_t , size_t ,
                             uchar_t ,  uchar_t , uint_t , struct cred *);
static int     iumfs_delmap (vnode_t *, offset_t , struct as *,caddr_t , size_t ,
                             uint_t ,uint_t , uint_t , struct cred *);
static int     iumfs_poll   (vnode_t *, short , int , short *,struct pollhead **);
static int     iumfs_dump   (vnode_t *, caddr_t , int ,  int );
static int     iumfs_pathconf(vnode_t *, int , ulong_t *,struct cred *);
static int     iumfs_pageio (vnode_t *, struct page *,u_offset_t , size_t , int ,  struct cred *);
static int     iumfs_dumpctl(vnode_t *, int , int *);
static void    iumfs_dispose(vnode_t *, struct page *, int ,int , struct cred *);
static int     iumfs_setsecattr(vnode_t *, vsecattr_t *, int ,struct cred *);
static int     iumfs_getsecattr(vnode_t *, vsecattr_t *, int ,struct cred *);
static int     iumfs_shrlock (vnode_t *, int , struct shrlock *,int );
static int     iumfs_getapage(vnode_t *, u_offset_t , size_t ,uint_t *, struct page *[],
                             size_t ,struct seg *, caddr_t , enum seg_rw ,struct cred *);
int            iumfs_putapage(vnode_t *vp, offset_t off, size_t len,int flags, struct cred *cr);
#endif // idfef SOL10
/*
 * このファイルシステムでサーポートする vnode オペレーション
 */
#ifdef SOL10
/*
 * Solaris 10 の場合、vnodeops 構造体は vfs_setfsops() にて得る。
 * OpenSolaris の場合さらに fs_operation_def_t の func メンバが union に代わっている
 */
struct vnodeops *iumfs_vnodeops;
#ifdef OPENSOLARIS
fs_operation_def_t iumfs_vnode_ops_def_array[] = {
    { VOPNAME_OPEN,    {&iumfs_open    }},
    { VOPNAME_CREATE,  {&iumfs_create  }},
    { VOPNAME_CLOSE,   {&iumfs_close   }},
    { VOPNAME_READ,    {&iumfs_read    }},
    { VOPNAME_WRITE,   {&iumfs_write   }},
    { VOPNAME_GETATTR, {&iumfs_getattr }},
    { VOPNAME_ACCESS,  {&iumfs_access  }},
    { VOPNAME_LOOKUP,  {&iumfs_lookup  }},
    { VOPNAME_MKDIR,   {&iumfs_mkdir   }},    
    { VOPNAME_READDIR, {&iumfs_readdir }},
    { VOPNAME_RMDIR,   {&iumfs_rmdir   }},    
    { VOPNAME_FSYNC,   {&iumfs_fsync   }},
    { VOPNAME_REMOVE,  {&iumfs_remove  }},    
    { VOPNAME_INACTIVE,{(fs_generic_func_p)&iumfs_inactive}},
    { VOPNAME_SEEK,    {&iumfs_seek    }},
    { VOPNAME_SETATTR, {&iumfs_setattr }},    
    { VOPNAME_CMP,     {&iumfs_cmp     }},
    { NULL, {NULL}},
};
#else
struct vnodeops *iumfs_vnodeops;
fs_operation_def_t iumfs_vnode_ops_def_array[] = {
    { VOPNAME_OPEN,    &iumfs_open    },
    { VOPNAME_CREATE,  &iumfs_create  },    
    { VOPNAME_CLOSE,   &iumfs_close   },
    { VOPNAME_READ,    &iumfs_read    },
    { VOPNAME_WRITE,   &iumfs_write   },    
    { VOPNAME_GETATTR, &iumfs_getattr },
    { VOPNAME_ACCESS,  &iumfs_access  },
    { VOPNAME_LOOKUP,  &iumfs_lookup  },
    { VOPNAME_MKDIR,   &iumfs_mkdir   },        
    { VOPNAME_READDIR, &iumfs_readdir },
    { VOPNAME_RMDIR,   &iumfs_rmdir   },
    { VOPNAME_FSYNC,   &iumfs_fsync   },
    { VOPNAME_REMOVE,  &iumfs_remove  },
    { VOPNAME_INACTIVE,(fs_generic_func_p)&iumfs_inactive},
    { VOPNAME_SEEK,    &iumfs_seek    },
    { VOPNAME_SETATTR, &iumfs_setattr },
    { VOPNAME_CMP,     &iumfs_cmp     },
    { NULL, NULL},
};
#endif // ifdef OPENSOLARIS
#else
/*
 * Solaris 9 の場合、vnodeops 構造体はファイルシステムが領域を確保し、直接参照できる
 */
struct vnodeops iumfs_vnodeops = {
    &iumfs_open,
    &iumfs_close,
    &iumfs_read,
    &iumfs_write, 
    &iumfs_ioctl,
    &iumfs_setfl,
    &iumfs_getattr,
    &iumfs_setattr,
    &iumfs_access, 
    &iumfs_lookup, 
    &iumfs_create, 
    &iumfs_remove, 
    &iumfs_link,   
    &iumfs_rename, 
    &iumfs_mkdir,  
    &iumfs_rmdir,  
    &iumfs_readdir,
    &iumfs_symlink,
    &iumfs_readlink,
    &iumfs_fsync,  
    &iumfs_inactive,
    &iumfs_fid,    
    &iumfs_rwlock, 
    &iumfs_rwunlock,
    &iumfs_seek,   
    &iumfs_cmp,    
    &iumfs_frlock, 
    &iumfs_space,  
    &iumfs_realvp, 
    &iumfs_getpage,
    &iumfs_putpage,
    &iumfs_map,    
    &iumfs_addmap, 
    &iumfs_delmap, 
    &iumfs_poll,   
    &iumfs_dump,   
    &iumfs_pathconf,
    &iumfs_pageio, 
    &iumfs_dumpctl,
    &iumfs_dispose,
    &iumfs_setsecattr,
    &iumfs_getsecattr,
    &iumfs_shrlock
};
#endif // ifdef SOL10

/************************************************************************
 * iumfs_open()  VNODE オペレーション
 *
 * open(2) システムコールに対応。
 * ここではほとんど何もしない。
 *************************************************************************/
static int
iumfs_open (vnode_t **vpp, int flag, struct cred *cr)
{
    vnode_t      *vp;

    DEBUG_PRINT((CE_CONT,"iumfs_open is called\n"));
    DEBUG_PRINT((CE_CONT,"iumfs_open: vpp = 0x%p, vp = 0x%p\n", vpp, *vpp));

    vp = *vpp;

    if(vp == NULL){
        DEBUG_PRINT((CE_CONT,"iumfs_open: vnode is null\n"));
        return(EINVAL);
    }
    
    return(SUCCESS);
}

/************************************************************************
 * iumfs_close()  VNODE オペレーション
 *
 * 常に 成功
 *************************************************************************/
static int
iumfs_close  (vnode_t *vp, int flag, int count,offset_t offset, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_close is called\n"));
    
    return(SUCCESS);
}

/************************************************************************
 * iumfs_read()  VNODE オペレーション
 *
 * read(2) システムコールに対応する。
 * ファイルのデータを uiomove を使って、ユーザ空間のアドレスにコピーする。
 *************************************************************************/
static int
iumfs_read (vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
    iumnode_t    *inp;
    int           err   = 0;
    offset_t      filedlen;
    uchar_t      *filedata;
    offset_t      offset; 
    
    DEBUG_PRINT((CE_CONT,"iumfs_read is called\n"));
    DEBUG_PRINT((CE_CONT,"iumfs_read: uiop->uio_offset = %d\n",uiop->uio_offset));    

    // ファイルシステム型依存のノード構造体を得る
    inp = VNODE2IUMNODE(vp);

    mutex_enter(&(inp->i_lock));
    filedata = (uchar_t *) inp->data;
    filedlen = inp->dlen;
    offset   = uiop->uio_offset;

    // break するためだけの do-while 文
    do {
        if(offset < filedlen && filedata != NULL){
            /*
             * ファイルのデータをユーザ空間にコピー。
             * もしデータ長がユーザが渡してきたバッファ長(uio_resid)より
             * 大きかったら uio_resid 分だけコピーする。
             */
            if ( uiop->uio_resid < filedlen - offset){
                err = uiomove(&(filedata[offset]), uiop->uio_resid, UIO_READ, uiop);
            } else {
                err = uiomove(&(filedata[offset]), filedlen - offset, UIO_READ, uiop);
            }
            if(err == SUCCESS)
                DEBUG_PRINT((CE_CONT,"iumfs_read: %d byte copied\n",filedlen - offset));            
        } else {
            // 0 byte のコピー。EOF のつもりだが、必要か？
            err = uiomove(filedata, 0, UIO_READ, uiop);
            if(err == SUCCESS)
                DEBUG_PRINT((CE_CONT,"iumfs_read: 0 byte copied\n"));
        }

    } while (FALSE);

    inp->vattr.va_atime = iumfs_get_current_time();
    
    mutex_exit(&(inp->i_lock));    
    return(err);    
}

/************************************************************************
 * iumfs_write()  VNODE オペレーション
 *
 * write(2) システムコールに対応する。
 *************************************************************************/
static int
iumfs_write  (vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
    iumnode_t    *inp;
    int           err   = SUCCESS;
    offset_t      filedlen, newfiledlen;
    uchar_t      *filedata , *newfiledata = NULL;
    offset_t      offset, resid; 
    
    DEBUG_PRINT((CE_CONT,"iumfs_write is called\n"));
    DEBUG_PRINT((CE_CONT,"iumfs_write: uiop->uio_offset = %d uiop->uio_resid = %d\n",
                 uiop->uio_offset, uiop->uio_resid));
    DEBUG_PRINT((CE_CONT,"iumfs_write: ioflag = 0x%x\n", ioflag));    

    // ファイルシステム型依存のノード構造体を得る
    inp = VNODE2IUMNODE(vp);

    mutex_enter(&(inp->i_lock));
    filedata = (uchar_t *) inp->data;

    DEBUG_PRINT((CE_CONT,"iumfs_write: vattr.va_size = %d\n", inp->vattr.va_size));        
    filedlen = inp->dlen;
    /*
     * もし ioflag に FAPPEND がセットされていたら、書き込みオフセットを
     * ファイルサイズ（ファイルの最後）にセットする。
     */
    if(ioflag & FAPPEND){
        offset   = filedlen;
    } else {
        offset   = uiop->uio_offset;
    }
    resid    = uiop->uio_resid;

    /*
     * 新しいファイルのデータ長を求める
     */
    newfiledlen = offset + resid;
    if (newfiledlen < filedlen)
        newfiledlen = filedlen;
    
    // 新しいファイルデータメモリを確保
    newfiledata = (uchar_t *)kmem_zalloc(newfiledlen, KM_NOSLEEP);
    if(newfiledata == NULL){
        cmn_err(CE_CONT, "iumfs_write: cannot allocate memory for data\n");
        err = ENOMEM;
        goto error;
    }

    /*
     * 既存のデータあれば、それを新しいコピーする。
     */
    if (filedata != NULL && filedlen > 0){
        bcopy(filedata, newfiledata, filedlen);
    }
        
    /*
     * ファイルのデータをユーザ空間から kernel 空間にコピー。
     */ 
    err = uiomove(&(newfiledata[offset]), resid, UIO_WRITE, uiop);
    if(err != SUCCESS){
        cmn_err(CE_CONT, "iumfs_write: cannot copy data\n");        
        goto error;
    }
        
    DEBUG_PRINT((CE_CONT,"iumfs_write: %d byte copied\n", resid));

    /*
     * 既存のデータあれば開放する。
     */    
    if (filedata != NULL && filedlen > 0){
        kmem_free(filedata, filedlen);
    }

    inp->data = (void *)newfiledata;
    inp->dlen = newfiledlen;

    // 新しい属性情報をセット
    inp->vattr.va_size     = newfiledlen;
    inp->vattr.va_nblocks  = (newfiledlen / BLOCKSIZE);
    inp->vattr.va_mtime    = iumfs_get_current_time();
    inp->vattr.va_atime    = iumfs_get_current_time();
    
  error:
    if(err && newfiledata)
        kmem_free(newfiledata, newfiledlen);

    mutex_exit(&(inp->i_lock));    
    return(err);        
}

/************************************************************************
 * iumfs_getattr()  VNODE オペレーション
 *
 * GETATTR ルーチン
 *************************************************************************/
static int
iumfs_getattr(vnode_t *vp, vattr_t *vap, int flags,  struct cred *cr)
{
    iumnode_t *inp;
    
    DEBUG_PRINT((CE_CONT,"iumfs_getattr is called\n"));

    DEBUG_PRINT((CE_CONT,"iumfs_getattr: va_mask = 0x%x\n", vap->va_mask));

    /*
     * ファイルシステム型依存のノード情報(iumnode 構造体)から vnode の属性情報をコピー。
     * 本来は、va_mask にて bit が立っている属性値だけをセットすればよいの
     * だが、めんどくさいので、全ての属性値を vap にコピーしてしまう。
     */
    inp = VNODE2IUMNODE(vp);
    bcopy(&inp->vattr, vap, sizeof(vattr_t));

    /* 
     * va_mask;      // uint_t           bit-mask of attributes        
     * va_type;      // vtype_t          vnode type (for create)      
     * va_mode;      // mode_t           file access mode             
     * va_uid;       // uid_t            owner user id                
     * va_gid;       // gid_t            owner group id               
     * va_fsid;      // dev_t(ulong_t)   file system id (dev for now) 
     * va_nodeid;    // ino_t          node id                      
     * va_nlink;     // nlink_t          number of references to file 
     * va_size;      // u_offset_t       file size in bytes           
     * va_atime;     // timestruc_t      time of last access          
     * va_mtime;     // timestruc_t      time of last modification    
     * va_ctime;     // timestruc_t      time file ``created''        
     * va_rdev;      // dev_t            device the file represents   
     * va_blksize;   // uint_t           fundamental block size       
     * va_nblocks;   // ino_t          # of blocks allocated        
     * va_vcode;     // uint_t           version code                
     */
    
    return(SUCCESS);
}

/************************************************************************
 * iumfs_setattr()  VNODE オペレーション
 *
 * SETATTR ルーチン。
 * ファイルの属性情報をセットする。現在は、MODE だけを変更可能
 *
 *************************************************************************/
static int
iumfs_setattr(vnode_t *vp, vattr_t *vap, int flags,struct cred *cr)
{
    iumnode_t *inp;
    int        err = SUCCESS;
    uint_t     mask;
    
    DEBUG_PRINT((CE_CONT,"iumfs_setattr is called\n"));
    DEBUG_PRINT((CE_CONT,"iumfs_setattr: va_mask = 0x%x\n", vap->va_mask));

    inp = VNODE2IUMNODE(vp);
    mask = vap->va_mask;

    /*
     * 現在は、MODE,ATIME,CTIME,MTIME だけを変更可能
     */
    if (!(mask & (AT_MODE|AT_ATIME|AT_CTIME|AT_MTIME))){
        err = ENOTSUP;
        return(err);
    }
    
    // モード
    if (mask & AT_MODE){
        mutex_enter(&(inp->i_lock));        
        inp->vattr.va_mode = vap->va_mode;
        mutex_exit(&(inp->i_lock));                
    }
    // 最終アクセス時間
    if (mask & AT_ATIME){ 
        mutex_enter(&(inp->i_lock));
        inp->vattr.va_atime = vap->va_atime;
        mutex_exit(&(inp->i_lock));
    }
    // 最終状態変更時間
    if (mask & AT_CTIME){ 
        mutex_enter(&(inp->i_lock));
        inp->vattr.va_ctime = vap->va_ctime;
        mutex_exit(&(inp->i_lock));
    }
    // 最終変更時間
    if (mask & AT_MTIME){ 
        mutex_enter(&(inp->i_lock));
        inp->vattr.va_mtime = vap->va_mtime;
        mutex_exit(&(inp->i_lock));
    }
    
    return(SUCCESS);
}

/************************************************************************
 * iumfs_access()  VNODE オペレーション
 *
 * 常に成功（アクセス可否を判定しない）
 *************************************************************************/
static int
iumfs_access (vnode_t *vp, int mode, int flags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_access is called\n"));

    // 常に成功
    return(SUCCESS);
}

/************************************************************************
 * iumfs_lookup()  VNODE オペレーション
 *
 *  引数渡されたファイル/ディレクトリ名（name）をディレクトリエントリから
 *  探し、もし存在すれば、そのファイル/ディレクトリの vnode のアドレスを
 *  引数として渡された vnode のポインタにセットする。
 *
 *************************************************************************/
static int
iumfs_lookup (vnode_t *dvp, char *name, vnode_t **vpp,  struct pathname *pnp, int flags,
              vnode_t *rdir, struct cred *cred)
{
    vnode_t      *vp = NULL;
    iumnode_t    *dinp;
    ino_t  nodeid = 0; // 64 bit のノード番号（ inode 番号）
    iumfs_t      *iumfsp;
    int           err;
    
    DEBUG_PRINT((CE_CONT,"iumfs_lookup is called\n"));

    DEBUG_PRINT((CE_CONT,"iumfs_lookup: pathname->pn_buf  = \"%s\"\n", pnp->pn_buf));
    DEBUG_PRINT((CE_CONT,"iumfs_lookup: name(file name)   = \"%s\"\n", name));

    iumfsp = VNODE2IUMFS(dvp);
    dinp   = VNODE2IUMNODE(dvp);

    nodeid = iumfs_find_nodeid_by_name(dinp, name);
    if(nodeid == 0){
        /*
         * ディレクトリエントリの中に該当するファイルが見つからなかった。
         */
        DEBUG_PRINT((CE_CONT,"iumfs_lookup: can't find \"%s\"\n", name));        
        return(ENOENT);
    }

    /*
     * デバッグ用
     */
    if(VN_CMP(VNODE2ROOT(dvp), dvp) != 0){
        DEBUG_PRINT((CE_CONT,"iumfs_lookup: directory vnode is rootvp\n"));
    }

    vp = iumfs_find_vnode_by_nodeid(iumfsp, nodeid);
    
    if(vp == NULL){
        if( strcmp(name, "..") == 0){
            /*
             * nodeid がこのファイルシステム上に存在せず、かつ親ディレクトリの lookup 要求だ。
             * 親ディレクトリの vnode を探してやる
             */
            DEBUG_PRINT((CE_CONT,"iumfs_lookup: look for a vnode of parent directory\n"));
            err = lookupname("/", UIO_SYSSPACE, FOLLOW, 0, &vp);
            /*
             * lookupname() が正常終了した場合は、親ディレクトリが存在するファイルシステムが
             * vp が指す vnode の参照カウントを増加させていると期待される。
             * なので、ここでは vnode に対して VN_HOLD() は使わない。
             */
            if(err){
                DEBUG_PRINT((CE_CONT,"iumfs_lookup: cannot find vnode of parent directory\n"));
                return(ENOENT);
            }
        } else {
            /*
             * 見つからなかった・・エラーを返す
             */
            DEBUG_PRINT((CE_CONT,"iumfs_lookup: cannot find vnode of specified nodeid(%d)\n", nodeid));
            return(ENOENT);
        }
    }

    *vpp = vp;
    return(SUCCESS);
}

/************************************************************************
 * iumfs_create()  VNODE オペレーション
 *
 * create(2) システムコールに対応する。
 *************************************************************************/
static int
iumfs_create (vnode_t *dirvp, char *name, vattr_t *vap,  vcexcl_t excl, int mode, vnode_t **vpp,
              struct cred *cr, int flag)
{
    vfs_t      *vfsp;
    int         err = SUCCESS;
    vnode_t    *newvp;
    iumnode_t  *newinp, *inp;

    DEBUG_PRINT((CE_CONT,"iumfs_create is called\n"));
    DEBUG_PRINT((CE_CONT,"iumfs_create: flag = 0x%x\n", flag));    

    vfsp = VNODE2VFS(dirvp);


    if(*vpp != NULL){
        // vnode が存在する                
        inp = VNODE2IUMNODE(*vpp);
        
        DEBUG_PRINT((CE_CONT,"iumfs_create: vnode already exists\n"));

        /*
         * もし va_mask の AT_SIZE フラグが立っており、va_size が 0 で
         * あった場合には、ファイルサイズを 0 にする。
         *
         * open(2) に O_TRUNC フラグが渡されたら、この関数の flag には FTRUNC
         * フラグが立っていると思っていたが、違った。
         * ファイルサイズを 0 にするかどうかを判断するには vattr の va_size を
         * 見るのが正しいようだ。
         */        
        if((vap->va_mask & AT_SIZE) && (vap->va_size == 0)){
            mutex_enter(&(inp->i_lock));
            // 既存のデータあれば開放する。
            if (inp->data != NULL && inp->dlen > 0){
                kmem_free(inp->data, inp->dlen);
                inp->data = NULL;
                inp->dlen = 0;
            }            
            // ファイルの vnode の属性情報をセット
            inp->vattr.va_size     = 0;
            inp->vattr.va_nblocks  = 0;
            inp->vattr.va_atime = iumfs_get_current_time();
            mutex_exit(&(inp->i_lock));
        }
        
        VN_HOLD(*vpp);
        return(SUCCESS);
    }

    /*
     * 新規ファイルの vnode を作成
     */
    do {
        
        err = iumfs_alloc_node(vfsp, &newvp, 0, VREG);
        if( err != 0){
            cmn_err(CE_CONT, "iumfs_create: cannot allocate vnode for file\n");
            break;
        }

        newinp = VNODE2IUMNODE(newvp);

        // ファイルの中身をセット
        newinp->data = NULL;
        newinp->dlen = 0;

        // ファイルの vnode の属性情報をセット
        newinp->vattr.va_mode     = 0100644; //0040644; //00644;
        newinp->vattr.va_size     = 0;
        newinp->vattr.va_nblocks  = 0;

        /*
         * ディレクトリにこのファイルのエントリを追加する。
         */
        if (iumfs_add_entry_to_dir(dirvp, name, strlen(name), newinp->vattr.va_nodeid) < 0){
            cmn_err(CE_CONT, "iumfs_create: cannot add new entry to directory\n");
            err = ENOSPC;
            break;
        }

        // 引数として渡されたポインタに新しい vnode のアドレスをセット
        *vpp = newvp;

        // vnode の参照カウントを増やす
        VN_HOLD(*vpp);        

    } while (FALSE);


    // エラー時は確保したリソースを解放する。
    if(err){
        if(newvp != NULL)
            iumfs_free_node(newvp);        
    }

    return(err);    
    
}

/************************************************************************
 * iumfs_remove()  VNODE オペレーション
 *
 * 
 *************************************************************************/
static int
iumfs_remove (vnode_t *pdirvp, char *name, struct cred *cr)
{
    vnode_t        *vp = NULL;       // 削除するファイルの vnode 構造体
    iumnode_t      *pdirinp;         // 親ディレクトリの iumnode 構造体    
    ino_t    nodeid  = 0;     // 削除するファイルのノード番号
    int             namelen;
    int             err = SUCCESS;
    iumfs_t        *iumfsp;
    
    DEBUG_PRINT((CE_CONT,"iumfs_remove is called\n"));

    namelen = strlen(name);
    iumfsp = VNODE2IUMFS(pdirvp);

    DEBUG_PRINT((CE_CONT,"iumfs_remove: removing \"%s\"\n", name));    

    pdirinp   = VNODE2IUMNODE(pdirvp);

    nodeid = iumfs_find_nodeid_by_name(pdirinp, name);
    
    if(nodeid == 0){
        /*
         * 親ディレクトリ中に該当するディレクトリが見つからなかった。
         */
        DEBUG_PRINT((CE_CONT,"iumfs_remove: can't find directory \"%s\"\n", name));
        err = ENOENT;
        goto error;
    }

    vp = iumfs_find_vnode_by_nodeid(iumfsp, nodeid);
    if(vp == NULL){
        DEBUG_PRINT((CE_CONT,"iumfs_remove: can't find file \"%s\"\n", name));
        err = ENOENT;
        goto error;
    }

    /*
     * ノードのタイプが VREG じゃ無かったらエラーを返す
     */
    if(!(vp->v_type & VREG)){
        DEBUG_PRINT((CE_CONT,"iumfs_remove: vnode is not a regular file.\n"));
        err = ENOTSUP;
        goto error;
    }

    /*
     * 親ディレクトリからエントリを削除
     */
    iumfs_remove_entry_from_dir(pdirvp, name);

    /*
     * 最後にディレクトリの vnode の参照カウントを減らす。
     * この vnode を参照中の人がいるかもしれないので（たとえば shell の
     * カレントディレクトリ）、ここでは free はしない。
     * 参照数が 1 になった段階で iumfs_inactive() が呼ばれ、iumfs_inactive()
     * から free される。
     */
    VN_RELE(vp); // 上の iumfs_find_vnode_by_nodeid() で増やされたの参照カウント分を減らす。
    VN_RELE(vp); // vnode 作成時に増加された参照カウント分を減らす。

    return(SUCCESS);

  error:
    /*
     * もし削除しようとしていたディレクトリの vnode のポインタを得ていたら、
     * vnode の参照数を減らす。
     */
    if (vp != NULL)
        VN_RELE(vp);

    return(err);
}


/************************************************************************
 * iumfs_mkdir()  VNODE オペレーション
 *
 * mkdir(2) システムコールに対応する。
 * 指定された名前の新規ディレクトリを作成する。
 * 
 *************************************************************************/
static int
iumfs_mkdir (vnode_t *dvp, char *dirname,  vattr_t *vap, vnode_t **vpp, struct cred *cr)
{
    int         err;
    vfs_t      *vfsp;
    vnode_t    *vp = NULL;
    iumnode_t  *inp;
    int         namelen;
    
    DEBUG_PRINT((CE_CONT,"iumfs_mkdir is called\n"));

    /*
     * まずは、渡されたディレクトリ名の長さをチェック
     */
    namelen = strlen(dirname);
    if (namelen > MAXNAMLEN)
        return(ENAMETOOLONG);

    vfsp = VNODE2VFS(dvp);

    /*
     * 指定された名前のディレクトリを作成する。
     */
    if ((err = iumfs_make_directory(vfsp, vpp, dvp, cr)) != SUCCESS){
        cmn_err(CE_CONT, "iumfs_mkdir: failed to create directory \"%s\"\n", dirname);
        goto error;
    }

    vp = *vpp;
    inp = VNODE2IUMNODE(vp);

    /*
     * 親ディレクトリ(dvp) に新しく作成したディレクトリのエントリを追加する。
     */
    if (iumfs_add_entry_to_dir(dvp, dirname, namelen, inp->vattr.va_nodeid) < 0 ){
        cmn_err(CE_CONT, "iumfs_mkdir: cannot add \"%s\" to directory\n", dirname);
        err = ENOSPC;
        goto error;
    }

    /*
     * vnode のポインタを返すので、参照数を増加させる
     */
    VN_HOLD(vp);
    
    return(SUCCESS);

  error:
    if(vp != NULL)
        iumfs_free_node(vp);
    
    return(err);
}

/************************************************************************
 * iumfs_rmdir()  VNODE オペレーション
 *
 * rmdir(2) システムコールに対応する。
 * 指定されたディレクトリを削除する。
 *
 * 引数:
 *     pdirvp : 削除するディレクトリの親ディレクトリの vnode ポインタ
 *     name   : 削除するディレクトリの名前
 *     cdirvp : カレントディレクトリの vnode ポインタ
 *     cr     : cread 構造体
 * 
 *************************************************************************/
static int
iumfs_rmdir (vnode_t *pdirvp, char *name, vnode_t *cdirvp, struct cred *cr)
{
    vnode_t        *dirvp = NULL;    // 削除するディレクトリの vnode 構造体
    iumnode_t      *pdirinp;         // 親ディレクトリの iumnode 構造体    
    ino_t    dir_nodeid  = 0; // 削除するディレクトリのノード番号
    int             namelen;
    int             err = SUCCESS;
    iumfs_t        *iumfsp;
    
    DEBUG_PRINT((CE_CONT,"iumfs_rmdir is called\n"));

    namelen = strlen(name);
    iumfsp = VNODE2IUMFS(pdirvp);

    DEBUG_PRINT((CE_CONT,"iumfs_rmdir: removing \"%s\"\n", name));    

    /*
     * 削除しようとしているのが、「.」や、「..」だったらエラーを返す
     */
    if ( (namelen == 1 && strcmp(name, ".") == 0) || (namelen == 2 && strcmp(name, "..") == 0)){
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: cannot remove \".\" or \"..\"\n"));        
        return(EINVAL);
    }

    pdirinp   = VNODE2IUMNODE(pdirvp);

    dir_nodeid = iumfs_find_nodeid_by_name(pdirinp, name);
    
    if(dir_nodeid == 0){
        /*
         * 親ディレクトリ中に該当するディレクトリが見つからなかった。
         */
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: can't find directory \"%s\"\n", name));
        err = ENOENT;
        goto error;
    }

    dirvp = iumfs_find_vnode_by_nodeid(iumfsp, dir_nodeid);
    if(dirvp == NULL){
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: can't find directory \"%s\"\n", name));
        err = ENOENT;
        goto error;
    }

    /*
     * ノードのタイプが VDIR じゃ無かったらエラーを返す
     */
    if(!(dirvp->v_type & VDIR)){
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: vnode is not a directory.\n"));
        err = ENOTDIR;
        goto error;
    }

    // カレントディレクトリを削除しようとしていないかチェック
    if (dirvp == cdirvp){
        err = EINVAL;
        goto error;
    }

    // 削除しようとしているディレクトリの中身をチェック
    if ( iumfs_dir_is_empty(dirvp) == 0){
        /*
         * ディレクトリは空ではない。
         */
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: directory \"%s\" is not empty\n", name));
        err = ENOTEMPTY;
        goto error;
    }

    /*
     * TODO: dirvp->v_count（参照カウント）をチェックしなければ・・
     * iumfs_inactive() を活用か？ iumfs_free_node() は v_count のチェックはしていない。
     * 一貫性のある vnode の削除方針を決めなければ・・
     */

    /*
     * TODO:
     * 上の、iumfs_dir_is_empty() と、下の iumfs_remove_entry_from_dir() の処理の
     * 間に新しいエントリが追加されると有無を言わさずそのエントリが削除されて
     * しまう・・ここでディレクトリの iumnode のロックを取ればよいのだが、
     * ここではロックは取りたくない。
     * (デッドロックの発生を防ぐため、iumnode ロックを取った状態で、他の
     * iumfs の関数を呼び出さないような方針にしているので）
     */

    /*
     * 親ディレクトリからエントリを削除
     */
    iumfs_remove_entry_from_dir(pdirvp, name);

    /*
     * 最後にディレクトリの vnode の参照カウントを減らす。
     * この vnode を参照中の人がいるかもしれないので（たとえば shell の
     * カレントディレクトリ）、ここでは free はしない。
     * 参照数が 1 になった段階で iumfs_inactive() が呼ばれ、iumfs_inactive()
     * から free される。
     */
    VN_RELE(dirvp); // 上の iumfs_find_vnode_by_nodeid() で増やされたの参照カウント分を減らす。
    VN_RELE(dirvp); // vnode 作成時に増加された参照カウント分を減らす。

    return(SUCCESS);

  error:
    /*
     * もし削除しようとしていたディレクトリの vnode のポインタを得ていたら、
     * vnode の参照数を減らす。
     */
    if (dirvp != NULL)
        VN_RELE(dirvp);

    if (err)
        return(err);
    else
        return(ENOTSUP);
}

/************************************************************************
 * iumfs_readdir()  VNODE オペレーション
 *
 * getdent(2) システムコールに対応する。
 * 引数で指定された vnode がさすディレクトリのデータを読み、dirent 構造体
 * を返す。
 *************************************************************************/
static int
iumfs_readdir(vnode_t *vp, struct uio *uiop, struct cred *cr, int *eofp)
{
    dirent_t    *dent;
    offset_t     dent_total;
    iumnode_t   *inp;
    int          err;

    DEBUG_PRINT((CE_CONT,"iumfs_readdir is called.\n"));

    // ノードのタイプが VDIR じゃ無かったらエラーを返す
    if(!(vp->v_type & VDIR)){
        DEBUG_PRINT((CE_CONT,"iumfs_readdir: vnode is not a directory.\n"));
        return(ENOTDIR);
    }

    // ファイルシステム型依存のノード構造体を得る
    inp = VNODE2IUMNODE(vp);

    mutex_enter(&(inp->i_lock));
    
    dent = (dirent_t *)inp->data;
    dent_total = inp->dlen;

    DEBUG_PRINT((CE_CONT,"iumfs_readdir: dent_total = %d\n",dent_total));
    DEBUG_PRINT((CE_CONT,"iumfs_readdir: uiop->uio_offset = %d\n",uiop->uio_offset));        

    if(uiop->uio_offset < dent_total){
        err = uiomove(dent, dent_total, UIO_READ, uiop);
        if(err == SUCCESS)
            DEBUG_PRINT((CE_CONT,"iumfs_readdir: %d byte copied\n", dent_total));    
    } else {
        err = uiomove(dent, 0, UIO_READ, uiop);
        if(err == SUCCESS)
            DEBUG_PRINT((CE_CONT,"iumfs_readdir: 0 byte copied\n"));            
    }
    inp->vattr.va_atime    = iumfs_get_current_time();
    
    mutex_exit(&(inp->i_lock));    
    
    return(err);
}

/************************************************************************
 * iumfs_fsync()  VNODE オペレーション
 *
 * 常に成功
 *************************************************************************/
static int
iumfs_fsync  (vnode_t *vp, int syncflag, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_fsync is called\n"));
    
    return(SUCCESS);
}

/************************************************************************
 * iumfs_inactive()  VNODE オペレーション
 *
 * vnode の 参照数（v_count）が 0 になった場合に VFS サブシステムから
 * 呼ばれる・・と思っていたが、これが呼ばれるときは v_count はかならず 1
 * のようだ。
 *
 * v_count が 0 になるのは、iumfs_rmdir で明示的に参照数を 1 にしたときのみ。
 * 
 *************************************************************************/
static void
iumfs_inactive(vnode_t *vp, struct cred *cr)
{
    vnode_t    *rootvp;
    
    DEBUG_PRINT((CE_CONT,"iumfs_inactive is called\n"));

    /*
     * 実際のファイルシステムでは、ここで、変更されたページのディスクへ
     * の書き込みが行われ、vnode の開放などが行われる。
     */

    rootvp = VNODE2ROOT(vp);

    if(rootvp == NULL)
        return;

    // この関数が呼ばれるときは v_count は常に 1 のようだ。
    DEBUG_PRINT((CE_CONT,"iumfs_inactive: vp->v_count = %d\n",vp->v_count ));
    
    if(VN_CMP(rootvp, vp) != 0){
        DEBUG_PRINT((CE_CONT,"iumfs_inactive: vnode is rootvp\n"));
    } else {
        DEBUG_PRINT((CE_CONT,"iumfs_inactive: vnode is not rootvp\n"));
    }

    // iumfsnode, vnode を free する。
    iumfs_free_node(vp);
    
    return;
}

/************************************************************************
 * iumfs_seek()  VNODE オペレーション
 *
 * 常に成功
 *************************************************************************/
static int
iumfs_seek (vnode_t *vp, offset_t ooff, offset_t *noffp)
{
    DEBUG_PRINT((CE_CONT,"iumfs_seek is called\n"));
    
    DEBUG_PRINT((CE_CONT,"iumfs_seek: ooff = %d, noffp = %d\n", ooff, *noffp));
    
    return(SUCCESS);
}

/************************************************************************
 * iumfs_cmp()  VNODE オペレーション
 *
 * 二つの vnode のアドレスを比較。
 *
 * 戻り値
 *    同じ vnode : 1
 *    違う vnode : 0
 *************************************************************************/
static int
iumfs_cmp (vnode_t *vp1, vnode_t *vp2)
{
    DEBUG_PRINT((CE_CONT,"iumfs_cmp is called\n"));

    // VN_CMP マクロに習い、同じだったら 1 を返す
    if (vp1 == vp2)
        return(1);
    else
        return(0);
}

#ifndef SOL10
/************************************************************************
 * iumfs_ioctl()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_ioctl  (vnode_t *vp, int cmd, intptr_t arg, int flag,  struct cred *cr, int *rvalp)
{
    DEBUG_PRINT((CE_CONT,"iumfs_ioctl is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_setfl()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_setfl  (vnode_t *vp, int oflags, int nflags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_setfl is called\n"));
    
    return(ENOTSUP);
}


/************************************************************************
 * iumfs_link()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_link   (vnode_t *tdvp, vnode_t *svp, char *tnm,  struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_link is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_rename()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_rename (vnode_t *sdvp, char *snm,  vnode_t *tdvp, char *tnm, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_rename is called\n"));
    
    return(ENOTSUP);
}


/************************************************************************
 * iumfs_symlink()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_symlink(vnode_t *dvp, char *linkname, vattr_t *vap, char *target,struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_symlink is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_readlink()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_readlink(vnode_t *vp, struct uio *uiop,struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_readlink is called\n"));
    
    return(ENOTSUP);
}


/************************************************************************
 * iumfs_fid()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_fid    (vnode_t *vp, struct fid *fidp)
{
    DEBUG_PRINT((CE_CONT,"iumfs_fid is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_rwlock()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static void
iumfs_rwlock (vnode_t *vp, int write_lock)
{
    DEBUG_PRINT((CE_CONT,"iumfs_rwlock is called\n"));
    
    return;
}

/************************************************************************
 * iumfs_rwunlock()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static void
iumfs_rwunlock(vnode_t *vp, int write_lock)
{
    DEBUG_PRINT((CE_CONT,"iumfs_rwunlock is called\n"));
    
    return;
}

/************************************************************************
 * iumfs_space()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_space (vnode_t *vp, int cmd, struct flock64 *bfp,int flag, offset_t offset, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_space is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_realvp()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_realvp (vnode_t *vp, vnode_t **vpp)
{
    DEBUG_PRINT((CE_CONT,"iumfs_realvp is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_getpage()  VNODE オペレーション
 *
 * サポートしていない
 *
 * TODO: いずれファイルの実データを他のファイルシステムの実ストレージに
 *       格納するようになった場合には、実装せねばならない。
 *       今は、kmem_zalloc()によって確保したページ不可なセグメントに
 *       ファイルデータが格納されており、ファイルサイズが大きくなると
 *       システムメモリーが足りなくなるのは必死。
 *************************************************************************/
static int
iumfs_getpage(vnode_t *vp, offset_t off, size_t len,uint_t *protp, struct page **plarr,
              size_t plsz,struct seg *seg, caddr_t addr, enum seg_rw rw,struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_getpage is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_putpage()  VNODE オペレーション
 *
 * サポートしていない
 *
 * TODO: いずれファイルの実データを他のファイルシステムの実ストレージに
 *       格納するようになった場合には、実装せねばならない。
 *       今は、kmem_zalloc()によって確保したページ不可なセグメントに
 *       ファイルデータが格納されており、ファイルサイズが大きくなると
 *       システムメモリーが足りなくなるのは必死。
 *************************************************************************/
static int
iumfs_putpage(vnode_t *vp, offset_t off, size_t len,int flags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_putpage is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_map()  VNODE オペレーション
 *
 * サポートしていない
 * 
 * TODO: 要サポート。実行ファイルの exec(2) にはこの vnode オペレーションの
 *       サポートが必須。なので、現在はこのファイルシステム上にある実行
 *       可能ファイルを exec(2) することはできない。
 *************************************************************************/
static int
iumfs_map (vnode_t *vp, offset_t off, struct as *as,caddr_t *addrp, size_t len,
           uchar_t prot,uchar_t maxprot, uint_t flags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_map is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_addmap()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_addmap (vnode_t *vp, offset_t off, struct as *as,caddr_t addr, size_t len,
              uchar_t prot,  uchar_t maxprot, uint_t flags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_addmap is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_delmap()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_delmap (vnode_t *vp, offset_t off, struct as *as,caddr_t addr, size_t len,
              uint_t prot,uint_t maxprot, uint_t flags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_delmap is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_poll()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_poll   (vnode_t *vp, short ev, int any, short *revp,struct pollhead **phpp)
{
    DEBUG_PRINT((CE_CONT,"iumfs_poll is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_dump()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_dump (vnode_t *vp, caddr_t addr, int lbdn,  int dblks)
{
    DEBUG_PRINT((CE_CONT,"iumfs_dump is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_pathconf()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_pathconf(vnode_t *vp, int cmd, ulong_t *valp,struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_pathconf is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_pageio()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_pageio (vnode_t *vp, struct page *pp,u_offset_t io_off, size_t io_len, int flags,  struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_pageio is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_dumpctl()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_dumpctl(vnode_t *vp, int action, int *blkp)
{
    DEBUG_PRINT((CE_CONT,"iumfs_dumpctl is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_dispose()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static void
iumfs_dispose(vnode_t *vp, struct page *pp, int flag,int dn, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_dispose is called\n"));
    
    return;
}

/************************************************************************
 * iumfs_setsecattr()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_setsecattr(vnode_t *vp, vsecattr_t *vsap, int flag,struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_setsecattr is called\n"));
    
    return(ENOTSUP);
}

/************************************************************************
 * iumfs_getsecattr()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_getsecattr(vnode_t *vp, vsecattr_t *vsap, int flag,struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_getsecattr is called\n"));
    
//    return(ENOTSUP);
    return(SUCCESS);
}

/************************************************************************
 * iumfs_shrlock()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_shrlock (vnode_t *vp, int cmd, struct shrlock *shr,int flag)
{
    DEBUG_PRINT((CE_CONT,"iumfs_shrlock is called\n"));
    
    return(ENOTSUP);
}


/************************************************************************
 * iumfs_frlock()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_frlock (vnode_t *vp, int cmd, struct flock64 *bfp,   int flag, offset_t offset,
              struct flk_callback *callback, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT,"iumfs_frlock is called\n"));
    
    return(ENOTSUP);
}
#endif // ifndef SOL10
