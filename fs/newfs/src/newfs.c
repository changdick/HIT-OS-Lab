#define _XOPEN_SOURCE 700

#include "newfs.h"

/******************************************************************************
* SECTION: 宏定义
*******************************************************************************/
#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }

/******************************************************************************
* SECTION: 全局变量
*******************************************************************************/
static const struct fuse_opt option_spec[] = {		/* 用于FUSE文件系统解析参数 */
	OPTION("--device=%s", device),
	FUSE_OPT_END
};

struct custom_options newfs_options;			 /* 全局选项 */
struct newfs_super super; 
/******************************************************************************
* SECTION: FUSE操作定义
*******************************************************************************/
static struct fuse_operations operations = {
	.init = newfs_init,						 /* mount文件系统 */		
	.destroy = newfs_destroy,				 /* umount文件系统 */
	.mkdir = newfs_mkdir,					 /* 建目录，mkdir */
	.getattr = newfs_getattr,				 /* 获取文件属性，类似stat，必须完成 */
	.readdir = newfs_readdir,				 /* 填充dentrys */
	.mknod = newfs_mknod,					 /* 创建文件，touch相关 */
	.write = newfs_write,					/* 写入文件 */
	.read = newfs_read,						/* 读文件 */
	.utimens = newfs_utimens,				 /* 修改时间，忽略，避免touch报错 */
	.truncate = newfs_truncate,				/* 改变文件大小 */
	.unlink = newfs_unlink,					/* 删除文件 */
	.rmdir	= newfs_rmdir,				 /* 删除目录， rm -r */
	.rename = newfs_rename,					/* 重命名，mv */

	.open = newfs_open,							
	.opendir = newfs_opendir,
	.access = newfs_access
};
/******************************************************************************
* SECTION: 必做函数实现
*******************************************************************************/
/**
 * @brief 挂载（mount）文件系统
 * 
 * @param conn_info 可忽略，一些建立连接相关的信息 
 * @return void*
 */
void* newfs_init(struct fuse_conn_info * conn_info) {
	/* TODO: 在这里进行挂载 */

	/* 下面是一个控制设备的示例 */
	// super.fd = ddriver_open(newfs_options.device);

	if (newfs_mount(newfs_options) != ERROR_NONE) {
        NFS_DBG("[%s] mount error\n", __func__);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	} 
	return NULL;

	
}

/**
 * @brief 卸载（umount）文件系统
 * 
 * @param p 可忽略
 * @return void
 */
void newfs_destroy(void* p) {
	/* TODO: 在这里进行卸载 */
	
	if (newfs_umount() != ERROR_NONE) {
		NFS_DBG("[%s] unmount error\n", __func__);
		fuse_exit(fuse_get_context()->fuse);
		
	}
	
	return;
}

/**
 * @brief 创建目录
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建模式（只读？只写？），可忽略 mode 就是uint
 * @return int 0成功，否则返回对应错误号
 */
int newfs_mkdir(const char* path, mode_t mode) {
	/* TODO: 解析路径，创建目录 */
	(void)mode;
	boolean is_find, is_root;
	char* fname;
	struct newfs_dentry* last_dentry = newfs_lookup(path, &is_find, &is_root);
	struct newfs_dentry* dentry;
	struct newfs_inode*  inode;

	if (is_find) {
		return -ERROR_EXISTS;
	}

	if (INODE_IS_REG(last_dentry->inode)) {
		return -ERROR_UNSUPPORTED;
	}

	fname  = newfs_get_fname(path);
	dentry = new_dentry(fname, WZT_DIR); 
	dentry->parent = last_dentry;
	inode  = newfs_alloc_inode(dentry);
	newfs_alloc_dentry(last_dentry->inode, dentry);

	
	return ERROR_NONE;  // return 0; 
}

/**
 * @brief 获取文件或目录的属性，该函数非常重要
 * 
 * @param path 相对于挂载点的路径
 * @param newfs_stat 返回状态
 * @return int 0成功，否则返回对应错误号
 */
