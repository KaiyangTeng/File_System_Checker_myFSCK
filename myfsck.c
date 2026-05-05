#include "types.h"
#include "log.h"
#include "fs.h"
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

char * imgpath;
void * img;
struct superblock * spblcok;
struct dinode * startinode;
uint sbsize,nblocks,ninodes,datblkstnum,datblkednum;

void check_size(void)
{
    uint inodezie=ninodes/IPB+1,bitmapsize=sbsize/BPB+1;
    datblkstnum=inodezie+bitmapsize+2;
    datblkednum=nblocks+datblkstnum;
    if(sbsize!=datblkednum)
    {
        superblock_error();
        exit(1);
    }
}

void check_node_type(struct dinode * temp)
{
    short type=temp->type;
    if(type!=0&&type!=1&&type!=2&&type!=3)
    {
        bad_inode_error();
        exit(1);
    }
}

void check_direct_address(uint addrs[])
{
    for(int i=0;i<NDIRECT;i++)
    {
        if(addrs[i]!=0)
        {
            if(addrs[i]<datblkstnum||addrs[i]>=datblkednum)
            {
                bad_direct_address_error();
                exit(1);
            }
        }
    }
}


void check_indirect_address(uint addrs[])
{
    uint ndirblknum=addrs[NDIRECT];
    if(ndirblknum!=0)
    {
        if(ndirblknum>=datblkstnum&&ndirblknum<datblkednum)
        {
            uint * startptr=(uint *)((char *)img+ndirblknum*BSIZE);
            for(int i=0;i<128;i++)
            {
                uint temp=startptr[i];
                if(temp<datblkstnum||temp>=datblkednum)
                {
                    bad_indirect_address_error();
                    exit(1);    
                }
            }
        }
        else 
        {
            bad_indirect_address_error();
            exit(1);
        }
    }
}


int main(int argc, char *argv[]) 
{
    if(argc==2)
    {
        imgpath=argv[1];
    }
    else if(argc==3)
    {
        if(strcmp("-r",argv[1])==0)
        {
            imgpath=argv[2];
        }
        else 
        {
            printf("invaid input\n");
            return 0;
        }
    }
    else 
    {
        printf("invaid input\n");
        return 0;
    }
    int fd=open(imgpath,O_RDONLY);
    if(fd<0)
    {
        fprintf(stderr, "image not found.\n"); 
        exit(1);
    }
    struct stat st;
    if(fstat(fd,&st)<0)
    {
        close(fd);
        exit(1);
    }
    img=mmap(NULL,st.st_size,PROT_READ, MAP_PRIVATE, fd, 0);
    if(img<0) 
    {
        close(fd);
        exit(1);
    }
    




    return 0;
}
