#include "newfs.h"

extern struct custom_options newfs_options;			 /* 全局选项 */
extern struct newfs_super super; 


/**
 * @brief 创建新的目录项dentry
 * 
 * @param name 目录项文件名字
 * @param type 目录项文件类型
 */
struct newfs_dentry* new_dentry(const char* name, FILE_TYPE type) {
    struct newfs_dentry* dentry = (struct newfs_dentry*)malloc(sizeof(struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    WZT_ASSIGN_FNAME(dentry, name);
    dentry->type = type;
    dentry->ino = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL; 
    dentry->brother = NULL;  // 一共六个字段 
    return dentry;
}


/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* newfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
} // definitely no problem

/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int newfs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
} // definitely no problem

/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int newfs_driver_read(int offset, uint8_t *out_content, int size) {
	int      offset_aligned = OFFSET_ROUND_DOWN(offset, LOGIC_BLOCK_SIZE);  //对齐按1024
	int      bias           = offset - offset_aligned;
	int      size_aligned   = OFFSET_ROUND_UP((size + bias), LOGIC_BLOCK_SIZE);
	uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);   // 申请一个上下都对齐了的缓冲区
	uint8_t* cur            = temp_content;                     // 缓冲区指针


	ddriver_seek(DRIVER_FD(), offset_aligned, SEEK_SET);
	while (size_aligned != 0)
	{
		ddriver_read(DRIVER_FD(), cur, IO_BLOCK_SIZE);
		cur          += IO_BLOCK_SIZE;               // 读写磁盘按照io块大小512来读
		size_aligned -= IO_BLOCK_SIZE;   
	}
	memcpy(out_content, temp_content + bias, size);
	free(temp_content);

	return ERROR_NONE;  // return 0;
	
} // absolutely no problem

/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int newfs_driver_write(int offset, uint8_t *in_content, int size) {
	int      offset_aligned = OFFSET_ROUND_DOWN(offset, LOGIC_BLOCK_SIZE);
	int      bias           = offset - offset_aligned;
	int      size_aligned   = OFFSET_ROUND_UP((size + bias), LOGIC_BLOCK_SIZE);

	uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
	uint8_t* cur            = temp_content;

	newfs_driver_read(offset_aligned, temp_content, size_aligned);  // 此处直接调用上面接口

	memcpy(temp_content + bias, in_content, size);
	ddriver_seek(DRIVER_FD(), offset_aligned, SEEK_SET);
	while (size_aligned != 0)
	{
		ddriver_write(DRIVER_FD(), cur, IO_BLOCK_SIZE);
		cur          += IO_BLOCK_SIZE;
		size_aligned -= IO_BLOCK_SIZE;   
	}
	free(temp_content);

	return ERROR_NONE;  // return 0;
} // absolutely no problem

/**
 * @brief 将denry插入到inode中，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int newfs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry) {
    
    // 此函数，在内存中管理inode和dentry的。inode是一个父目录的inode，dentry是子目录。
    // 在内存中，inode使用链表管理其下拥有的dentrys。和sfs是一样的原理。
    // sfs的构造不管数据块，但是我们要求按需分配数据块。所以要考虑inode的数据块按需分配
    


    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }



    inode->dir_cnt++;
    inode->size += sizeof(struct newfs_dentry);
	int MAX_DENTRY_PER_BLOCK = DENTRY_PER_BLK();   // 先声明，后面用宏定义补
	// 如果当前储存dentry的数据块已存满，则需要开辟新的数据块储存dentry
	// 如何判断父目录数据块是否已满，用inode->dir_cnt，和已知每个数据块最多存储的dentry数量
	// 如果dir_cnt % MAX_DENTRY_PER_BLOCK == 0，则说明此前数据块已满。
	// 而现在插入新的dentry，dir_cnt++，则取余后为1，说明此前数据块已满，加入一个后需要新开辟一个数据块
	if (inode->dir_cnt % MAX_DENTRY_PER_BLOCK == 1) {
		// int byte_cursor = 0; 
        // int bit_cursor = 0;
        // int blkno_cursor = 0;    
		// int blk_cnt = inode->dir_cnt / MAX_DENTRY_PER_BLOCK;     //2024/12/7 增加了已使用块数block_used字段后，不再需要实时计算即将分配的是第几块   // 第blk_cnt个block_pointer还未被使用
		// boolean is_find_free_data_blk = FALSE;
		// // 遍历数据块位图，找到一个空闲的数据块
		// for (byte_cursor = 0; byte_cursor < BLOCKS_SIZE(1); byte_cursor++)
        //     {
        //         for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
        //             if((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
        //                                                     /* 当前数据块位置空闲 */
        //                 super.map_data[byte_cursor] |= (0x1 << bit_cursor);

        //                 inode->data_block[blk_cnt] = blkno_cursor;
        //                 is_find_free_data_blk = TRUE;
        //                 break;
        //             }
        //             blkno_cursor++;
        //         }
        //         if (is_find_free_data_blk) {
        //             break;
        //         }
        //     }

        //     // 未找到空闲数据块
        //     if (!is_find_free_data_blk || blkno_cursor == super.max_data)
        //         return -ERROR_NOSPACE;
        inode->data_block[inode->block_used++] = wztfs_alloc_data(); // 分配数据块的同时，已使用数据块数加1

	}


    return inode->dir_cnt;
}