int newfs_getattr(const char* path, struct stat * newfs_stat) {
	/* TODO: 解析路径，获取Inode，填充newfs_stat，可参考/fs/simplefs/sfs.c的sfs_getattr()函数实现 */

	boolean	is_find, is_root;
	struct newfs_dentry* dentry = newfs_lookup(path, &is_find, &is_root);
	if (is_find == FALSE) {
		return -ERROR_NOTFOUND;
	}

	if (INODE_IS_DIR(dentry->inode)) {
		newfs_stat->st_mode = S_IFDIR | NEWFS_DEFAULT_PERM;
		newfs_stat->st_size = dentry->inode->dir_cnt * sizeof(struct newfs_dentry_d);
	}
	else if (INODE_IS_REG(dentry->inode)) {
		newfs_stat->st_mode = S_IFREG | NEWFS_DEFAULT_PERM;
		newfs_stat->st_size = dentry->inode->size;
	}

	newfs_stat->st_nlink = 1;
	newfs_stat->st_uid 	 = getuid();
	newfs_stat->st_gid 	 = getgid();
	newfs_stat->st_atime   = time(NULL);
	newfs_stat->st_mtime   = time(NULL);
	newfs_stat->st_blksize = LOGIC_BLOCK_SIZE;  // 逻辑块大小
	newfs_stat->st_blocks = DATA_PER_FILE;
	if (is_root) {
		newfs_stat->st_size	= super.sz_usage; 
		newfs_stat->st_blocks = DISK_SIZE / LOGIC_BLOCK_SIZE;  // 磁盘块数/块大小 没有错
		newfs_stat->st_nlink  = 2;		/*根目录link数为2 */
	}
	return ERROR_NONE;

	
}

/**
 * @brief 遍历目录项，填充至buf，并交给FUSE输出
 * 
 * @param path 相对于挂载点的路径
 * @param buf 输出buffer
 * @param filler 参数讲解:
 * 
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *				const struct stat *stbuf, off_t off)
 * buf: name会被复制到buf中
 * name: dentry名字
 * stbuf: 文件状态，可忽略
 * off: 下一次offset从哪里开始，这里可以理解为第几个dentry
 * 
 * @param offset 第几个目录项？
 * @param fi 可忽略
 * @return int 0成功，否则返回对应错误号
 */
int newfs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
			    		 struct fuse_file_info * fi) {
    /* TODO: 解析路径，获取目录的Inode，并读取目录项，利用filler填充到buf，可参考/fs/simplefs/sfs.c的sfs_readdir()函数实现 */
    boolean	is_find, is_root;
	int		cur_dir = offset;

	struct newfs_dentry* dentry = newfs_lookup(path, &is_find, &is_root);
	struct newfs_dentry* sub_dentry;
	
	if (is_find) {
		struct newfs_inode* inode;
		inode = dentry->inode;     
		sub_dentry = newfs_get_dentry(inode, cur_dir);   // 根据当前偏移找到对应dentry
		if (sub_dentry) {
			filler(buf, sub_dentry->name, NULL, ++offset);
		}
		return ERROR_NONE;
	}
	return -ERROR_NOTFOUND;
	
}

/**
 * @brief 创建文件
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建文件的模式，可忽略
 * @param dev 设备类型，可忽略
 * @return int 0成功，否则返回对应错误号
 */
int newfs_mknod(const char* path, mode_t mode, dev_t dev) {
	/* TODO: 解析路径，并创建相应的文件 */
	boolean	is_find, is_root;
	
	struct newfs_dentry* last_dentry = newfs_lookup(path, &is_find, &is_root);
	struct newfs_dentry* dentry;
	struct newfs_inode* inode;
	char* fname;
	
	if (is_find == TRUE) {
		return -ERROR_EXISTS;
	}

	fname = newfs_get_fname(path);
	
	if (S_ISREG(mode)) {
		dentry = new_dentry(fname, WZT_FILE);
	}
	else if (S_ISDIR(mode)) {
		dentry = new_dentry(fname, WZT_DIR);
	}
	dentry->parent = last_dentry;
	inode = newfs_alloc_inode(dentry);
	newfs_alloc_dentry(last_dentry->inode, dentry);

	return ERROR_NONE;
	
}

