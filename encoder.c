#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include "mempool.h"
#include "mp3.h"
#include "thread_pool.h"
#include "db.h"
#include "mstring.h"
#include "display.h"
#include "hash.h"
#include "event.h"

typedef struct encode_file_t {
    char   *src;
    char   *dst;
    time_t  mtime;

    char    data[0];
} encode_file_t;

void encode_th(void *data)
{
    encode_file_t *enc          = data;
    char          *argv[8];
    int            pipedesc[2];
    int            pid_decoder;
    int            pid_encoder;
    mp3_info_t     info;
    int            ret;
    song_t         song;

    ret = mp3_info_decode(&info, enc->src);
    if(info.album  == NULL ||
       info.artist == NULL ||
       info.title  == NULL)
        ret = -2;

    song.src      = enc->src;
    song.dst      = enc->dst;
    song.album    = info.album;
    song.artist   = info.artist;
    song.title    = info.title;
    song.years    = info.years;
    song.track    = info.track;
    song.track_nb = info.nb_track;
    song.genre    = 0;
    song.mtime    = enc->mtime;
    song.bitrate  = 192;
    song.duration = info.duration;
    
    switch(ret) {
    case -1:
        print_warning("Skip %s -> %s", enc->src, enc->dst);
        mp3_info_free(&info);
        break;
    case -2:
        print_warning("Bad tag %s -> %s", enc->src, enc->dst);
        song.status = SONG_STATUS_BAD_TAG;
        db_new_song(&song);
        mp3_info_free(&info);
        break;
    default:
        print_log("Encode %s -> %s", enc->src, enc->dst);
        mp3_info_dump(&info);
        break;
    }

    if(ret < 0)
        return;

    pipe(pipedesc);
    pid_decoder = fork();
    if(pid_decoder == 0) {
        argv[0] = "mpg123";
        argv[1] = "--stereo";
        argv[2] = "-r";
        argv[3] = "44100";
        argv[4] = "-s";
        argv[5] = enc->src;
        argv[6] = NULL;
        dup2(pipedesc[1], 1);
        close(0);
        close(pipedesc[0]);
        close(2);
        nice(2);
        execv("/usr/bin/mpg123", argv);
        abort();
    }

    pid_encoder = fork();
    if(pid_encoder == 0) {
        close(pipedesc[1]);
        close(1);
        close(2);
        dup2(pipedesc[0], 0);
        argv[0] = "lame";
        argv[1] = "-";
        argv[2] = enc->dst;
        argv[3] = "-r";
        argv[4] = "-b";
        argv[5] = "192";
        argv[6] = "-t";
        argv[7] = NULL;
        nice(2);
        execv("/usr/bin/lame", argv);
        abort();
    }
    close(pipedesc[0]);
    close(pipedesc[1]);

    int stat_loc;

    waitpid(pid_decoder, &stat_loc, 0);
    waitpid(pid_encoder, &stat_loc, 0);


    if(stat_loc == 0)
        song.status = SONG_STATUS_OK;
    else
        song.status = SONG_STATUS_ENCODING_FAIL;

    db_new_song(&song);

    mp3_info_free(&info);
    free(data);
}

typedef struct inode_cache_entry {
    ino_t  ino;
    time_t mtime;
} inode_cache_entry_t;

typedef struct inode_cache {
    hash_t                      inode_hash;
    mempool_t                  *inode_pool;
} inode_cache_t;

typedef struct encoder_data {
    thread_pool_t              *pool;
    inode_cache_t               inode_cache;
    string_t                    srcdir;
    string_t                    dstdir;
} encoder_data_t;

static uint32_t inode_cache_hash(ino_t *ino)
{
    return *ino;
}

static int inode_cache_cmp(ino_t *ino1, ino_t *ino2)
{
    return (*ino1 == *ino2);
}

void inode_cache_init(inode_cache_t *cache)
{
    cache->inode_pool = mempool_new(sizeof(inode_cache_entry_t), 64);
    hash_init(&cache->inode_hash, (cmp_f)inode_cache_cmp, (hash_f)inode_cache_hash, 32);
}

