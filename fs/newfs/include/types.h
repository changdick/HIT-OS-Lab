#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128     

#define LOGIC_BLOCK_SIZE 1024
#define IO_BLOCK_SIZE    512
#define DISK_SIZE        1024*1024*4
#define DATA_PER_FILE 12   // 每个文件数据块数

#define DRIVER_FD()       (super.fd)     // 暂时放着

typedef int          boolean;
#define TRUE                    1
#define FALSE                   0


#define SUPER_OFFSET           0
#define SUPER_BLKS             1
#define MAP_INODE_BLKS         1
#define MAP_DATA_BLKS          1
#define INODE_BLKS            315 // 暂时放着
#define DATA_BLKS             3778      //2024/12/7 补上宏定义 

// 定义MAGIC
#define MAGIC_NUM              0x12345678


#define UINT8_BITS              8

#define ERROR_NOSPACE       ENOSPC
#define ERROR_NONE          0
#define ERROR_ACCESS        EACCES
#define ERROR_SEEK          ESPIPE     
#define ERROR_ISDIR         EISDIR
#define ERROR_NOSPACE       ENOSPC
#define ERROR_EXISTS        EEXIST
#define ERROR_NOTFOUND      ENOENT
#define ERROR_UNSUPPORTED   ENXIO
#define ERROR_IO            EIO     /* Error Input/Output */
#define ERROR_INVAL         EINVAL  /* Invalid Args */

//

#define ROOT_INO            0

typedef enum file_type {
    WZT_FILE,           // 普通文件,不能叫FILE，否则和标准库撞车
    WZT_DIR             // 目录文件
} FILE_TYPE;


struct custom_options {
	const char*        device;
};

struct newfs_super {
    uint32_t magic;
    int      fd;
    /* TODO: Define yourself */
    // 在这里自己定义至少五个部分的信息
    
    //io
    uint32_t sz_io;          // io大小

    // 逻辑块大小和块数
    uint32_t block_size;     // 块大小
    uint32_t block_nums;     // 块数

    // 磁盘大小
    uint32_t disk_size;
    int                 sz_usage;           /* ioctl 相关信息 */
   
    // 索引节点位图起始块号
    uint32_t inode_bitmap_block;
    uint8_t*            map_inode;           // 指向 inode 位图的内存起点 
    // inode位图块数
    uint32_t map_inode_blks;
    // inode位图偏移
    uint32_t map_inode_offset;

    // 数据块位图起始块号
    uint32_t data_bitmap_block;
    uint8_t*            map_data;           // 指向 data 位图的内存起点 
    // 数据块位图块数
    uint32_t map_data_blks;
    // 数据块位图偏移
    uint32_t map_data_offset;

    // 索引节点起始offset
    uint32_t inode_offset;
    // inode信息
    uint32_t inode_size;     // inode大小
    uint32_t inode_nums;     // inode数(最大支持的inode数量)
    
    // 数据块起始块号
    uint32_t data_offset;
    uint32_t data_block;
    //数据块最大数目
    uint32_t max_data;

   

    // 根目录索引
    uint32_t root_inode;

    struct newfs_dentry* root_dentry;


    // 是否挂
    boolean is_mounted;
};
struct newfs_super_d {
    uint32_t         magic;
    uint32_t         sz_usage;

    uint32_t         map_inode_blks;     /* inode 位图占用的块数 */
    uint32_t         map_inode_offset;   /* inode 位图在磁盘上的偏移 */

    uint32_t         map_data_blks;      /* data 位图占用的块数 */
    uint32_t         map_data_offset;    /* data 位图在磁盘上的偏移 */

    uint32_t         inode_offset;       /* 索引结点的偏移 */
    uint32_t         data_offset;        /* 数据块的偏移*/
};

struct newfs_inode {
    uint32_t ino;
    /* TODO: Define yourself */
    // 文件大小
    uint32_t size;
    // 连接数
    uint32_t link;
    // 文件类型
    FILE_TYPE type;
    int                  dir_cnt;  // 维护目录项数量
    struct newfs_dentry* dentry;                        /* 指向该inode的dentry */
    struct newfs_dentry* dentrys;                       /* 所有目录项 */

    // 数据块索引
    uint32_t data_block[DATA_PER_FILE];     // 数据块索引，就是一个整数块号 
    uint8_t* data_ptr[DATA_PER_FILE];           // 指向数据块的内存起点   

    // 已经使用的块数
    int block_used;
};

// inode的大小是128B，所以一个块可以放8个inode
struct newfs_inode_d {
    uint32_t           ino;                             // 索引编号
    uint32_t           size;                            // 文件已占用空间
    uint32_t           link;                            // 链接数，默认为1
    FILE_TYPE          type;                            // 文件类型（目录类型、普通文件类型）
    int                data_block[DATA_PER_FILE];       // 数据块指针（可固定分配）
    int                dir_cnt;                         // 如果是目录类型文件，下面有几个目录项
    int                block_used;                      // 已经使用的块数


    // 前面18个int， 72字节
    // 凑成可以让1024整除的字节数，凑到128B，所以还差56字节
    char               padding[56];                // 填充字节                           
};

struct newfs_dentry {
    char     name[MAX_NAME_LEN];
    uint32_t ino;
    /* TODO: Define yourself */
    
    struct newfs_dentry* brother;
    struct newfs_dentry* parent;

    struct newfs_inode*  inode;                         /* 指向inode */

    // 文件类型
    FILE_TYPE type;
};
struct newfs_dentry_d {
    char            name[MAX_NAME_LEN];                 // 文件名
    uint32_t        ino;                                // 指向的ino号
    FILE_TYPE       type;                              // 文件类型
};






/*
    宏定义一些函数
*/
// 求偏移量下界
#define OFFSET_ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
// 求偏移量上界
#define OFFSET_ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))
// 块数求字节数
#define BLOCKS_SIZE(blks)               ((blks) * LOGIC_BLOCK_SIZE)

// 计算偏移
#define INODE_OFFSET(ino)                (super.inode_offset + (ino) * sizeof(struct newfs_inode_d)) // 这个能用是说一个索引就占一个块
#define DATA_OFFSET(bno)               (super.data_offset + BLOCKS_SIZE(bno))
// 判断inode类型
#define INODE_IS_DIR(pinode)              (pinode->dentry->type == WZT_DIR)
#define INODE_IS_REG(pinode)              (pinode->dentry->type == WZT_FILE)

#define DENTRY_PER_BLK()            ((BLK_SZ()) / sizeof(struct newfs_dentry_d)) // 这边问题：要用dentry还是dentry_d?



#define BLK_SZ()                    (super.block_size)  

// 复制文件名到某个dentry中
#define WZT_ASSIGN_FNAME(pjfs_dentry, _fname) memcpy(pjfs_dentry->name, _fname, strlen(_fname))
#endif /* _TYPES_H_ */