/**
 * @brief 修改时间，为了不让touch报错 
 * 
 * @param path 相对于挂载点的路径
 * @param tv 实践
 * @return int 0成功，否则返回对应错误号
 */
int newfs_utimens(const char* path, const struct timespec tv[2]) {
	(void)path;
	return 0;
}
/******************************************************************************
* SECTION: 选做函数实现
*******************************************************************************/
/**
 * @brief 写入文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 写入的内容
 * @param size 写入的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 写入大小
 */
int newfs_write(const char* path, const char* buf, size_t size, off_t offset,
		        struct fuse_file_info* fi) {
	/* 选做 */
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = newfs_lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;

	if (is_find == FALSE) {
		return -ERROR_NOTFOUND;
	}
	inode = dentry->inode;
	if (INODE_IS_DIR(inode)) {
		return -ERROR_ISDIR;
	}

	if (inode->size < offset) {
		return -ERROR_SEEK;
	}
	memcpy(inode->data_ptr[0] + offset, buf, size);
	inode->size = offset + size > inode->size ? offset + size : inode->size;
	// 应该为文件分配数据块，通过block_used 和文件size的关系来判断是否需要分配新的数据块
	
	// 下面进行数据块的分配。先计算出该文件总共需要的数据块，然后和block_used字段比较就知道要不要分配新的数据块了。
	int required_block = (inode->size + LOGIC_BLOCK_SIZE - 1) / LOGIC_BLOCK_SIZE;
	if (required_block > inode->block_used) {
		inode->data_block[inode->block_used++] = wztfs_alloc_data();
	}


	return size;
}

/**
 * @brief 读取文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 读取的内容
 * @param size 读取的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 读取大小
 */
int newfs_read(const char* path, char* buf, size_t size, off_t offset,
		       struct fuse_file_info* fi) {
	/* 选做 */
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = newfs_lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;

	if (is_find == FALSE) {
		return -ERROR_NOTFOUND;
	}
	// 找到对应文件的目录项后，判断文件类型
	inode = dentry->inode;
	if (INODE_IS_DIR(inode)) {
		return -ERROR_ISDIR;
	}
	// 文件大小判断，若文件大小比offset小报错
	if (inode->size < offset) {
		return -ERROR_SEEK;
	}

	// 由于修改了文件读入的方式，文件数据读入内存后一定是连续的。因此data_ptr[0]就是内存中数据块的起点
	memcpy(buf, inode->data_ptr[0] + offset, size);
	return size;			   
}

/**
 * @brief 删除文件
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_unlink(const char* path) {
	/* 选做 */
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = newfs_lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;

	if (is_find == FALSE) {
		return -ERROR_NOTFOUND;
	}

	inode = dentry->inode;

	wztfs_drop_inode(inode);
	wztfs_drop_dentry(dentry->parent->inode, dentry);

	return ERROR_NONE;
}

/**
 * @brief 删除目录
 * 
 * 一个可能的删除目录操作如下：
 * rm ./tests/mnt/j/ -r
 *  1) Step 1. rm ./tests/mnt/j/j
 *  2) Step 2. rm ./tests/mnt/j
 * 即，先删除最深层的文件，再删除目录文件本身
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_rmdir(const char* path) {
	/* 选做 */
	return newfs_unlink(path);
}

/**
 * @brief 重命名文件 
 * 
 * @param from 源文件路径
 * @param to 目标文件路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_rename(const char* from, const char* to) {
	/* 选做 */
	int ret = ERROR_NONE;
	boolean	is_find, is_root;
	struct newfs_dentry* from_dentry = newfs_lookup(from, &is_find, &is_root);
	struct newfs_inode*  from_inode;
	struct newfs_dentry* to_dentry;
	mode_t mode = 0;

	if (is_find == FALSE) {
		return -ERROR_NOTFOUND;
	}
	if (strcmp(from, to) == 0) {
		return ERROR_NONE;
	}
	from_inode = from_dentry->inode;
	if (INODE_IS_DIR(from_inode)) {
		mode = S_IFDIR;
	} else if(INODE_IS_REG(from_inode)) {
		mode = S_IFREG;
	}

	ret = newfs_mknod(to, mode, NULL);

	if (ret != ERROR_NONE) {					  /* 保证目的文件不存在 */
		return ret;
	}
	to_dentry = newfs_lookup(to, &is_find, &is_root);
	wztfs_drop_inode(to_dentry->inode);
	to_dentry->ino = from_inode->ino;
	to_dentry->inode = from_inode;
	to_dentry->inode->dentry = to_dentry;


	wztfs_drop_dentry(from_dentry->parent->inode, from_dentry);

	return ret;
}

