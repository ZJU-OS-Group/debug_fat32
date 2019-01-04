#include <zjunix/vfs/vfs.h>
#include <zjunix/vfs/err.h>
#include <zjunix/vfs/errno.h>
#include <driver/vga.h>
#include <errno.h>

/****************************** 外部变量 *******************************/
extern struct dentry                    * pwd_dentry;   /* 当前工作目录 */
extern struct vfsmount                  * pwd_mnt;
// TODO: vfs_open
// TODO: path_lookup


const char* __my_strcat(const u8* dest,const u8* src)
{
    char* res = dest;
    while(*res) res++;// *res != '\0'
    while(*res++ = *src++);
    return dest;
}

// cat：连接文件并打印到标准输出设备上
u32 vfs_cat(const u8 *path) {
    u8 *buf;
    u32 err;
    u32 base;
    u32 file_size;
    struct file *file;

    // 打开文件
    file = vfs_open(path, O_RDONLY);
    // 处理打开文件的错误
    if (IS_ERR_OR_NULL(file)){
        if ( PTR_ERR(file) == -ENOENT )
            kernel_printf("No such file.\n");
        return PTR_ERR(file);
    }

    // 读取文件内容到缓存区
     base = 0;
     file_size = file->f_dentry->d_inode->i_size;

     buf = (u8*) kmalloc ((unsigned int) (file_size + 1));
     if ( vfs_read(file, buf, file_size, &base) != file_size )
         return 1;

     // 打印buf里面的内容
     buf[file_size] = 0;
     kernel_printf("%s\n", buf);

     // 关闭文件并释放内存
     err = vfs_close(file);
     if (err)
         return err;

     kfree(buf);
    return 0;
}

// mkdir：新建目录
u32 vfs_mkdir(const u8 * path) {
    //
    // 判断是否可以创建

//    if (!dir->i_op || !dir->i_op->mkdir)
//        return -EPERM;
//
//    mode &= (S_IRWXUGO|S_ISVTX);
//    error = security_inode_mkdir(dir, dentry, mode);
//    if (error)
//        return error;
//
//    DQUOT_INIT(dir);
//    error = dir->i_op->mkdir(dir, dentry, mode);
//    if (!error) {
//        inode_dir_notify(dir, DN_CREATE);
//        security_inode_post_mkdir(dir,dentry, mode);
//    }
//    return error;
}

// rm：删除文件
u32 vfs_rm(const u8 * path) {
    u32 err;
    struct nameidata nd;

    /* 查找目的文件 */
    err = path_lookup(path, 0, &nd);
    if (err == -ENOENT) { // 返回No such file or directory的错误信息
        kernel_printf("No such file.\n");
        return err;
    } else if (IS_ERR_VALUE(err)) { // 如果有其他错误
        kernel_printf("Other error: %d\n", err);
        return err;
    }

    /* 删除对应文件在外存上的相关信息 */
    // 由dentry去找inode去找super_block去调用相关的删除文件的操作
    err = nd.dentry->d_inode->i_sb->s_op->delete_dentry_inode(nd.dentry);
    if (err)
        return err;

    /* 删除缓存中的inode */
    nd.dentry->d_inode = 0;

    return 0;
}

// rm -r：递归删除目录
u32 vfs_rm_r(const u8 * path) {
    u32 err;
    struct file *file;
    struct getdent getdent;

    // 打开目录
    if (path[0] == 0) {
        kernel_printf("No parameter.\n");
        return -ENOENT;
    }
    else
        file = vfs_open(path, LOOKUP_DIRECTORY);
    if (file->f_dentry->d_inode->i_type!=FTYPE_DIR)
        vfs_rm(path);
    if (IS_ERR_OR_NULL(file)) {
        if (PTR_ERR(file) == -ENOENT)
            kernel_printf("Directory not found!\n");
        else
            kernel_printf("Other error: %d\n", -PTR_ERR(file));
        return PTR_ERR(file);
    }
    err = file->f_op->readdir(file, &getdent);
    if (err)
        return err;
    // TODO: 遍历目录下每一项，若是文件直接调用rm，否则递归调用vfs_rm_r
    for (int i = 0; i < getdent.count; ++i) {
        if (getdent.dirent[i].type == FTYPE_DIR) {
            const u8* tmp_path = __my_strcat(path, "/");
            const u8* new_path = __my_strcat(tmp_path, getdent.dirent[i].name);
            vfs_rm_r(new_path);
        } else if (getdent.dirent[i].type == FTYPE_NORM) {
            const u8* tmp_path = __my_strcat(path, "/");
            const u8* new_path = __my_strcat(tmp_path, getdent.dirent[i].name);
            vfs_rm(new_path);
        } else if (getdent.dirent[i].type == FTYPE_LINK) {
            // TODO: 如何处理链接文件
        } else {
            return -ENOENT;
        }
    }
    return 0;
}

// ls：列出目录项下的文件信息
u32 vfs_ls(const u8 * path) {
    u32 err;
    struct file *file;
    struct getdent getdent;

    // 打开目录
    if (path[0] == 0)
        file = vfs_open(".", LOOKUP_DIRECTORY);
    else
        file = vfs_open(path, LOOKUP_DIRECTORY);
    if (IS_ERR_OR_NULL(file)) {
        if (PTR_ERR(file) == -ENOENT)
            kernel_printf("Directory not found!\n");
        else
            kernel_printf("Other error: %d\n", -PTR_ERR(file));
        return PTR_ERR(file);
    }

    // 读取目录到gedent中
    err = file->f_op->readdir(file, &getdent);
    if (err)
        return err;

    // 遍历gedent，向屏幕打印结果
    for (int i = 0; i < getdent.count; ++i) {
        // TODO: 这里之后可以添加上根据文件类型打印不同颜色的功能
        kernel_puts(getdent.dirent[i].name, VGA_WHITE,VGA_BLACK);
        kernel_printf(" ");
    }
    kernel_printf("\n");
    return 0;
}

// cd：切换当前工作目录
u32 vfs_cd(const u8 * path) {
    u32 err;
    struct nameidata nd;

    /* 查找目的目录 */
    err = path_lookup(path, LOOKUP_DIRECTORY, &nd);
    if (err == -ENOENT) { // 返回No such file or directory的错误信息
        kernel_printf("No such directory.\n");
        return err;
    } else if (IS_ERR_VALUE(err)) { // 如果有其他错误
        kernel_printf("Other error: %d\n", err);
        return err;
    }

    /* 一切顺利，更改dentry和mnt */
    pwd_dentry = nd.dentry;
    pwd_mnt = nd.mnt;
    return 0;
}

// mv：移动文件（同目录下移动则为重命名）
u32 vfs_mv(const u8 * path) {

}
