#include <sqlite3.h>
#include <stdlib.h>

#include "db.h"
#include "mstring.h"
#include "song.h"

#include <string.h>

sqlite3 *db = NULL;

int db_init(void)
{
    sqlite3_open("jukebox.db", &db);
    sqlite3_exec(db, "create table if not exists library ("
                       "mid INTEGER PRIMARY KEY,"
                       "src TEXT, dst TEXT,"
                       "title TEXT, artist TEXT, album TEXT, years INTEGER UNSIGNED NULL,"
                       "track INTEGER UNSIGNED NULL, trackNb INTEGER UNSIGNED NULL, genre INTEGER UNSIGNED NULL,"
                       "status INTEGER, mtime INTEGER, bitrate INTEGER, duration INTEGER);", NULL, 0, NULL);
    return 0; 
}

void db_update_song()
{

}

void db_scan_song(scan_fn fn, void *data)
{
    time_t                mtime;
    const unsigned char  *src;
    sqlite3_stmt         *stmt    = NULL;
    static const char     req[]   = "SELECT src, mtime FROM library WHERE 1";
    int                   running = 1;

    while(sqlite3_prepare_v2(db, req, sizeof(req), &stmt, NULL) < 0);

    while(running) {
        int s;
        s = sqlite3_step(stmt);
        switch(s) {
        case SQLITE_ROW:
            src   = sqlite3_column_text(stmt, 0);
            mtime = sqlite3_column_int(stmt, 1);
            fn(src, mtime, data);
            break;

        case SQLITE_DONE:
        default:
            running = 0;
            break;
        }
    }
}

int db_song_random(void)
{
    char          req[]   = "SELECT mid FROM library WHERE status=5 ORDER BY RANDOM() LIMIT 1";
    int           running = 1;
    int           mid     = -1;
    sqlite3_stmt *stmt    = NULL;

    sqlite3_prepare_v2(db, req, sizeof(req), &stmt, NULL);

    while(running) {
        int s;
        s = sqlite3_step(stmt);
        switch(s) {
        case SQLITE_ROW:
            mid = sqlite3_column_int(stmt, 0);
        case SQLITE_DONE:
            running = 0;
            break;
        default:
            break;
        }
    }
    sqlite3_finalize(stmt);

    return mid;
}

static song_t * db_to_song(sqlite3_stmt *stmt)
{
    song_t *song;

    song = malloc(sizeof(song_t));

    song->mid      = sqlite3_column_int(stmt, 0);
    song->src      = strdup((char *)sqlite3_column_text(stmt, 1));
    song->dst      = strdup((char *)sqlite3_column_text(stmt, 2));
    song->title    = strdup((char *)sqlite3_column_text(stmt, 3));
    song->artist   = strdup((char *)sqlite3_column_text(stmt, 4));
    song->album    = strdup((char *)sqlite3_column_text(stmt, 5));
    song->years    = sqlite3_column_int(stmt, 6);
    song->track    = sqlite3_column_int(stmt, 7);
    song->track_nb = sqlite3_column_int(stmt, 8);
    song->status   = sqlite3_column_int(stmt, 9);
    song->years    = sqlite3_column_int(stmt, 10);
    song->mtime    = sqlite3_column_int(stmt, 11);
    song->bitrate  = sqlite3_column_int(stmt, 12);
    song->duration = sqlite3_column_int(stmt, 13);

    return song;
}

song_t * db_song_load(int mid)
{
    sqlite3_stmt         *stmt        = NULL;
    int                   running     = 1;
    char                  req[]       = "SELECT * FROM library WHERE mid=?";
    song_t               *song        = NULL;

    if(mid == -1) {
        return NULL;
    }

    sqlite3_prepare_v2(db, req, sizeof(req), &stmt, NULL);
    sqlite3_bind_int(stmt,  1, mid);

    while(running) {
        int s;
        s = sqlite3_step(stmt);
        switch(s) {
        case SQLITE_ROW:
            song = db_to_song(stmt);
        case SQLITE_DONE:
            running = 0;
            break;
        default:
            break;
        }
    }
    sqlite3_finalize(stmt);

    return song;
}

void db_song_save(song_t *song)
{
    static const char  req[] = "INSERT INTO library (src, dst, title, artist, album, years, track, trackNb, genre, status, mtime, bitrate, duration) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt      *stmt = NULL;

    sqlite3_prepare_v2(db, req, sizeof(req), &stmt, NULL);

    sqlite3_bind_text(stmt,  1, song->src,
                      song->src ? strlen(song->src) : 0,       SQLITE_STATIC);
    sqlite3_bind_text(stmt,  2, song->dst,
                      song->dst ? strlen(song->dst) : 0,       SQLITE_STATIC);
    sqlite3_bind_text(stmt,  3, song->title,
                      song->title ? strlen(song->title) : 0,   SQLITE_STATIC);
    sqlite3_bind_text(stmt,  4, song->artist,
                      song->artist ? strlen(song->artist) : 0, SQLITE_STATIC);
    sqlite3_bind_text(stmt,  5, song->album,
                      song->album ? strlen(song->album) : 0,   SQLITE_STATIC);
    sqlite3_bind_int (stmt,  6, song->years);
    sqlite3_bind_int (stmt,  7, song->track);
    sqlite3_bind_int (stmt,  8, song->track_nb);
    sqlite3_bind_int (stmt,  9, song->genre);
    sqlite3_bind_int (stmt, 10, song->status);
    sqlite3_bind_int (stmt, 11, song->mtime);
    sqlite3_bind_int (stmt, 12, song->bitrate);
    sqlite3_bind_int (stmt, 13, song->duration);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
