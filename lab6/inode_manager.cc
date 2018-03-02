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
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

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
  sb.version = 0;
  sb.next_inode = 1;
  sb.end_inode = 1;

  char buf[BLOCK_SIZE];
  bzero(buf, sizeof(buf));
  memcpy(buf, &sb, sizeof(sb));
  write_block(1, buf);

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
  //modify super block
  //there will not be real free so the inum will increase forever
   uint32_t inode_num = bm->sb.next_inode++;
   uint32_t pos = bm->sb.end_inode++;
   char temp_sb[BLOCK_SIZE];
   bzero(temp_sb, BLOCK_SIZE);
   memcpy(temp_sb, &bm->sb, sizeof(bm->sb));
   bm->write_block(1, temp_sb);

   //allocate the new inode
   char buf[BLOCK_SIZE];
   bm->read_block(IBLOCK(pos, bm->sb.nblocks), buf);
   inode_t* inode_tmp = (inode_t*)buf;
   inode_tmp->commit = -1;
   inode_tmp->inum = inode_num;
   inode_tmp->pos = pos;
   inode_tmp->type = type;
   inode_tmp->size = 0;
   inode_tmp->atime = time(0);
   inode_tmp->mtime = time(0);
   inode_tmp->ctime = time(0);
   bm->write_block(IBLOCK(pos, bm->sb.nblocks), buf);

   //printf("2b4free");
   //free(inode_tmp);
   //printf("\tim: error! no free space for new inode\n");
   return inode_num;
 }

 void inode_manager::commit(){
   uint32_t pos = bm->sb.end_inode++;

   char buf[BLOCK_SIZE];
   bm->read_block(IBLOCK(pos, bm->sb.nblocks), buf);
   inode_t * end_inode = (inode_t *)buf;
   end_inode->commit = bm->sb.version++;
   bm->write_block(IBLOCK(pos, bm->sb.nblocks), buf);
   //free(end_inode);

   char temp_sb[BLOCK_SIZE];
   bzero(temp_sb, BLOCK_SIZE);
   memcpy(temp_sb, &bm->sb, sizeof(bm->sb));
   bm->write_block(1, temp_sb);

 }

 void inode_manager::undo(){
   char buf[BLOCK_SIZE];
   bm->sb.version--;
   inode_t* inode_temp;
   while(true){
     bm->read_block(IBLOCK(--bm->sb.end_inode, bm->sb.nblocks), buf);
     inode_temp = (inode_t*)buf;
     if(inode_temp->commit == (short)bm->sb.version){
       bzero(buf, BLOCK_SIZE);
       memcpy(buf, &bm->sb, sizeof(bm->sb));
       bm->write_block(1, buf);
       return;
     }
   }
 }

 void inode_manager::redo(){
   char buf[BLOCK_SIZE];
   bm->sb.version++;
   inode_t* inode_temp;
   while(true){
     bm->read_block(IBLOCK(bm->sb.end_inode, bm->sb.nblocks), buf);
     inode_temp = (inode_t*)buf;
     if(inode_temp->commit == (short)bm->sb.version){
       bzero(buf, BLOCK_SIZE);
       memcpy(buf, &bm->sb, sizeof(bm->sb));
       bm->write_block(1, buf);
       return;
     }
     bm->sb.end_inode++;
   }
 }

 /*void
 inode_manager::free_inode(uint32_t inum)
 {*/
   /*
    * your lab1 code goes here.
    * note: you need to check if the inode is already a freed one;
    * if not, clear it, and remember to write back to disk.
    * do not forget to free memory if necessary.
    */
