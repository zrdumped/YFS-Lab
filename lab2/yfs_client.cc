// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }
    else if(a.type == extent_protocol::T_DIR){
        printf("isfile: %lld is a dir\n", inum);
        return false;
    }
    else{
        printf("isfile: %lld is a sym\n", inum);
        return false;      
    }
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isdir: %lld is a file\n", inum);
        return false;
    }
    else if(a.type == extent_protocol::T_DIR){
        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    else{
        printf("isdir: %lld is a sym\n", inum);
        return false;      
    }
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}

int yfs_client::get_symlink(inum inum, syminfo& sin){
    int r = OK;

    printf("get_symlink %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    sin.atime = a.atime;
    sin.mtime = a.mtime;
    sin.ctime = a.ctime;
    sin.size = a.size;
    printf("get_symlink %016llx -> sz %llu\n", inum, sin.size);

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
     std::string buf;
     ec->get(ino, buf);
     buf.resize(size);
     ec->put(ino, buf);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
     //check the parent
     //printf("\tcreate begins\n");

     if(!isdir(parent)){
       printf("\tcreate: error! parent is not a directory\n");
       return IOERR;
     }
     //check new file
     bool exist;
     lookup(parent, name, exist, ino_out);
     if(exist){
       printf("\tcreate: error! file exists\n");
       return EXIST;
     }
     //get parent dir
     std::string buf;
     if(ec->get(parent, buf) != OK){
       printf("\tcreate: error! parent invalid\n");
       return IOERR;
     }
     //add new file
     if(ec->create(extent_protocol::T_FILE, ino_out) != OK){
       printf("\tcreate: error! create fails\n");
       return IOERR;
     }
     //write back
     //directory struct : inum  / filename / ... / 0 /
     buf = buf.substr(0,buf.length() - 2);
     buf += filename(ino_out) + "/" + std::string(name) + "/0/" ;
     ec->put(parent, buf);
     return OK;
}


int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
  int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
     //check the parent
     //printf("\tmkdir begins\n");

     if(!isdir(parent)){
       printf("\tmkdir: error! parent is not a directory\n");
       return IOERR;
     }
     //check new file
     bool exist;
     lookup(parent, name, exist, ino_out);
     if(exist){
       printf("\tmkdir: error! file exists\n");
       return EXIST;
     }
     //get parent dir
     std::string buf;
     if(ec->get(parent, buf) != OK){
       printf("\tmkdir: error! parent invalid\n");
       return IOERR;
     }
     //add new file
     if(ec->create(extent_protocol::T_DIR, ino_out) != OK){
       printf("\tcreate: error! create fails\n");
       return IOERR;
     }
     //write back
     //directory struct : inum  / filename / ... / 0 /
     buf = buf.substr(0,buf.length() - 2);
     buf += filename(ino_out) + "/" + std::string(name) + "/0/" ;
     ec->put(parent, buf);
     return OK;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
     std::list<dirent> dir_list;
   	 r = readdir(parent, dir_list);
   	 std::list<dirent>::iterator iter;
     //printf("\tlookup: target %s\n", name);
     //printf("\tlookup: size %d\n", dir_list.size());
    // for(iter = dir_list.begin(); iter != dir_list.end(); ++iter){
    //  printf("\tlookup: content circle %s\n", iter->name.c_str());
    //}
   	 for(iter = dir_list.begin(); iter != dir_list.end(); ++iter){
      // printf("\tlookup: content %s\n", iter->name.c_str());
   		 if(iter->name == std::string(name)){
   			 found = true;
   			 ino_out = iter->inum;
   			 return r;
   		 }
   	 }
     found = false;
     printf("\tlookup: no such file or directory\n");
     return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
     //check the parent
     //printf("\treaddir begins\n");
     if(!isdir(dir)){
       printf("\treaddir: error! parent is not a directory\n");
       return IOERR;
     }
     std::string buf;
     if(ec->get(dir, buf) != OK){
       printf("\treaddir: error! parent invalid\n");
       return IOERR;
     }

     //read
     //directory struct : inum  / filename / ... / 0 /
     int old_idx = 0, new_idx = 0;
     dirent temp_dir;
     while(true){
       new_idx = buf.find('/', old_idx);
       if(new_idx == -1) break;
       temp_dir.inum = n2i(buf.substr(old_idx, new_idx - old_idx));
       if(temp_dir.inum == 0) break;
       old_idx = new_idx + 1;
       new_idx = buf.find('/', old_idx);
       temp_dir.name = buf.substr(old_idx, new_idx - old_idx);
       old_idx = new_idx + 1;
       list.push_back(temp_dir);
     }
     //printf("\treaddir finished, size %d\n", list.size());
     return OK;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    if(ec->get(ino, buf) != OK){
       printf("\twrite: error! ino invalid\n");
       return IOERR;
    }
    if(off > buf.length()) data="";
    else data = buf.substr(off, size);

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    //data >= size

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    std::string buf, data_written = std::string(data, size);
    if(ec->get(ino, buf) != OK){
       printf("\twrite: error! ino invalid\n");
       return IOERR;
     }
    bytes_written = 0;
    int old_length = buf.length();

    if(off + size > old_length){
        buf.resize(off);
        buf += data_written;
        //data > size, cut surplus
        if(buf.length() > off + size) buf.resize(off + size);
        if(off > old_length){
            bytes_written = size + off - old_length;
        }
        else{
            bytes_written = size;
        }
    }

    else{
        bytes_written = size;
        buf.replace(off, size, data_written.substr(0, size));
    }
    ec->put(ino, buf);
    return r;
}
int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    std::list<dirent> dir_list;
    bool found;
    yfs_client::inum ino;
    r = lookup(parent, name, found, ino);
    std::string new_dir = "";

    if(found){
        ec->remove(ino);
        std::list<dirent>::iterator iter;
        std::string name_unlink = std::string(name);
        readdir(parent, dir_list);
        for(iter = dir_list.begin(); iter != dir_list.end(); ++iter){
            if(iter->name == name_unlink){
                continue;
            }
            else{
                new_dir += filename(iter->inum) + '/' + iter->name + '/';
            }
        }
        new_dir += "0/";

    }else r = NOENT; 
    ec->put(parent, new_dir);

    return r;
}

int yfs_client::create_symlink(inum parent, const char* link, const char* name, inum& ino_out){
    printf("create symlink begins\n");
    bool exist;
    lookup(parent, name, exist, ino_out);
    if(exist){
       printf("\tmkdir: error! file exists\n");
       return EXIST;
    }

    std::string buf;
    ec->get(parent, buf);
    ec->create(extent_protocol::T_SYM, ino_out);

    buf = buf.substr(0,buf.length() - 2);
    buf += filename(ino_out) + "/" + std::string(name) + "/0/" ;
    ec->put(parent, buf);
    ec->put(ino_out, std::string(link));

    return OK;
}

int yfs_client::read_symlink(inum ino, std::string & link){
    if(ec->get(ino, link) != OK){
       printf("\tread_symlink: error! ino invalid\n");
       return IOERR;
    }
    return OK;
}
