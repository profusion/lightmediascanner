#include <lightmediascanner_db.h>
#include "lightmediascanner_db_private.h"
#include <stdlib.h>
#include <stdio.h>

struct lms_db_image {
    sqlite3 *db;
    sqlite3_stmt *insert;
    int _references;
};

lms_db_image_t *_singleton = NULL;

static int
_db_create_table_if_required(sqlite3 *db)
{
    char *errmsg;
    int r, ret;

    errmsg = NULL;
    r = sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS images ("
                     "id INTEGER PRIMARY KEY, "
                     "title TEXT, "
                     "artist TEXT, "
                     "date INTEGER NOT NULL, "
                     "width INTEGER NOT NULL, "
                     "height INTEGER NOT NULL, "
                     "orientation INTEGER NOT NULL, "
                     "thumb_width INTEGER NOT NULL, "
                     "thumb_height INTEGER NOT NULL, "
                     "gps_lat REAL DEFAULT 0.0, "
                     "gps_long REAL DEFAULT 0.0, "
                     "gps_alt REAL DEFAULT 0.0"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'images' table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    r = sqlite3_exec(db,
                     "CREATE INDEX IF NOT EXISTS images_date_idx ON images ("
                     "date"
                     ")",
                     NULL, NULL, &errmsg);
    if (r != SQLITE_OK) {
        fprintf(stderr, "ERROR: could not create 'images_date_idx' index: %s\n",
                errmsg);
        sqlite3_free(errmsg);
        return -2;
    }

    ret = lms_db_create_trigger_if_not_exists(db,
        "   delete_images_on_files_deleted "
        "DELETE ON files FOR EACH ROW BEGIN "
        "   DELETE FROM images WHERE id = OLD.id; END;");
    if (ret != 0)
        goto done;

    ret = lms_db_create_trigger_if_not_exists(db,
        "   delete_files_on_images_deleted "
        "DELETE ON images FOR EACH ROW BEGIN "
        "   DELETE FROM files WHERE id = OLD.id; END;");

  done:
    return ret;
}

static int
_db_compile_all_stmts(lms_db_image_t *ldi)
{
    ldi->insert = lms_db_compile_stmt(ldi->db,
        "INSERT OR REPLACE INTO images ("
        "id, title, artist, date, width, height, orientation, "
        "thumb_width, thumb_height, gps_lat, gps_long, gps_alt) VALUES ("
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    if (!ldi->insert)
        return -1;

    return 0;
}

lms_db_image_t *
lms_db_image_new(sqlite3 *db)
{
    lms_db_image_t *ldi;

    if (_singleton) {
        _singleton->_references++;
        return _singleton;
    }

    if (!db)
        return NULL;

    if (_db_create_table_if_required(db) != 0) {
        fprintf(stderr, "ERROR: could not create table.\n");
        return NULL;
    }

    ldi = calloc(1, sizeof(lms_db_image_t));
    ldi->_references = 1;
    ldi->db = db;

    if (_db_compile_all_stmts(ldi) != 0) {
        fprintf(stderr, "ERROR: could not compile image statements.\n");
        lms_db_image_free(ldi);
        return NULL;
    }

    return ldi;
}

int
lms_db_image_free(lms_db_image_t *ldi)
{
    if (!ldi)
        return -1;

    ldi->_references--;
    if (ldi->_references > 0)
        return 0;

    if (ldi->insert)
        lms_db_finalize_stmt(ldi->insert, "insert");

    free(ldi);
    _singleton = NULL;

    return 0;
}

int
_db_insert(lms_db_image_t *ldi, const struct lms_image_info *info)
{
    sqlite3_stmt *stmt;
    int r, ret;
    unsigned long tw, th;

    if (info->height < info->width) {
        tw = 128;
        th = (info->height * 128) / info->width;
        if (th == 0)
            th = 1;
    } else if (info->height == info->width)
        tw = th = 128;
    else {
        th = 128;
        tw = (info->width * 128) / info->height;
        if (tw == 0)
            tw = 1;
    }

    stmt = ldi->insert;

    ret = lms_db_bind_int64(stmt, 1, info->id);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 2, info->title.str, info->title.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_text(stmt, 3, info->artist.str, info->artist.len);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 4, info->date);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 5, info->width);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 6, info->height);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 7, info->orientation);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 8, tw);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_int(stmt, 9, th);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_double(stmt, 10, info->gps.latitude);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_double(stmt, 11, info->gps.longitude);
    if (ret != 0)
        goto done;

    ret = lms_db_bind_double(stmt, 12, info->gps.altitude);
    if (ret != 0)
        goto done;

    r = sqlite3_step(stmt);
    if (r != SQLITE_DONE) {
        fprintf(stderr, "ERROR: could not insert image info: %s\n",
                sqlite3_errmsg(ldi->db));
        ret = -13;
        goto done;
    }

    ret = 0;

  done:
    lms_db_reset_stmt(stmt);

    return ret;
}

int
lms_db_image_add(lms_db_image_t *ldi, struct lms_image_info *info)
{
    if (!ldi)
        return -1;
    if (!info)
        return -2;
    if (info->id < 1)
        return -3;

    return _db_insert(ldi, info);
}
