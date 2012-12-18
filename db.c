#include <sqlite3.h>
#include <stdlib.h>

#include "db.h"
#include <string.h>

enum file_status {
    FILE_WAIT              = 1,
    FILE_BAD_TAG           = 2,
    FILE_ENCODING_PROGRESS = 3,
    FILE_ENCODING_FAIL     = 4,
    FILE_OK                = 5,
};

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

void db_request_song(char *fields_selection, char *field, char *value, char *order, int start, int len)
{
    char        buffer[1024];
    mstring     req;

    req = string_init_full(buffer, 0, sizeof(buffer), STRING_ALLOC_STATIC);

    if(fields_selection) {
        req = string_concat(req, STRING_INIT_CSTR("SELECT ")); 
        req = string_concat(req, string_init_static(fields_selection));
        req = string_concat(req, STRING_INIT_CSTR("FROM library WHERE status=5 ")); 
    } else {
        req = string_concat(req, STRING_INIT_CSTR("SELECT * FROM library WHERE status=5 "));
    }
    if(field) {
        req = string_concat(req, STRING_INIT_CSTR("AND "));
        req = string_concat(req, string_init_static(field));
        req = string_concat(req, STRING_INIT_CSTR(" LIKE ? "));
    }
    if(order) {
        req = string_concat(req, STRING_INIT_CSTR("ORDER BY "));
        req = string_concat(req, string_init_static(order));
        req = string_add_chr(req, ' ');
    }

    if(len != -1) {
        char len_str[16];
        snprintf(len_str, sizeof(len_str), "%i", len);

        if(first != -1) {
            char first_str[16];
            snprintf(first_str, sizeof(first_str), "%i", start);

            req = string_concat(req, STRING_INIT_CSTR("LIMIT "));
            req = string_concat(req, string_init_static(first_str));
            req = string_add_chr(req, ',');
            req = string_concat(req, string_init_static(len_str));
        } else {
            req = string_concat(req, STRING_INIT_CSTR("LIMIT "));
            req = string_concat(req, string_init_static(len_str));
        }
    }

}

void db_scan_song(scan_fn fn, void *data)
{
    time_t                mtime;
    const unsigned char  *src;
    sqlite3_stmt         *stmt    = NULL;
    static const char     req[]   = "SELECT src, mtime FROM library WHERE 1";
    int                   running = 1;

    sqlite3_prepare_v2(db, req, sizeof(req), &stmt, NULL);

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

void db_new_song(song_t *song)
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