/*    inode_t* inode_tmp = get_inode(inum);
    if(inode_tmp == NULL) {
     printf("\tim: error! inode %d has been free\n", inum);
     return;
    }
     inode_tmp->type = 0;
     put_inode(inum, inode_tmp);
     //printf("3b4free");
     free(inode_tmp);
     return;
 }*/


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode*
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char old_inode[BLOCK_SIZE], new_inode[BLOCK_SIZE], temp[BLOCK_SIZE];
  char old_indirect[BLOCK_SIZE], new_indirect[BLOCK_SIZE];
  bool old = false;

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  unsigned int pos = bm->sb.end_inode - 1;
  while(pos > 0){
    bm->read_block(IBLOCK(pos, bm->sb.nblocks), old_inode);
    ino = (inode_t*)old_inode;
    //there is not target of the current version
    if(ino->commit != -1){
      old = true;
    }else if(ino->inum == inum){
      //find the target inode
      if(ino->type == 0)
        return NULL;
      break;
    }
    pos--;
  }

  //if the version is old, the replicate it into current version
  if(old){
    //modify the super block
    pos = bm->sb.end_inode++;
    bzero(temp, BLOCK_SIZE);
    memcpy(temp, &bm->sb, sizeof(bm->sb));
    bm->write_block(1, temp);

    bm->read_block(IBLOCK(pos, bm->sb.nblocks), new_inode);
    ino_disk = (inode_t*)new_inode;
    ino_disk->commit = ino->commit; //it must be -1
    ino_disk->type = ino->type;
    ino_disk->inum = ino->inum;
    ino_disk->pos = pos;
    ino_disk->size = ino->size;
    ino_disk->atime = ino->atime;
    ino_disk->mtime = ino->mtime;
    ino_disk->ctime = ino->ctime;

    unsigned int block_num = (ino->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if(block_num > NDIRECT){
      bm->read_block(ino->blocks[NDIRECT], old_indirect);
      for(unsigned int i = NDIRECT; i < block_num; i++){
        bm->read_block(old_indirect[i - NDIRECT], temp);
        new_indirect[i - NDIRECT] = bm->alloc_block();
        bm->write_block(new_indirect[i - NDIRECT], temp);
      }
      ino_disk->blocks[NDIRECT] = bm->alloc_block();
      bm->write_block(ino_disk->blocks[NDIRECT], new_indirect);
      for(unsigned int i = 0; i < NDIRECT; i++){
        bm->read_block(ino->blocks[i], temp);
        ino_disk->blocks[i] = bm->alloc_block();
        bm->write_block(ino_disk->blocks[i], temp);
      }
    }
    else{
      for(unsigned int i = 0; i < block_num; i++){
        bm->read_block(ino->blocks[i], temp);
        ino_disk->blocks[i] = bm->alloc_block();
        bm->write_block(ino_disk->blocks[i], temp);
      }
    }
    bm->write_block(IBLOCK(pos, bm->sb.nblocks), new_inode);
    ino = ino_disk;
  }

  struct inode* res ;
  res = (struct inode*)malloc(sizeof(*res));
  *res = *ino;
  return res;
  /*inode_t res;
  *res = *ino;
  return ino;
  */
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(ino->pos, bm->sb.nblocks), buf);
  //ino_disk = (struct inode*)buf + inum%IPB;
  ino_disk = (struct inode*)buf;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(ino->pos, bm->sb.nblocks), buf);
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
  //printf("write file\n");
  if(size < 0 || (uint32_t)size > BLOCK_SIZE * MAXFILE){
    printf("\tim: error! size invalid\n");
    return;
  }
  inode_t* inode = get_inode(inum);
   if(inode == NULL) {
    printf("\tim: error! inode %d does not exist\n", inum);
    return;
   }
  unsigned int old_blocks_num = inode->size / BLOCK_SIZE + !!(inode->size % BLOCK_SIZE);
  unsigned int new_blocks_num = size / BLOCK_SIZE + !!(size % BLOCK_SIZE);
  //char block[BLOCK_SIZE];
  char tail[BLOCK_SIZE];


  if(old_blocks_num > new_blocks_num){
    if(new_blocks_num > NDIRECT){
      //printf("1free");
      bm->read_block(inode->blocks[NDIRECT], tail);
      for(unsigned int i = new_blocks_num; i < old_blocks_num; i++){
        bm->free_block(*((blockid_t*)tail + (i - NDIRECT)));
      }
    }
    else if(old_blocks_num <= NDIRECT){
      //printf("2free");
      for(unsigned int i = new_blocks_num; i < old_blocks_num; i++){
        bm->free_block(inode->blocks[i]);
      }
    }
    else{
      //printf("3free");
      bm->read_block(inode->blocks[NDIRECT], tail);
      for(unsigned int i = NDIRECT; i < old_blocks_num; i++){
        bm->free_block(*((blockid_t*)tail + (i - NDIRECT)));
      }
      for(unsigned int i = new_blocks_num; i <= NDIRECT; i++){
        bm->free_block(inode->blocks[i]);
      }
    }
  }

  else if(new_blocks_num > old_blocks_num){
    if(new_blocks_num <= NDIRECT){
      //printf("1alloc");
      for(unsigned int i = old_blocks_num; i < new_blocks_num; i++){
        inode->blocks[i]=bm->alloc_block();
      }
    }
    else if(old_blocks_num > NDIRECT){
      //printf("2alloc");
      bm->read_block(inode->blocks[NDIRECT], tail);
      for(unsigned int i = old_blocks_num; i < new_blocks_num; i++){
        *((blockid_t*)tail + (i - NDIRECT)) = bm->alloc_block();
      }
      bm->write_block(inode->blocks[NDIRECT], tail);
    }
    else{
      //printf("3alloc");
      for(unsigned int i = old_blocks_num; i <= NDIRECT; i++){
        inode->blocks[i] = bm->alloc_block();
      }
      //printf("4alloc");
      bzero(tail, BLOCK_SIZE);
      for(unsigned int i = NDIRECT; i < new_blocks_num; i++){
        *((blockid_t*)tail + (i - NDIRECT)) = bm->alloc_block();
      }
      bm->write_block(inode->blocks[NDIRECT], tail);
    }
  }
  //printf("5alloc\n");
  unsigned int new_blocks_num_no_tail = size / BLOCK_SIZE;
  unsigned int i = 0;
  for(; i < NDIRECT && i < new_blocks_num_no_tail; i++)
    bm->write_block(inode->blocks[i], buf + BLOCK_SIZE * i);
  //printf("6alloc\n");
  if(i == NDIRECT && i < new_blocks_num_no_tail){
    bm->read_block(inode->blocks[NDIRECT], tail);
    for(; i <= (NINDIRECT + NDIRECT)  && i < new_blocks_num_no_tail; i++)
      bm->write_block(*((blockid_t*)tail + (i - NDIRECT)), buf + BLOCK_SIZE * i);
  }
  //printf("7alloc\n");

  int tail_len = size % BLOCK_SIZE;
  char buffer[BLOCK_SIZE];
  bzero(buffer, BLOCK_SIZE);
  if(new_blocks_num == 0){
    //printf("7.2alloc\n");
    bm->write_block(inode->blocks[0], buffer);
    //printf("7.5alloc\n");
  }
  else if(tail_len != 0 ){
    //printf("7.6alloc\n");
    memcpy(buffer, buf + (new_blocks_num - 1) * BLOCK_SIZE, tail_len);
    if(i < NDIRECT){
      //printf("7.7alloc\n");
      bm->write_block(inode->blocks[i], buffer);
      //bm->write_block(inode->blocks[i], buf + (new_blocks_num - 1) * BLOCK_SIZE);
    }
    else{
      //printf("7.75alloc\n");
      bm->write_block(*((blockid_t*)tail + (i - NDIRECT)), buffer);
      //bm->write_block(*((blockid_t*)tail + (i - NDIRECT)), buf + (new_blocks_num - 1) * BLOCK_SIZE);
    }
    //printf("7.8alloc\n");
  }
  //printf("8alloc\n");
  //printf("%s", buf);
  inode->size = size;
  inode->mtime = time(0);
  inode->ctime = time(0);
  //if(inode == NULL) printf("error1\n");
  put_inode(inum, inode);
  //printf("5b4free\n");
  //if(inode != NULL) printf("error2\n");
  free(inode);
  //printf("9alloc\n");
  return;
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
  unsigned int pos = bm->sb.end_inode++;
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
   char buf[BLOCK_SIZE];
   bm->read_block(IBLOCK(pos, bm->sb.nblocks), buf);
  inode_t* inode = (inode_t*)buf;
  inode->commit = -1;
  inode->type = 0;
  inode->inum = inum;
  bm->write_block(IBLOCK(pos, bm->sb.nblocks), buf);

  bzero(buf, sizeof(buf));
  memcpy(buf, &bm->sb, sizeof(bm->sb));
  bm->write_block(1, buf);
  /*
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
  }*/
  //free_inode(inum);
  //free(inode);
  return;
}
