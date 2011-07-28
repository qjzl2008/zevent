#include "btpd.h"
#include <io.h>

void
active_add(const uint8_t *hash)
{
    FILE *fp;
    if ((fp = fopen("active", "ab")) == NULL) {
        btpd_log(BTPD_L_ERROR, "couldn't open file 'active' (%s).\r\n",
            strerror(errno));
        return;
    }
    fwrite(hash, 20, 1, fp);
    fclose(fp);
}

static void
active_del_pos(FILE *fp, long pos, off_t *size)
{
    DWORD offlo,offhi,rc;
    HANDLE hFile;
    uint8_t ehash[20];
    fseek(fp, -20, SEEK_END);
    fread(ehash, 20, 1, fp);
    fseek(fp, pos, SEEK_SET);
    fwrite(ehash, 20, 1, fp);
    fflush(fp);
    *size -= 20;

    offlo = (DWORD)*size;
    offhi = 0;
    hFile = (HANDLE)_get_osfhandle(_fileno(fp));

    rc = SetFilePointer(hFile,offlo,&offhi,FILE_BEGIN);
    if (rc == INVALID_SET_FILE_POINTER)
    {
    }
    SetEndOfFile(hFile);
}

void
active_del(const uint8_t *hash)
{
    FILE *fp;
    long pos;
    struct stat sb;
    uint8_t buf[20];

    if ((fp = fopen("active", "rb+")) == NULL) {
        btpd_log(BTPD_L_ERROR, "couldn't open file 'active' (%s).\r\n",
            strerror(errno));
        return;
    }

    if (fstat(fileno(fp), &sb) != 0) {
        btpd_log(BTPD_L_ERROR, "couldn't stat file 'active' (%s).\r\n",
            strerror(errno));
        goto close;
    }

    pos = 0;
    while (fread(buf, 20, 1, fp) == 1) {
        if (memcmp(buf, hash, 20) == 0) {
            active_del_pos(fp, pos, &sb.st_size);
            break;
        }
        pos += 20;
    }
close:
    fclose(fp);
}

void
active_start(void)
{
    FILE *fp;
    long pos;
    struct stat sb;
    uint8_t hash[20];

    if ((fp = fopen("active", "rb+")) == NULL)
        return;

    if (fstat(fileno(fp), &sb) != 0) {
        btpd_log(BTPD_L_ERROR, "Couldn't stat file 'active' (%s).\r\n",
            strerror(errno));
        goto close;
    }

    pos = 0;
    while (fread(hash, sizeof(hash), 1, fp) == 1) {
        struct tlib *tl = tlib_by_hash(hash);
        if (tl != NULL && tl->tp == NULL)
            if (torrent_start(tl) != 0) {
                active_del_pos(fp, pos, &sb.st_size);
                fseek(fp, pos, SEEK_SET);
            }
        pos += 20;
    }
close:
    fclose(fp);
}

void
active_clear(void)
{
    _unlink("active");
}
