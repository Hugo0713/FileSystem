#include "disk.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "log.h"

#define BLOCKSIZE 512

// global variables
int _ncyl, _nsec, _ttd;
int fd;
long FILESIZE;
char *diskfile;
int cur_cyl = 0;

int init_disk(char *filename, int ncyl, int nsec, int ttd)
{
    _ncyl = ncyl;
    _nsec = nsec;
    _ttd = ttd;
    // do some initialization...

    // open file

    // stretch the file

    // mmap
    fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
    {
        Log("Error: Could not open file '%s'.\n", filename);
        exit(-1);
    }

    off_t lseek(int filedes, off_t offset, int whence);
    FILESIZE = BLOCKSIZE * nsec * ncyl;
    int result = lseek(fd, FILESIZE - 1, SEEK_SET);
    if (result == -1)
    {
        Log("Error calling lseek() to 'stretch' the file");
        close(fd);
        exit(-1);
    }
    result = write(fd, "", 1);
    if (result != 1)
    {
        Log("Error writing last byte of the file");
        close(fd);
        exit(-1);
    }

    diskfile = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (diskfile == MAP_FAILED)
    {
        Log("Error mmapping the file");
        close(fd);
        exit(-1);
    }

    Log("Disk initialized: %s, %d Cylinders, %d Sectors per cylinder", filename, ncyl, nsec);
    return 0;
}

// all cmd functions return 0 on success
int cmd_i(int *ncyl, int *nsec)
{
    // get the disk info
    *ncyl = _ncyl;
    *nsec = _nsec;
    return 0;
}

int cmd_r(int cyl, int sec, char *buf)
{
    // read data from disk, store it in buf
    if (cyl >= _ncyl || sec >= _nsec || cyl < 0 || sec < 0)
    {
        Log("Invalid cylinder or sector");
        return 1;
    }
    if (buf == NULL)
    {
        Log("Buffer is NULL");
        return 1;
    }
    // calculate the offset
    long offset = (long)cyl * _nsec * BLOCKSIZE + (long)sec * BLOCKSIZE;
    // calculate the delay
    int delay = abs(cyl - cur_cyl) * _ttd;
    usleep(delay * 1000);

    // read the data
    memcpy(buf, diskfile + offset, BLOCKSIZE);
    // update the current cylinder
    cur_cyl = cyl;
    Log("Read %d bytes from cylinder %d, sector %d", BLOCKSIZE, cyl, sec);
    return 0;
}

int cmd_w(int cyl, int sec, int len, char *data)
{
    // write data to disk
    if (cyl >= _ncyl || sec >= _nsec || cyl < 0 || sec < 0)
    {
        Log("Invalid cylinder or sector");
        return 1;
    }
    if (data == NULL)
    {
        Log("Data is NULL");
        return 1;
    }
    if (len > BLOCKSIZE)
    {
        Log("Data length is greater than block size");
        return 1;
    }
    // calculate the offset
    long offset = (long)cyl * _nsec * BLOCKSIZE + (long)sec * BLOCKSIZE;
    // calculate the delay
    int delay = abs(cyl - cur_cyl) * _ttd;
    usleep(delay * 1000);
    // write the data
    memcpy(diskfile + offset, data, len);
    if (len < 512)
    {
        memset(diskfile + offset + len, 0, 512 - len);
    }
    // update the current cylinder
    cur_cyl = cyl;
    Log("Wrote %d bytes to cylinder %d, sector %d", len, cyl, sec);
    return 0;
}

void close_disk()
{
    // unmap
    if (diskfile != NULL)
    {
        if (munmap(diskfile, FILESIZE) == -1)
        {
            Log("Error unmapping the file");
        }
        Log("Disk unmapped");
        diskfile = NULL;
    }
    // close the file
    if (close(fd) == -1)
    {
        Log("Error closing the file");
    }
    else
    {
        Log("Disk closed");
    }
}