// alloc_dentry 中会用到分配数据块，把他提出来做一个函数
/**
 * @brief 分配一个空闲数据块，返回数据块号
 * 
 * @return int 
 */
int wztfs_alloc_data() {
    int byte_cursor       = 0; 
    int bit_cursor        = 0;   
    int dno_cursor        = 0; 
    int is_find_free_data = 0;

    for (byte_cursor = 0; byte_cursor < BLOCKS_SIZE(super.map_data_blks); byte_cursor++) {
        // 再在该字节中遍历8个bit寻找空闲的inode位图
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {
               
                super.map_data[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_data = 1;
                break;
            }
            dno_cursor++;
        }
        if (is_find_free_data) {
            break;
        }
    }

    if (!is_find_free_data || dno_cursor >= super.max_data) {
        return -ERROR_NOSPACE;
    }

    return dno_cursor;
} // definitely no problem

/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return nfs_inode
 */
struct newfs_inode* newfs_alloc_inode(struct newfs_dentry * dentry) {

    // 为一个dentry分配对应的inode。创建文件的时候，先创建dentry，再用该函数分配inode。
    // 要根据位图，查找可用的inode块来使用。

    // 因为创建的文件都是空文件，创建的目录也暂时无需分配数据块， 不管数据块的问题。

    struct newfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;    // 记录inode块号
    boolean is_find_free_entry = FALSE;