/**
 * @brief 打开文件，可以在这里维护fi的信息，例如，fi->fh可以理解为一个64位指针，可以把自己想保存的数据结构
 * 保存在fh中
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则返回对应错误号
 */
int newfs_open(const char* path, struct fuse_file_info* fi) {
	/* 选做 */

	return 0;
}

/**
 * @brief 打开目录文件
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则返回对应错误号
 */
int newfs_opendir(const char* path, struct fuse_file_info* fi) {
	/* 选做 */
	return 0;
}

/**
 * @brief 改变文件大小
 * 
 * @param path 相对于挂载点的路径
 * @param offset 改变后文件大小
 * @return int 0成功，否则返回对应错误号
 */
int newfs_truncate(const char* path, off_t offset) {
	/* 选做 */
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = newfs_lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;


	if (is_find == FALSE) {
		return -ERROR_NOTFOUND;
	}
	inode = dentry->inode;
	if (INODE_IS_DIR(inode)) {
		return -ERROR_ISDIR;	
	}
	inode->size = offset;

	return ERROR_NONE;
}


/**
 * @brief 访问文件，因为读写文件时需要查看权限
 * 
 * @param path 相对于挂载点的路径
 * @param type 访问类别
 * R_OK: Test for read permission. 
 * W_OK: Test for write permission.
 * X_OK: Test for execute permission.
 * F_OK: Test for existence. 
 * 
 * @return int 0成功，否则返回对应错误号
 */
int newfs_access(const char* path, int type) {
	/* 选做: 解析路径，判断是否存在 */
	boolean	is_find, is_root;
	boolean is_access_ok = FALSE;
	struct newfs_dentry* dentry = newfs_lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;
	switch (type)
	{
	case R_OK:
		is_access_ok = TRUE;
		break;
	case F_OK:
		if (is_find) {
			is_access_ok = TRUE;
		}
		break;	
	case W_OK:
		is_access_ok = TRUE;
		break;
	case X_OK:
		is_access_ok = TRUE;
		break;

	default:
		break;
	}
	return is_access_ok ? ERROR_NONE : -ERROR_ACCESS;
}	
/******************************************************************************
* SECTION: FUSE入口
*******************************************************************************/
int main(int argc, char **argv)
{
    int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	newfs_options.device = strdup("/home/students/220110430/ddriver");

	if (fuse_opt_parse(&args, &newfs_options, option_spec, NULL) == -1)
		return -1;
	
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}

// 挂载并创建目录和文件，都成功了，同时mnt文件夹里也能看到东西，但是再次挂z载就没有了 2024/12/7  20:36
// 应该是卸载之后，东西就丢失了, dump后面全是0
// 幻数忘记了宏定义，

// 刷回磁盘的函数可能有问题，应该在inode里面做一个使用了的块的个数，写回的时候只写回使用了的块

// 2024/12/7 23：57 在inode里面加入了已使用块数block_used字段，用于记录已经使用的块数，并更新到read_inode和sync_inode函数中，
// 重新挂载还是见不到东西
// 先创建文件夹，后创建文件，然后卸载，然后挂载，可以看到上次的文件。再卸载再挂载，可以。而且好像有概率不可以？？

// 先创建文件，后创建文件夹，然后卸载，然后挂在，看不到上次的文件。？？



// 2024/12/8 2:53 找到现在没找出问题出在哪里

//2024 12/8 7:10  data_offset 是325632

// 2024/12/8 8：21 bug到底是什么？ 并不是file0和dir0谁先创建导致的，而是创建第一、第二个，读取不到；第三第四个创建后，可以读取第一第二个