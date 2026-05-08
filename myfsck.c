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

bool bit_is_used(uint blknum)
{
    char * start=(char*)img+BBLOCK(blknum,ninodes)*BSIZE;
    uint bytepos=(blknum%BPB)/8;
    uint bitpos=blknum%8;
    return start[bytepos]&(1<<bitpos);
}



void traverse_dirent(uint blknum,ushort currinodenum,int * curcnt,int * parcnt,int inodeindirent[])
{
    struct dirent * startptr=(struct dirent *)((char *)img+blknum*BSIZE);
    int nentry=BSIZE/sizeof(struct dirent);
    for(int i=0;i<nentry;i++)
    {
        struct dirent * tempdirent=startptr+i;
        if(tempdirent->inum!=0)
        {
            if(strncmp(".",tempdirent->name,DIRSIZ)==0)
            {
                if(tempdirent->inum!=currinodenum)
                {
                    bad_directory_format_error();
                    exit(1);
                }
                (*curcnt)++;
            }
            else if(strncmp("..",tempdirent->name,DIRSIZ)==0) (*parcnt)++;
            else inodeindirent[tempdirent->inum]+=1;
        }
    }
}


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

int check_direct_address(struct dinode * inode,bool visited[],ushort currinodenum,int inodeindirent[])
{
    int blkcnt=0;
    for(int i=0;i<NDIRECT;i++)
    {
        uint tempblknum=inode->addrs[i];
        if(tempblknum!=0&&(tempblknum<datblkstnum||tempblknum>=datblkednum))
        {
            bad_direct_address_error();
            exit(1);
        }
        if(tempblknum!=0&&visited[tempblknum])
        {
            bad_direct_address_once_error();
            exit(1);
        }
        if(tempblknum!=0)
        {
            visited[addrs[i]]=1;
            if(!bit_is_used(tempblknum))
            {
                bad_used_address_error();
                exit(1);
            }
            visited[tempblknum]=1;
            blkcnt++;
            int curcnt=0,parcnt=0;
            if(inode->type==1) traverse_dirent(tempblknum,currinodenum,&curcnt,&parcnt,inodeindirent);
            if(curcnt!=1||parcnt!=1) 
            {
                bad_directory_format_error();
                exit(1);
            }
        }
    }
    return blkcnt;
}
            

int check_indirect_address(struct dinode * inode,bool visited[],ushort currinodenum,int inodeindirent[])
{
    int blkcnt=0;
    uint ndirblknum=inode->addrs[NDIRECT];
    if(!bit_is_used(ndirblknum))
    {
        bad_used_address_error();
        exit(1);
    }
    if(ndirblknum!=0&&visited[ndirblknum])
    {
        bad_indirect_address_once_error();
        exit(1);
    }
    if(ndirblknum!=0)
    {
        if(ndirblknum>=datblkstnum&&ndirblknum<datblkednum)
        {
            visited[ndirblknum]=1;
            // blkcnt++;
            uint * startptr=(uint *)((char *)img+ndirblknum*BSIZE);
            for(int i=0;i<128;i++)
            {
                uint temp=startptr[i];
                if(temp!=0&&(temp<datblkstnum||temp>=datblkednum))
                {
                    bad_indirect_address_error();
                    exit(1);    
                }
                if(temp!=0&&visited[temp])
                {
                    bad_indirect_address_once_error();
                    exit(1);
                }
                if(temp!=0) 
                {
                    visited[addrs[i]]=1;
                    if(!bit_is_used(temp))
                    {
                        bad_used_address_error();
                        exit(1);
                    }
                    visited[temp]=1;
                    blkcnt++;
                    int curcnt=0,parcnt=0;
                    if(inode->type==1) traverse_dirent(temp,currinodenum,&curcnt,&parcnt,inodeindirent);
                    if(curcnt!=1||parcnt!=1) 
                    {
                        bad_directory_format_error();
                        exit(1);
                    }
                }
            }
        }
        else 
        {
            check_indirect_address(uint addrs[])
            exit(1);
        }
    }
    return blkcnt;
}



void helper()
{
    bool blkvisited[sbsize+1];
    bool usedinode[ninodes+1];
    int inodeindirent[ninodes+1];
    memset(blkvisited,0,sizeof(blkvisited));
    memset(usedinode,0,sizeof(usedinode));
    memset(inodeindirent,0,sizeof(inodeindirent));
    for(int i=0;i<ninodes;i++)
    {
        struct dinode * temp=startinode+i;
        check_node_type(temp);
        int dircnt=check_direct_address(temp,blkvisited,i+1,inodeindirent);
        int indircnt=check_indirect_address(temp,blkvisited,i+1,inodeindirent);
        int sum=dircnt+indircnt;
        if(temp->size<=(sum-1)*BSIZE||temp->size>sum*BSIZE)
        {
            bad_file_size_error();
            exit(1);
        }
        if(temp->type!=0) usedinode[i+1]=1;
    }


    
    for(int i=0;i<ninodes;i++)
    {
        struct dinode * temp=startinode+i;
        if(usedinode[i+1]&&!inodeindirent[i+1])
        {
            bad_used_inode_not_found_error();
            exit(1);
        }
        if(!usedinode[i+1]&&inodeindirent[i+1])
        {
            bad_inode_referred_marked_error();
            exit(1);
        }
        if(temp->type==2&&temp->nlink!=inodeindirent[i+1])
        {
            bad_reference_count_error();
            exit(1);
        }
        if(temp->type==1&&inodeindirent[i+1]!=1)
        {
            bad_directory_once_error();
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