int inode_cache_insert(inode_cache_t *cache, ino_t ino, time_t mtime)
{
    inode_cache_entry_t *entry;

    entry = hash_get(&cache->inode_hash, &ino);
    if(entry) {
        if(entry->mtime == mtime)
            return -1;
        // update
        entry->mtime = mtime;
        return 0;
    }
    
    entry = mempool_alloc(cache->inode_pool);

    entry->ino   = ino;
    entry->mtime = mtime;

    hash_add(&cache->inode_hash, &entry->ino, entry);

    return 0;
}

void scan(const unsigned char *src, time_t mtime, void *data)
{
    inode_cache_t *inode_cache = data;
    struct stat    buf;

    stat((const char *)src, &buf);

    inode_cache_insert(inode_cache, buf.st_ino, mtime);
}

static void encoder_scan_dir(encoder_data_t *data, const struct timeval *now)
{
    DIR                        *dp; 
    struct dirent              *dirp;
    int                         scan_time    = 30; // 30s

    dp = opendir(data->srcdir.txt);
    if(dp == NULL)
        return;

    while ((dirp = readdir(dp)) != NULL) {
        encode_file_t *encode_data;
        string_t       name;
        string_t       srcfile;
        string_t       dstfile;

        name = string_init_static(dirp->d_name);

        if((name.len == 1 && memcmp(dirp->d_name, "." , 1) == 0) ||
           (name.len == 2 && memcmp(dirp->d_name, "..", 2) == 0))
            continue;

        encode_data = malloc(sizeof(encode_file_t) +
                             name.len + data->srcdir.len + 1 + 1 +
                             name.len + data->dstdir.len + 1 + 1);

        srcfile = string_init_full(encode_data->data, 0,
                                   name.len + data->srcdir.len + 1 + 1, STRING_ALLOC_STATIC);
        dstfile = string_init_full(encode_data->data + srcfile.size, 0,
                                   name.len + data->srcdir.len + 1 + 1, STRING_ALLOC_STATIC);

        srcfile   = string_concat(srcfile, data->srcdir);
        srcfile   = string_add_chr(srcfile, '/');
        srcfile   = string_concat(srcfile, name);
        encode_data->src = srcfile.txt;

        dstfile   = string_concat(dstfile, data->dstdir);
        dstfile   = string_add_chr(dstfile, '/');
        dstfile   = string_concat(dstfile, name);
        encode_data->dst = dstfile.txt;

        struct stat buf;

        stat(encode_data->src, &buf);            

        encode_data->mtime = buf.st_mtime;


        if(S_ISDIR(buf.st_mode)) {
            string_t       srcdir_save;

            srcdir_save = data->srcdir;
            data->srcdir = srcfile;
            encoder_scan_dir(data, now);
            data->srcdir = srcdir_save;

            free(encode_data);

            continue;
        }

        if(!S_ISREG(buf.st_mode) || buf.st_mtime + (scan_time * 2) > now->tv_sec ||
           inode_cache_insert(&data->inode_cache, buf.st_ino, buf.st_mtime) == -1) {
            free(encode_data);
            continue;
        }

        thread_pool_add(data->pool, encode_th, encode_data);
    }
    closedir(dp);
}
 
void encoder_scan(event_t *timer, const struct timeval *now, void *void_data)
{
    encoder_data_t             *data         = (encoder_data_t *) void_data;

    (void) timer;

    print_debug("Encoder: scan new file");

    encoder_scan_dir(data, now);

    print_debug("Encoder: scan end");

    return;
}

encoder_data_t data;

void encoder_init(char *src, char *dst, int nb_thread)
{
    db_init();

    data.srcdir = string_dup(string_init_static(src));
    data.dstdir = string_dup(string_init_static(dst));

    inode_cache_init(&data.inode_cache);

    db_scan_song(scan, &data.inode_cache);

    data.pool = thread_pool_new(nb_thread);

    event_timer_add(30000, EVENT_TIMER_KIND_PERIODIC, encoder_scan, &data);
}
