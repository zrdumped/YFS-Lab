#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  /*
   *your lab1 code goes here.
   *if id is smaller than 0 or larger than BLOCK_NUM
   *or buf is null, just return.
   *put the content of target block into buf.
   *hint: use memcpy
  */
	if(id < 0 || id > BLOCK_NUM-1 || buf == NULL) return;
	memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  /*
   *your lab1 code goes here.
   *hint: just like read_block
  */
	if(id < 0 || id > BLOCK_NUM-1) return;
	memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.

   *hint: use macro IBLOCK and BBLOCK.
          use bit operation.
          remind yourself of the layout of disk.
   */
  blockid_t block_num =  BPB;
  char buf[BLOCK_SIZE];

  while(block_num < sb.nblocks){
    read_block(BBLOCK(block_num), buf);
    for(int i = 0; i < BLOCK_SIZE && block_num < sb.nblocks; i++){

      for(int j = 0; j < 8; j++){
        if((buf[i] & (0x01 << (7 - j))) == 0){
          ////printf("buf[i]=%d", (unsigned)buf[i]);
          buf[i] |= (0x01 << (7 - j));
          ////printf("buf[i]=%d", (unsigned)buf[i]);
          write_block(BBLOCK(block_num), buf);
          //printf("******alloc block %d i=%d j=%d ********\n", block_num + i * 8 + j, i, j);
          return block_num + i * 8 + j;
        }
      }
    }
    block_num += BPB;
  }

  printf("\tim: error! no enough free block\n");
  exit(0);
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /*
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  char buf[BLOCK_SIZE];
  read_block(BBLOCK(id), buf);
  unsigned char mask = (1 << (7 - ((id % BPB) & 0x7))) ^ 0xFF;
  buf[(id % BPB) / 8] &= mask;
  write_block(BBLOCK(id), buf);
  //printf("free block %d", id);
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
 uint32_t
 inode_manager::alloc_inode(uint32_t type)
 {
   /*
    * your lab1 code goes here.
    * note: the normal inode block should begin from the 2nd inode block.
    * the 1st is used for root_dir, see inode_manager::inode_manager().

    * if you get some heap memory, do not forget to free it.
    */
   uint32_t inode_num = 1;
   inode_t* inode_tmp = (inode_t*)malloc(sizeof(inode_t));
   inode_tmp->type = type;
   inode_tmp->size = 0;
   inode_tmp->atime = time(0);
   inode_tmp->mtime = time(0);
   inode_tmp->ctime = time(0);

   while(inode_num < bm->sb.ninodes){
     //find a free inode in bitmap for free blocks
     if(get_inode(inode_num)== NULL){
       put_inode(inode_num, inode_tmp);
       //printf("1b4free");
       free(inode_tmp);
       return inode_num;
     }
       inode_num ++;
   }
   //printf("2b4free");
   free(inode_tmp);
   printf("\tim: error! no free space for new inode\n");
   return 0;
 }

 void
 inode_manager::free_inode(uint32_t inum)
 {
   /*
    * your lab1 code goes here.
    * note: you need to check if the inode is already a freed one;
    * if not, clear it, and remember to write back to disk.
    * do not forget to free memory if necessary.
    */
    inode_t* inode_tmp = get_inode(inum);
    if(inode_tmp == NULL) {
     printf("\tim: error! inode %d has been free\n", inum);
     return;
    }
     inode_tmp->type = 0;
     put_inode(inum, inode_tmp);
     //printf("3b4free");
     free(inode_tmp);
     return;
 }


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode*
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum.
 * Return alloced data, should be freed by caller. */
 void
 inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
 {
   //printf("read file\n");
   /*
    * your lab1 code goes here.
    * note: read blocks related to inode number inum,
    * and copy them to buf_out
    */
   inode_t* inode = get_inode(inum);
    if(inode == NULL) {
     printf("\tim: error! inode %d does not exist\n", inum);
     return;
    }
   int total_size = inode->size;
   char* buf = (char*)malloc(inode->size);

   unsigned int blocks_num = total_size / BLOCK_SIZE;
   //printf("%d\n",total_size);
   //printf("%d\n",blocks_num);
   char blocks_2[BLOCK_SIZE];
   bm->read_block(inode->blocks[NDIRECT], blocks_2);
   unsigned int i = 0;
   for(; i < NDIRECT && i < blocks_num; i++)
     bm->read_block(inode->blocks[i], buf + BLOCK_SIZE * i);
   if(i < blocks_num){
     for(; i <= NINDIRECT + NDIRECT && i < blocks_num; i++)
       bm->read_block(*((blockid_t*)blocks_2 + (i - NDIRECT)), buf + BLOCK_SIZE * i);
   }
   //printf("%d\n", i);
   char tail [BLOCK_SIZE];
   if(i < NDIRECT - 1){
     //printf("%d\n", i);
     bm->read_block(inode->blocks[i], tail);
   }
   else{
     bm->read_block(*((blockid_t*)blocks_2 + i - NDIRECT), tail);
   }
   memcpy(buf + BLOCK_SIZE * i, tail, total_size % BLOCK_SIZE);

   *buf_out = buf;
   *size = inode->size;
   ////printf("%s\n", buf);
   inode->atime = time(0);
   inode->ctime = time(0);
   put_inode(inum, inode);
   //printf("4b4free");
   free(inode);
   return;
 }

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */
  struct inode *ino = get_inode(inum);
  if(ino == NULL){
    printf("write_file: no such inode %d\n", inum);
    return;
  }
  unsigned int oldBlockNum = ino->size / BLOCK_SIZE;
  oldBlockNum += (ino->size % BLOCK_SIZE != 0)? 1 : 0;
  unsigned int newBlockNum = size / BLOCK_SIZE;
  newBlockNum += (size % BLOCK_SIZE != 0)? 1 : 0;
  if(newBlockNum > MAXFILE){
    printf("write_file: no space \n");
    return;
  }
  //alloc
  if(newBlockNum > oldBlockNum){
    if(newBlockNum <= NDIRECT){
      for(unsigned int i = oldBlockNum; i < newBlockNum; i++){
        ino->blocks[i] = bm->alloc_block();
      }
    }
    else if(oldBlockNum > NDIRECT){
      blockid_t extraBlocks[NINDIRECT];
      bm->read_block(ino->blocks[NDIRECT], (char *)extraBlocks);
      for(unsigned int i = oldBlockNum; i < newBlockNum; i++){
        extraBlocks[i - NDIRECT] = bm->alloc_block();
      }
      bm->write_block(ino->blocks[NDIRECT], (char *)extraBlocks);
    }
    else{
      for(unsigned int i = oldBlockNum; i < NDIRECT; i++){
        ino->blocks[i] = bm->alloc_block();
      }
      ino->blocks[NDIRECT] = bm->alloc_block();
      blockid_t extraBlocks[NINDIRECT];
      for(unsigned int i = NDIRECT ; i < newBlockNum; i++){
        extraBlocks[i - NDIRECT] = bm->alloc_block();
      }
      bm->write_block(ino->blocks[NDIRECT], (char *)extraBlocks);
    }
  }
  //write
  
  char block[BLOCK_SIZE];
  char indirect[BLOCK_SIZE];
  int cur = 0;
  for (int i = 0; i < NDIRECT && cur < size; ++i) {
    if (size - cur > BLOCK_SIZE) {
      bm->write_block(ino->blocks[i], buf + cur);
      cur += BLOCK_SIZE;
    } else {
      int len = size - cur;
      memcpy(block, buf + cur, len);
      bm->write_block(ino->blocks[i], block);
      cur += len;
    }
  }

  if (cur < size) {
    bm->read_block(ino->blocks[NDIRECT], indirect);
    for (unsigned int i = 0; i < NINDIRECT && cur < size; ++i) {
      blockid_t ix = *((blockid_t *)indirect + i);
      if (size - cur > BLOCK_SIZE) {
        bm->write_block(ix, buf + cur);
        cur += BLOCK_SIZE;
      } else {
        int len = size - cur;
        memcpy(block, buf + cur, len);
        bm->write_block(ix, block);
        cur += len;
      }
    }
  }
  //free
  if(newBlockNum < oldBlockNum){
    if(oldBlockNum < NDIRECT){
      for(unsigned int i = newBlockNum; i < oldBlockNum; i++){
        bm->free_block(ino->blocks[i]);
      }
    }
    else if(newBlockNum > NDIRECT){
      blockid_t extraBlocks[NINDIRECT];
      bm->read_block(ino->blocks[NDIRECT], (char *)extraBlocks);
      for(unsigned int i = newBlockNum ; i < oldBlockNum; i++){
        bm->free_block(extraBlocks[i - NDIRECT]);
      }
    }
    else{
      for(unsigned int i = newBlockNum; i < NDIRECT; i++){
        bm->free_block(ino->blocks[i]);
      }
      blockid_t extraBlocks[NINDIRECT];
      bm->read_block(ino->blocks[NDIRECT], (char *)extraBlocks);
      for(unsigned int i = newBlockNum ; i < oldBlockNum; i++){
        bm->free_block(extraBlocks[i - NDIRECT]);
      }
    }
  }
  ino->size = size;
  ino->atime = time(0);
  ino->mtime = time(0);
  ino->ctime = time(0);
  put_inode(inum, ino);
  free(ino);
  printf("write_file: succeed %d\n", inum);
}