    // 在索引节点位图上查找空闲的索引节点 
    for (byte_cursor = 0; byte_cursor < BLOCKS_SIZE(1); byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    // 未找到空闲结点  !!!!2024/12/8 8:59 这里不对！！！ 条件里面应该是判断ino_cursor是否大于等于super.inode_nums
    // 本来写的不是inode_nums，是max_inode而是map_inode_blks,也就是ino_cursor为1的时候直接返回了，也就是ino=1直接返回了，ino=1的索引节点永远是空的
    if (!is_find_free_entry || ino_cursor == super.inode_nums)
        return -ERROR_NOSPACE;

    // 为目录项分配inode节点并建立他们之间的连接
    inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;
    inode->dir_cnt = 0;
    inode->dentrys = NULL;
    inode->block_used = 0;  //2024/12/7 23:50, 增加了新的字段，同步更新

    /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
    /* inode指回dentry */
    inode->dentry = dentry;
    

    /* 如果inode指向文件类型，则需要分配数据指针。如果是目录则不需要，目录项已存在dentrys中*/
    // inode是内存里面的索引节点，分配指向内存中数据块的指针

    // 应该改一下，改成直接分配一个大的连续数据块，使得数据块是连续的。
    if (INODE_IS_REG(inode)) {
        uint8_t *large_block = (uint8_t *)malloc(DATA_PER_FILE * LOGIC_BLOCK_SIZE);

        for(int i = 0; i < DATA_PER_FILE; i++){
            // inode->data_ptr[i] = (uint8_t *)malloc(LOGIC_BLOCK_SIZE);
            inode->data_ptr[i] = large_block + i * LOGIC_BLOCK_SIZE;
        }

    }

    return inode;
}

/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int newfs_sync_inode(struct newfs_inode * inode) {
    struct newfs_inode_d  inode_d;   // 申请一个写回磁盘的inode对象（不是指针），把inode先复制进inode_d再写回
    struct newfs_dentry*  dentry_cursor;
    struct newfs_dentry_d dentry_d;
    /* inode_d 6个字段*/
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    inode_d.type       = inode->dentry->type;
    inode_d.dir_cnt     = inode->dir_cnt;
    inode_d.block_used  = inode->block_used;

    int blk_cnt = 0;
    for(blk_cnt = 0; blk_cnt < DATA_PER_FILE; blk_cnt++) {
        inode_d.data_block[blk_cnt] = inode->data_block[blk_cnt]; /* 数据块的块号也要赋值 */
    }
    int offset, offset_limit;  /* 用于密集写回 dentry */
    
    // inode_d 刷回磁盘
    if (newfs_driver_write(INODE_OFFSET(ino), (uint8_t *)&inode_d, 
                     sizeof(struct newfs_inode_d)) != ERROR_NONE) {
        NFS_DBG("[%s] io error\n", __func__);
        return -ERROR_IO;
    }
                                           
    // 下面将inode对应的数据块刷回磁盘。要根据inode对应的文件类型（文件或目录）采取不同操作
    if (INODE_IS_DIR(inode)) { 
        printf("%s 被写回磁盘",inode->dentry->name); //妈的，根本看不到输出
        blk_cnt = 0;                    
        dentry_cursor = inode->dentrys;   // 理解一下dentrys它是内存里面连续存储的目录项，dentry_cursor是用来遍历这些目录项的指针
        

        //条件有没有问题？
        while(dentry_cursor != NULL && blk_cnt < inode->block_used) {
            // offset用于定位磁盘上的位置
            offset = DATA_OFFSET(inode->data_block[blk_cnt]); // dentry 从 inode 分配的首个数据块开始存
            offset_limit = DATA_OFFSET(inode->data_block[blk_cnt] + 1);
            /* 写满一个 blk 时换到下一个 bno */
            // 这个内层的while，是针对inode->datablock[blk_cnt]这个数据块的，把这个数据块里面的目录项进行写回。一个数据块里的目录项写完，就去写下一个数据块目录项如果有需要。

            // 控制内层while循环的条件：1.目录项写完了 2.blk_cnt这个数据块写满了
            while ((dentry_cursor != NULL) && (offset + sizeof(struct newfs_dentry_d) < offset_limit))
            {
                /*下面三行是把内存中的目录项dentry复制到写入磁盘的dentry_d中*/
                memcpy(dentry_d.name, dentry_cursor->name, MAX_NAME_LEN);
                dentry_d.type = dentry_cursor->type;
                dentry_d.ino = dentry_cursor->ino;
                // dentry_d写回磁盘 ， offset是在磁盘上的偏移
                if (newfs_driver_write(offset, (uint8_t *)&dentry_d, 
                                    sizeof(struct newfs_dentry_d)) != ERROR_NONE) {
                    NFS_DBG("[%s] io error\n", __func__);
                    return -ERROR_IO;                     
                }
                
                if (dentry_cursor->inode != NULL) {
                    newfs_sync_inode(dentry_cursor->inode); /* 完成子目录写回 */
                }

                dentry_cursor = dentry_cursor->brother;  // 写下一个 子目录 ，内存中下一个dentry，实现内存中的遍历
                offset += sizeof(struct newfs_dentry_d);  // 磁盘上的偏移

            }

            blk_cnt++; // 访问下一个指向的数据块
        }
    } else if (INODE_IS_REG(inode)){  /* 如果是文件 */
        // 如果全部写回去，可能并不恰当。因为没有校验。原本判断条件for(blk_cnt = 0; blk_cnt < DATA_PER_FILE; blk_cnt++)
        printf("%s 被写回磁盘",inode->dentry->name);
        // int used_blocks = (inode->size + LOGIC_BLOCK_SIZE - 1) / LOGIC_BLOCK_SIZE;
        int used_blocks = inode->block_used; //2024/12/7 23:52 加了新的字段，已经使用的块数可以直接使用inode自己记录的已使用块数block_used
        for(blk_cnt = 0; blk_cnt < used_blocks; blk_cnt++){
            if (newfs_driver_write(DATA_OFFSET(inode->data_block[blk_cnt]), 
                    inode->data_ptr[blk_cnt], LOGIC_BLOCK_SIZE) != ERROR_NONE) {
                NFS_DBG("[%s] io error\n", __func__);
                return -ERROR_IO;
            }
        }
    }
    return ERROR_NONE;
}

/**
 * @brief 读取指定dentry的inode. 读入参数是dentry和ino，dentry和ino是对应的，调用者负责确定对应关系
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct newfs_inode* 
 */
struct newfs_inode* newfs_read_inode(struct newfs_dentry * dentry, int ino) {