void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
   inode_t* inode_tmp = get_inode(inum);
    if(inode_tmp == NULL) {
    printf("\tim: error! inode %d does not exist\n", inum);
    return;
   }
   a.type = inode_tmp->type;
   a.atime = inode_tmp->atime;
   a.mtime = inode_tmp->mtime;
   a.ctime = inode_tmp->ctime;
   a.size = inode_tmp->size;
   //printf("6b4free");
   free(inode_tmp);
   return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  printf("\tremove file\n");
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
  inode_t* inode = get_inode(inum);
  if(inode == NULL) {
    printf("\tim: error! inode %d does not exist\n", inum);
    return;
  }
  unsigned int blocks_num = inode->size / BLOCK_SIZE + !!(inode->size % BLOCK_SIZE);

  if(blocks_num < NDIRECT){
    for(unsigned int i = 0 ; i < blocks_num; i++){
      bm->free_block(inode->blocks[i]);
    }
  }
  else{
    char tail[BLOCK_SIZE];
    bm->read_block(inode->blocks[NDIRECT], tail);
    for(unsigned int i = NDIRECT; i < blocks_num && i < NINDIRECT + NDIRECT; i++){
      bm->free_block(*((blockid_t*)tail + (i - NDIRECT)));
    }
    for(unsigned int i = 0 ; i <= NDIRECT; i++){
      bm->free_block(inode->blocks[i]);
    }
  }
  free_inode(inum);
  free(inode);
  return;
}