    // 传入的是内存中的dentry结构， 传入的ino是磁盘上的inode编号。
    

    
    struct newfs_inode* inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    struct newfs_inode_d inode_d;
    struct newfs_dentry* sub_dentry; /* 指向 子dentry 数组 */
    struct newfs_dentry_d dentry_d;
    int    blk_cnt = 0; /* 用于读取多个 bno */
    int    dir_cnt = 0, offset, offset_limit; /* 用于读取目录项 不连续的 dentrys */

    if (newfs_driver_read(INODE_OFFSET(ino), (uint8_t *)&inode_d, 
                        sizeof(struct newfs_inode_d)) != ERROR_NONE) {
        NFS_DBG("[%s] io error\n", __func__);
        return NULL;
    }

    inode->dir_cnt = 0; // inode是文件，目录项数量为0，是目录，这个值才有意义，到后面会重新赋值
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    inode->dentry = dentry;     /* 指回父级 dentry*/
    inode->block_used = inode_d.block_used;
    inode->dentrys = NULL;
    // 数据块块号赋值
    for(blk_cnt = 0; blk_cnt < DATA_PER_FILE; blk_cnt++)
        inode->data_block[blk_cnt] = inode_d.data_block[blk_cnt];
    
    // 根据inode对应类型，读取不同的数据
    // 判断如果是目录
    if (INODE_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        blk_cnt = 0;

        // 如果是目录，数据块里面就是存储目录项dentry_d型，用dentry_d读取出来
        while((dir_cnt > 0) && (blk_cnt < DATA_PER_FILE)){
            // offset是磁盘中的偏移量
            offset = DATA_OFFSET(inode->data_block[blk_cnt]); // dentry 从 inode 分配的首个数据块开始存。 
            offset_limit = DATA_OFFSET(inode->data_block[blk_cnt] + 1);
            /* 写满一个 blk 时换到下一个 bno */
            // 下面这个循环在同一个数据块 data_block[blk_cnt]里面连续读取目录项
            while ((dir_cnt > 0) && (offset + sizeof(struct newfs_dentry_d) < offset_limit))
            {
                if (newfs_driver_read(offset, (uint8_t *)&dentry_d, 
                                    sizeof(struct newfs_dentry_d)) != ERROR_NONE) {
                    NFS_DBG("[%s] io error\n", __func__);
                    return NULL;                    
                }
                
                sub_dentry = new_dentry(dentry_d.name, dentry_d.type);
                sub_dentry->parent = inode->dentry;
                sub_dentry->ino    = dentry_d.ino; 
                newfs_alloc_dentry(inode, sub_dentry);

                offset += sizeof(struct newfs_dentry_d);
                dir_cnt--;
             
            }
            blk_cnt++; /* 访问下一个指向的数据块 */
        }
    } else if (INODE_IS_REG(inode)) {  /* 如果是文件 */
        // for (int i = 0; i < DATA_PER_FILE; i++){
        //     inode->data_ptr[i] = (uint8_t *)malloc(LOGIC_BLOCK_SIZE);
        //     if (newfs_driver_read(DATA_OFFSET(inode->data_block[i]), (uint8_t *)inode->data_ptr[i], 
        //                         LOGIC_BLOCK_SIZE) != ERROR_NONE) {
        //         NFS_DBG("[%s] io error\n", __func__);
        //         return NULL;                    
        //     }
        // }
        
        // 改个方法，不是每个数据块分别申请内存空间，而是一次性给所有数据块申请内存空间，然后分别用指针去指。
        // 这样使得一个文件的数据在内存中连续。 外层读取就方便一点。
        uint8_t *large_block = (uint8_t *)malloc(DATA_PER_FILE * LOGIC_BLOCK_SIZE);
        for (int i = 0; i < DATA_PER_FILE; i++) {
            inode->data_ptr[i] = large_block + i * LOGIC_BLOCK_SIZE;
            if (newfs_driver_read(DATA_OFFSET(inode->data_block[i]), inode->data_ptr[i], 
                                LOGIC_BLOCK_SIZE) != ERROR_NONE) {
                NFS_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
        }
    }
    return inode;
}

/**
 * @brief 将dentry从inode的dentrys中取出.inode是父的节点，传入的dentry是inode下属的目录项
 * 
 * 
 * 
 */
int wztfs_drop_dentry(struct newfs_inode * inode , struct newfs_dentry * dentry){
    boolean is_find = FALSE;
    struct newfs_dentry* dentry_cursor = inode->dentrys;

    if (dentry_cursor == dentry) {
        inode->dentrys = dentry->brother;  // 如果上来第一个就是，直接可以删除掉
        is_find = TRUE;
    } else {
        while (dentry_cursor) {
            if (dentry_cursor->brother == dentry) {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find) {
        return ERROR_NOTFOUND;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}

/**
 * @brief 删除内存中的一个inode。对于目录需要递归删除其下的所有子目录项以及对应inode。
 * 需要管理inode位图。对于文件，还需要管理数据块位图。
 * 
 * inode和dentry的顺序是，先删除inode，外层负责删除对应的dentry，通过drop_dentry函数
 * 
 * @param inode 要删除的inode
 */
int wztfs_drop_inode(struct newfs_inode * inode) {
    struct newfs_dentry* dentry_cursor;
    struct newfs_dentry* dentry_to_free;
    struct newfs_inode* inode_cursor;

    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find = FALSE;

    // inode不能是根节点，先进行检查
    if (inode == super.root_dentry->inode) {
        return ERROR_INVAL;
    }

    // 如果是目录，需要递归删除其下的所有子目录项
    if(INODE_IS_DIR(inode)) {
        dentry_cursor = inode->dentrys;
        while (dentry_cursor) {
            inode_cursor = dentry_cursor->inode;
            wztfs_drop_inode(inode_cursor);
            wztfs_drop_dentry(inode, dentry_cursor);
            dentry_to_free = dentry_cursor;
            dentry_cursor = dentry_cursor->brother;
            free(dentry_to_free);
        
        }
        // 管理inode位图。把inode对应的位图位置0
        for(byte_cursor = 0; byte_cursor < BLOCKS_SIZE(1); byte_cursor++) {
            for(bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == inode->ino) {
                    super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                    is_find = TRUE;
                    break;
                }
                ino_cursor++;
            }
            if(is_find) {
                break;
            }
        }

    } else if (INODE_IS_REG(inode)) {
        // 如果是文件，需要管理inode下面的数据块的数据块位图全部删一遍
        // 通过inode->block_used来判断有多少个数据块，对于inode->data_block[0]到inode->data_block[block_used-1]的数据块,数据块位图置0
        for (int i = 0; i < inode->block_used; i++) {
            byte_cursor = inode->data_block[i] / UINT8_BITS;
            bit_cursor  = inode->data_block[i] % UINT8_BITS;
            super.map_data[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
        }
        // 管理inode位图。把inode对应的位图位置0
        for(byte_cursor = 0; byte_cursor < BLOCKS_SIZE(1); byte_cursor++) {
            for(bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == inode->ino) {
                    super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                    is_find = TRUE;
                    break;
                }
                ino_cursor++;
            }
            if(is_find) {
                break;
            }
        }
        // 释放内存中数据块
        if (inode->data_ptr[0])
            free (inode->data_ptr[0]);
        free(inode);
    }
    return ERROR_NONE;
}


/**
 * @brief 指定inode，获取dentrys中的第dir个dentry
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct newfs_dentry* 
 */
struct newfs_dentry* newfs_get_dentry(struct newfs_inode * inode, int dir) {
    struct newfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
} //definitely no problem

/**
 * @brief 找到路径所对应的目录项，或者返回上一级目录项
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 * 
 * @param path 
 * @return struct newfs_inode* 
 */
struct newfs_dentry* newfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct newfs_dentry* dentry_cursor = super.root_dentry;
    struct newfs_dentry* dentry_ret = NULL;
    struct newfs_inode*  inode; 
    int   total_lvl = newfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    /* 如果路径的级数为0，则说明是根目录*/
    if (total_lvl == 0) {                           
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = super.root_dentry;
    }

    /* 获取最外层文件夹名称 */
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;

        /* Cache机制，如果当前dentry对应的inode为空，则从磁盘中读取 */
        if (dentry_cursor->inode == NULL) {           
            newfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        /* 当前dentry对应的inode */
        inode = dentry_cursor->inode;

        /* 若当前inode对应文件类型，且还没查找到对应层数，说明路径错误，跳出循环 */
        if (INODE_IS_REG(inode) && lvl < total_lvl) {
            NFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        /* 若当前inode对应文件夹 */
        if (INODE_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            
            while (dentry_cursor) /* 遍历子目录项 */
            {
                /* 如果名称匹配，则命中 */
                if (memcmp(dentry_cursor->name, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                // 否则查找下一个目录项
                dentry_cursor = dentry_cursor->brother;
            }
            
            /* 若未命中 */
            if (!is_hit) {
                *is_find = FALSE;
                NFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            /* 若命中 */
            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        /* 获取下一层文件夹名称 */
        fname = strtok(NULL, "/"); 
    }

    /* 如果对应dentry的inode还没读进来，则重新读 */
    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = newfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}

/**
 * @brief 挂载newfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data Map | Data |
 * 
 *  BLK_SZ = 2 * IO_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int newfs_mount(struct custom_options options){
    int                 ret = ERROR_NONE;
    int                 driver_fd;
    struct newfs_super_d  super_d;    /* 临时存放 driver 读出的超级块 */
    struct newfs_dentry*  root_dentry;
    struct newfs_inode*   root_inode;

    int                 super_blks;
    int                 inode_num;
    int                 data_num;
    int                 map_inode_blks;
    int                 map_data_blks;
    
    boolean             is_init = FALSE;

    super.is_mounted = FALSE;

    driver_fd = ddriver_open(newfs_options.device);

    if (driver_fd < 0) {
        return driver_fd;
    }

    super.fd = driver_fd;
    ddriver_ioctl(DRIVER_FD(), IOC_REQ_DEVICE_SIZE,  &super.disk_size);  /* 请求查看设备大小 4MB */
    ddriver_ioctl(DRIVER_FD(), IOC_REQ_DEVICE_IO_SZ, &super.sz_io);    /* 请求设备IO大小 512B */
    super.block_size = LOGIC_BLOCK_SIZE;          /* EXT2文件系统实际块大小 1024B,是io块乘以2 */

    

    /* 读取 super 到临时空间 */
    if (newfs_driver_read(SUPER_OFFSET, (uint8_t *)(&super_d), 
                        sizeof(struct newfs_super_d)) != ERROR_NONE) {
        return -ERROR_IO;
    }   
                                                    
    if (super_d.magic != MAGIC_NUM) {     /* 幻数无，重建整个磁盘 */
                                                      /* 估算各部分大小 */
        printf("认为是首次挂载");
        /* 规定各部分大小 */
        super_blks      = SUPER_BLKS;
        map_inode_blks  = MAP_INODE_BLKS;
        map_data_blks   = MAP_DATA_BLKS;
        inode_num       = INODE_BLKS;
        data_num        = DATA_BLKS;

                                                      /* 布局layout */
       

        super_d.magic = MAGIC_NUM;              // 设置幻数

        /*设置后面四个部分的偏移量*/
        super_d.map_inode_offset = SUPER_OFFSET + BLOCKS_SIZE(super_blks);
        super_d.map_data_offset  = super_d.map_inode_offset + BLOCKS_SIZE(map_inode_blks);
        super_d.inode_offset = super_d.map_data_offset + BLOCKS_SIZE(map_data_blks);
        super_d.data_offset  = super_d.inode_offset + BLOCKS_SIZE(inode_num);

        /*设置后面四个部分的数目*/
        super_d.map_inode_blks  = map_inode_blks;
        super_d.map_data_blks   = map_data_blks;
        super.inode_nums = inode_num;
        super.max_data = data_num;                  // max_data字段通过初始化的data_num传递过来了，其实就是DATA_BLKS

        super_d.sz_usage        = 0;

        is_init = TRUE;
    }
    // 初始化内存里边的超级块，无论是不是第一次都要做。前面是磁盘内的超级快，只有第一次初始化才要做。
    super.sz_usage   = super_d.sz_usage;      /* 建立 in-memory 结构 */
    
    super.map_inode = (uint8_t *)malloc(BLOCKS_SIZE(super_d.map_inode_blks));
    super.map_inode_blks = super_d.map_inode_blks;
    super.map_inode_offset = super_d.map_inode_offset;

    super.map_data = (uint8_t *)malloc(BLOCKS_SIZE(super_d.map_data_blks));
    super.map_data_blks = super_d.map_data_blks;
    super.map_data_offset = super_d.map_data_offset;

    super.inode_offset = super_d.inode_offset;
    super.data_offset = super_d.data_offset;

    /* 读取索引位图和数据位图到内存空间 */
    if (newfs_driver_read(super_d.map_inode_offset, (uint8_t *)(super.map_inode), 
                        BLOCKS_SIZE(super_d.map_inode_blks)) != ERROR_NONE) {
        return -ERROR_IO;
    }
    if (newfs_driver_read(super_d.map_data_offset, (uint8_t *)(super.map_data), 
                        BLOCKS_SIZE(super_d.map_data_blks)) != ERROR_NONE) {
        return -ERROR_IO;
    }

    root_dentry = new_dentry("/", WZT_DIR); // 创建根目录
    if (is_init) {                                    /* 如果进行了重建，则分配根节点 */
        root_inode = newfs_alloc_inode(root_dentry);    
        newfs_sync_inode(root_inode);  /* 将重建后的根inode 写回磁盘 */
    }
    
    /* 如果磁盘有数据，则先读入根结点，其他暂时不读 (Cache) */
    root_inode            = newfs_read_inode(root_dentry, ROOT_INO);  //newfs_read_inode读取的inode是看第二个参数，传第0个正确
    root_dentry->inode    = root_inode;
    super.root_dentry = root_dentry;
    super.is_mounted  = TRUE;

    return ret;
}

/**
 * @brief 
 * 
 * @return int 
 */
int newfs_umount() {
    struct newfs_super_d  super_d; 

    if (!super.is_mounted) { // 检查超级块，没有挂载就报错
        return ERROR_NONE;
    }

    newfs_sync_inode(super.root_dentry->inode);     /* 从根节点向下递归将节点写回 */
                                                    
    super_d.magic               = MAGIC_NUM;
    super_d.sz_usage            = super.sz_usage;

    super_d.map_inode_blks      = super.map_inode_blks;
    super_d.map_inode_offset    = super.map_inode_offset;
    super_d.map_data_blks       = super.map_data_blks;
    super_d.map_data_offset     = super.map_data_offset;

    super_d.inode_offset        = super.inode_offset;
    super_d.data_offset         = super.data_offset;
    

    if (newfs_driver_write(SUPER_OFFSET, (uint8_t *)&super_d, 
                     sizeof(struct newfs_super_d)) != ERROR_NONE) {
        return -ERROR_IO;
    }

    if (newfs_driver_write(super_d.map_inode_offset, (uint8_t *)(super.map_inode), 
                         BLOCKS_SIZE(super_d.map_inode_blks)) != ERROR_NONE) {
        return -ERROR_IO;
    }

    if (newfs_driver_write(super_d.map_data_offset, (uint8_t *)(super.map_data), 
                         BLOCKS_SIZE(super_d.map_data_blks)) != ERROR_NONE) {
        return -ERROR_IO;
    }

    free(super.map_inode);
    free(super.map_data);
    ddriver_close(DRIVER_FD());

    return ERROR_NONE;
}