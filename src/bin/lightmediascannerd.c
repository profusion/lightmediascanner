#include "lightmediascanner.h"
#include <gio/gio.h>
#include <glib-unix.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>

static char *db_path = NULL;
static char **charsets = NULL;
static GHashTable *categories = NULL;
static int commit_interval = 100;
static int slave_timeout = 60;
static int delete_older_than = 30;
static gboolean vacuum = FALSE;
static gboolean startup_scan = FALSE;
static gboolean omit_scan_progress = FALSE;

static GDBusNodeInfo *introspection_data = NULL;

static const char BUS_PATH[] = "/org/lightmediascanner/Scanner1";
static const char BUS_IFACE[] = "org.lightmediascanner.Scanner1";

static const char introspection_xml[] =
    "<node>"
    "  <interface name=\"org.lightmediascanner.Scanner1\">"
    "    <property name=\"DataBasePath\" type=\"s\" access=\"read\" />"
    "    <property name=\"IsScanning\" type=\"b\" access=\"read\" />"
    "    <property name=\"WriteLocked\" type=\"b\" access=\"read\" />"
    "    <property name=\"UpdateID\" type=\"t\" access=\"read\" />"
    "    <property name=\"Categories\" type=\"a{sv}\" access=\"read\" />"
    "    <method name=\"Scan\">"
    "      <arg direction=\"in\" type=\"a{sv}\" name=\"specification\" />"
    "    </method>"
    "    <method name=\"Stop\" />"
    "    <method name=\"RequestWriteLock\" />"
    "    <method name=\"ReleaseWriteLock\" />"
    "    <signal name=\"ScanProgress\">"
    "      <arg type=\"s\" name=\"Category\" />"
    "      <arg type=\"s\" name=\"Path\" />"
    "      <arg type=\"t\" name=\"UpToDate\" />"
    "      <arg type=\"t\" name=\"Processed\" />"
    "      <arg type=\"t\" name=\"Deleted\" />"
    "      <arg type=\"t\" name=\"Skipped\" />"
    "      <arg type=\"t\" name=\"Errors\" />"
    "    </signal>"
    "  </interface>"
    "</node>";

typedef struct scanner_category
{
    char *category;
    GArray *parsers;
    GArray *dirs;
} scanner_category_t;

typedef struct scanner_pending
{
    char *category;
    GList *paths;
} scanner_pending_t;

typedef struct scan_progress {
    GDBusConnection *conn;
    gchar *category;
    gchar *path;
    guint64 uptodate;
    guint64 processed;
    guint64 deleted;
    guint64 skipped;
    guint64 errors;
    time_t last_report_time;
    gint updated;
} scan_progress_t;

/* Scan progress signals will be issued if the time since last
 * emission is greated than SCAN_PROGRESS_UPDATE_TIMEOUT _and_ number
 * of items is greater than the SCAN_PROGRESS_UPDATE_COUNT.
 *
 * Be warned that D-Bus signal will wake-up the dbus-daemon (unless
 * k-dbus) and all listener clients, which may hurt scan performance,
 * thus we keep these good enough for GUI to look responsive while
 * conservative to not hurt performance.
 *
 * Note that at after a path is scanned (check/progress) the signal is
 * emitted even if count or timeout didn't match.
 */
#define SCAN_PROGRESS_UPDATE_TIMEOUT 1 /* in seconds */
#define SCAN_PROGRESS_UPDATE_COUNT  50 /* in number of items */

#define SCAN_MOUNTPOINTS_TIMEOUT 1 /* in seconds */

typedef struct scanner {
    GDBusConnection *conn;
    char *write_lock;
    unsigned write_lock_name_watcher;
    GDBusMethodInvocation *pending_stop;
    GList *pending_scan; /* of scanner_pending_t, see scanner_thread_work */
    GThread *thread; /* see scanner_thread_work */
    unsigned cleanup_thread_idler; /* see scanner_thread_work */
    scan_progress_t *scan_progress;
    struct {
        GIOChannel *channel;
        unsigned watch;
        unsigned timer;
        GList *paths;
        GList *pending;
    } mounts;
    guint64 update_id;
    struct {
        unsigned idler; /* not a flag, but g_source tag */
        unsigned is_scanning : 1;
        unsigned write_locked : 1;
        unsigned update_id : 1;
        unsigned categories: 1;
    } changed_props;
} scanner_t;

static scanner_category_t *
scanner_category_new(const char *category)
{
    scanner_category_t *sc = g_new0(scanner_category_t, 1);

    sc->category = g_strdup(category);
    sc->parsers = g_array_new(TRUE, TRUE, sizeof(char *));
    sc->dirs = g_array_new(TRUE, TRUE, sizeof(char *));

    return sc;
}

static void
scanner_category_destroy(scanner_category_t *sc)
{
    char **itr;

    for (itr = (char **)sc->parsers->data; *itr != NULL; itr++)
        g_free(*itr);
    for (itr = (char **)sc->dirs->data; *itr != NULL; itr++)
        g_free(*itr);

    g_array_free(sc->parsers, TRUE);
    g_array_free(sc->dirs, TRUE);
    g_free(sc->category);

    g_free(sc);
}

static void scanner_release_write_lock(scanner_t *scanner);

static void
scanner_write_lock_vanished(GDBusConnection *conn, const char *name, gpointer data)
{
    scanner_t *scanner = data;
    g_warning("Write lock holder %s vanished, release lock\n", name);
    scanner_release_write_lock(scanner);
}

static guint64
get_update_id(void)
{
    const char sql[] = "SELECT version FROM lms_internal WHERE tab='update_id'";
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int ret;
    guint64 update_id = 0;

    ret = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);
    if (ret != SQLITE_OK) {
        g_warning("Couldn't open '%s': %s", db_path, sqlite3_errmsg(db));
        goto end;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("Couldn't get update_id from %s: %s",
                  db_path, sqlite3_errmsg(db));
        goto end;
    }

    ret = sqlite3_step(stmt);
    if (ret ==  SQLITE_DONE)
        update_id = 0;
    else if (ret == SQLITE_ROW)
        update_id = sqlite3_column_int(stmt, 0);
    else
        g_warning("Couldn't run SQL to get update_id, ret=%d: %s",
                  ret, sqlite3_errmsg(db));

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

end:
    sqlite3_close(db);

    g_debug("update id: %llu", (unsigned long long)update_id);
    return update_id;
}

static void
do_delete_old(void)
{
    const char sql[] = "DELETE FROM files WHERE dtime > 0 and dtime <= ?";
    sqlite3 *db;
    sqlite3_stmt *stmt;
    gint64 dtime;
    int ret;

    ret = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
    if (ret != SQLITE_OK) {
        g_warning("Couldn't open '%s': %s", db_path, sqlite3_errmsg(db));
        goto end;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("Couldn't prepare delete old from %s: %s",
                  db_path, sqlite3_errmsg(db));
        goto end;
    }

    dtime = (gint64)time(NULL) - delete_older_than * (24 * 60 * 60);
    if (sqlite3_bind_int64(stmt, 1, dtime) != SQLITE_OK) {
        g_warning("Couldn't bind delete old dtime '%"G_GINT64_FORMAT
                  "'from %s: %s", dtime, db_path, sqlite3_errmsg(db));
        goto cleanup;
    }

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE)
        g_warning("Couldn't run SQL delete old dtime '%"G_GINT64_FORMAT
                  "', ret=%d: %s", dtime, ret, sqlite3_errmsg(db));

cleanup:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

end:
    sqlite3_close(db);
}

static void
do_vacuum(void)
{
    const char sql[] = "VACUUM";
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int ret;

    ret = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
    if (ret != SQLITE_OK) {
        g_warning("Couldn't open '%s': %s", db_path, sqlite3_errmsg(db));
        goto end;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        g_warning("Couldn't vacuum from %s: %s",
                  db_path, sqlite3_errmsg(db));
        goto end;
    }

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE)
        g_warning("Couldn't run SQL VACUUM, ret=%d: %s",
                  ret, sqlite3_errmsg(db));

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

end:
    sqlite3_close(db);
}

static gboolean
check_write_locked(const scanner_t *scanner)
{
    return scanner->write_lock != NULL || scanner->thread != NULL;
}

static void
category_variant_foreach(gpointer key, gpointer value, gpointer user_data)
{
    scanner_category_t *sc = value;
    GVariantBuilder *builder = user_data;
    GVariantBuilder *sub, *darr, *parr;
    char **itr;

    darr = g_variant_builder_new(G_VARIANT_TYPE("as"));
    for (itr = (char **)sc->dirs->data; *itr != NULL; itr++)
        g_variant_builder_add(darr, "s", *itr);

    parr = g_variant_builder_new(G_VARIANT_TYPE("as"));
    for (itr = (char **)sc->parsers->data; *itr != NULL; itr++)
        g_variant_builder_add(parr, "s", *itr);

    sub = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(sub, "{sv}", "dirs", g_variant_builder_end(darr));
    g_variant_builder_add(sub, "{sv}", "parsers", g_variant_builder_end(parr));

    g_variant_builder_add(builder, "{sv}", sc->category,
                          g_variant_builder_end(sub));

    g_variant_builder_unref(sub);
    g_variant_builder_unref(parr);
    g_variant_builder_unref(darr);
}

static GVariant *
categories_get_variant(void)
{
    GVariantBuilder *builder;
    GVariant *variant;

    builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

    g_hash_table_foreach(categories, category_variant_foreach, builder);

    variant = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);

    return variant;
}

static gboolean
scanner_dbus_props_changed(gpointer data)
{
    GVariantBuilder *builder;
    GError *error = NULL;
    scanner_t *scanner = data;
    guint64 update_id = 0;

    if (!check_write_locked(scanner))
        update_id = get_update_id();
    if (update_id > 0 && update_id != scanner->update_id) {
        scanner->changed_props.update_id = TRUE;
        scanner->update_id = update_id;
    }

    builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

    if (scanner->changed_props.is_scanning) {
        scanner->changed_props.is_scanning = FALSE;
        g_variant_builder_add(builder, "{sv}", "IsScanning",
                              g_variant_new_boolean(scanner->thread != NULL));
    }
    if (scanner->changed_props.write_locked) {
        scanner->changed_props.write_locked = FALSE;
        g_variant_builder_add(
            builder, "{sv}", "WriteLocked",
            g_variant_new_boolean(check_write_locked(scanner)));
    }
    if (scanner->changed_props.update_id) {
        scanner->changed_props.update_id = FALSE;
        g_variant_builder_add(builder, "{sv}", "UpdateID",
                              g_variant_new_uint64(scanner->update_id));
    }
    if (scanner->changed_props.categories) {
        scanner->changed_props.categories = FALSE;
        g_variant_builder_add(builder, "{sv}", "Categories",
                              categories_get_variant());
    }

    g_dbus_connection_emit_signal(scanner->conn,
                                  NULL,
                                  BUS_PATH,
                                  "org.freedesktop.DBus.Properties",
                                  "PropertiesChanged",
                                  g_variant_new("(sa{sv}as)",
                                                BUS_IFACE, builder, NULL),
                                  &error);
    g_variant_builder_unref(builder);
    g_assert_no_error(error);

    scanner->changed_props.idler = 0;
    return FALSE;
}

static void
scanner_write_lock_changed(scanner_t *scanner)
{
    if (scanner->changed_props.idler == 0)
        scanner->changed_props.idler = g_idle_add(scanner_dbus_props_changed,
                                                  scanner);

    scanner->changed_props.write_locked = TRUE;
}

static void
scanner_acquire_write_lock(scanner_t *scanner, const char *sender)
{
    g_debug("acquired write lock for %s", sender);
    scanner->write_lock = g_strdup(sender);
    scanner->write_lock_name_watcher = g_bus_watch_name_on_connection(
        scanner->conn, sender, G_BUS_NAME_WATCHER_FLAGS_NONE,
        NULL, scanner_write_lock_vanished, scanner, NULL);

    scanner_write_lock_changed(scanner);
}

static void
scanner_release_write_lock(scanner_t *scanner)
{
    g_debug("release write lock previously owned by %s", scanner->write_lock);

    g_free(scanner->write_lock);
    scanner->write_lock = NULL;

    g_bus_unwatch_name(scanner->write_lock_name_watcher);
    scanner->write_lock_name_watcher = 0;

    scanner_write_lock_changed(scanner);
}

static void
scanner_pending_free(scanner_pending_t *pending)
{
    g_list_free_full(pending->paths, g_free);
    g_free(pending->category);
    g_free(pending);
}

static scanner_pending_t *
scanner_pending_get_or_add(scanner_t *scanner, const char *category)
{
    scanner_pending_t *pending;
    GList *n, *nlast = NULL;

    g_assert(scanner->thread == NULL);

    for (n = scanner->pending_scan; n != NULL; n = n->next) {
        nlast = n;
        pending = n->data;
        if (strcmp(pending->category, category) == 0)
            return pending;
    }

    pending = g_new0(scanner_pending_t, 1);
    pending->category = g_strdup(category);
    pending->paths = NULL;

    /* I can't believe there is no g_list_insert_after() :-( */
    n = g_list_alloc();
    n->data = pending;
    n->next = NULL;
    n->prev = nlast;

    if (nlast)
        nlast->next = n;
    else
        scanner->pending_scan = n;

    return pending;
}

/* NOTE:  assumes array was already validated for duplicates/restrictions */
static void
scanner_pending_add_all(scanner_pending_t *pending, const GArray *arr)
{
    char **itr;


    for (itr = (char **)arr->data; *itr != NULL; itr++)
        pending->paths = g_list_prepend(pending->paths, g_strdup(*itr));

    pending->paths = g_list_reverse(pending->paths);
}

static gboolean
scanner_category_allows_path(const GArray *restrictions, const char *path)
{
    const char * const *itr;
    for (itr = (const char *const*)restrictions->data; *itr != NULL; itr++) {
        if (g_str_has_prefix(path, *itr))
            return TRUE;
    }
    return FALSE;
}

static void
scanner_pending_add(scanner_pending_t *pending, const GArray *restrictions, const char *path)
{
    GList *n, *nlast;

    for (n = pending->paths; n != NULL; n = n->next) {
        const char *other = n->data;
        if (g_str_has_prefix(path, other)) {
            g_debug("Path already in pending scan in category %s: %s (%s)",
                    pending->category, path, other);
            return;
        }
    }


    if (restrictions && (!scanner_category_allows_path(restrictions, path))) {
        g_warning("Path is outside of category %s directories: %s",
                  pending->category, path);
        return;
    }

    nlast = NULL;
    for (n = pending->paths; n != NULL; ) {
        char *other = n->data;

        nlast = n;

        if (!g_str_has_prefix(other, path))
            n = n->next;
        else {
            GList *tmp;

            g_debug("Path covers previous pending scan in category %s, "
                    "replace %s (%s)",
                    pending->category, other, path);

            tmp = n->next;
            nlast = n->next ? n->next : n->prev;

            pending->paths = g_list_delete_link(pending->paths, n);
            g_free(other);

            n = tmp;
        }
    }

    g_debug("New scan path for category %s: %s", pending->category, path);

    /* I can't believe there is no g_list_insert_after() :-( */
    n = g_list_alloc();
    n->data = g_strdup(path);
    n->next = NULL;
    n->prev = nlast;

    if (nlast)
        nlast->next = n;
    else
        pending->paths = n;
}

static void
scan_params_all(gpointer key, gpointer value, gpointer user_data)
{
    scanner_pending_t *pending;
    scanner_category_t *sc = value;
    scanner_t *scanner = user_data;

    pending = scanner_pending_get_or_add(scanner, sc->category);
    scanner_pending_add_all(pending, sc->dirs);
}

static void
dbus_scanner_scan_params_set(scanner_t *scanner, GVariant *params)
{
    GVariantIter *itr;
    GVariant *el;
    char *cat;
    gboolean empty = TRUE;

    g_variant_get(params, "(a{sv})", &itr);
    while (g_variant_iter_loop(itr, "{sv}", &cat, &el)) {
        scanner_category_t *sc;
        scanner_pending_t *pending;
        GVariantIter *subitr;
        char *path;
        gboolean nodirs = TRUE;

        sc = g_hash_table_lookup(categories, cat);
        if (!sc) {
            g_warning("Unexpected scan category: %s, skipped.", cat);
            continue;
        }

        pending = scanner_pending_get_or_add(scanner, cat);
        empty = FALSE;

        g_variant_get(el, "as", &subitr);
        while (g_variant_iter_loop(subitr, "s", &path)) {
            scanner_pending_add(pending, sc->dirs, path);
            nodirs = FALSE;
        }

        if (nodirs)
            scanner_pending_add_all(pending, sc->dirs);
    }
    g_variant_iter_free(itr);

    if (empty)
        g_hash_table_foreach(categories, scan_params_all, scanner);
}

static void
scanner_is_scanning_changed(scanner_t *scanner)
{
    if (scanner->changed_props.idler == 0)
        scanner->changed_props.idler = g_idle_add(scanner_dbus_props_changed,
                                                  scanner);

    scanner->changed_props.is_scanning = TRUE;
}

static gboolean
report_scan_progress(gpointer data)
{
    scan_progress_t *sp = data;
    GError *error = NULL;

    g_dbus_connection_emit_signal(sp->conn,
                                  NULL,
                                  BUS_PATH,
                                  BUS_IFACE,
                                  "ScanProgress",
                                  g_variant_new("(ssttttt)",
                                                sp->category,
                                                sp->path,
                                                sp->uptodate,
                                                sp->processed,
                                                sp->deleted,
                                                sp->skipped,
                                                sp->errors),
                                  &error);
    g_assert_no_error(error);

    return FALSE;
}

static gboolean
report_scan_progress_and_free(gpointer data)
{
    scan_progress_t *sp = data;

    if (sp->updated)
        report_scan_progress(sp);

    g_object_unref(sp->conn);
    g_free(sp->category);
    g_free(sp->path);
    g_free(sp);
    return FALSE;
}

static void
scan_progress_cb(lms_t *lms, const char *path, int pathlen, lms_progress_status_t status, void *data)
{
    const scanner_t *scanner = data;
    scan_progress_t *sp = scanner->scan_progress;

    if (scanner->pending_stop)
        lms_stop_processing(lms);

    if (!sp)
        return;

    switch (status) {
    case LMS_PROGRESS_STATUS_UP_TO_DATE:
        sp->uptodate++;
        break;
    case LMS_PROGRESS_STATUS_PROCESSED:
        sp->processed++;
        break;
    case LMS_PROGRESS_STATUS_DELETED:
        sp->deleted++;
        break;
    case LMS_PROGRESS_STATUS_KILLED:
    case LMS_PROGRESS_STATUS_ERROR_PARSE:
    case LMS_PROGRESS_STATUS_ERROR_COMM:
        sp->errors++;
        break;
    case LMS_PROGRESS_STATUS_SKIPPED:
        sp->skipped++;
        break;
    }

    sp->updated++;
    if (sp->updated > SCAN_PROGRESS_UPDATE_COUNT) {
        time_t now = time(NULL);
        if (sp->last_report_time + SCAN_PROGRESS_UPDATE_TIMEOUT < now) {
            sp->last_report_time = now;
            sp->updated = 0;
            g_idle_add(report_scan_progress, sp);
        }
    }
}

static lms_t *
setup_lms(const char *category, const scanner_t *scanner)
{
    scanner_category_t *sc;
    char **itr;
    lms_t *lms;

    sc = g_hash_table_lookup(categories, category);
    if (!sc) {
        g_error("Unknown category %s", category);
        return NULL;
    }

    if (sc->parsers->len == 0) {
        g_warning("No parsers for category %s", category);
        return NULL;
    }

    lms = lms_new(db_path);
    if (!lms) {
        g_warning("Failed to create lms");
        return NULL;
    }

    lms_set_commit_interval(lms, commit_interval);
    lms_set_slave_timeout(lms, slave_timeout * 1000);

    if (charsets) {
        for (itr = charsets; *itr != NULL; itr++)
            if (lms_charset_add(lms, *itr) != 0)
                g_warning("Couldn't add charset: %s", *itr);
    }

    for (itr = (char **)sc->parsers->data; *itr != NULL; itr++) {
        const char *parser = *itr;
        lms_plugin_t *plugin;

        if (parser[0] == '/')
            plugin = lms_parser_add(lms, parser);
        else
            plugin = lms_parser_find_and_add(lms, parser);

        if (!plugin)
            g_warning("Couldn't add parser: %s", parser);
    }

    lms_set_progress_callback(lms, scan_progress_cb, scanner, NULL);

    return lms;
}

static void scan_mountpoints(scanner_t *scanner);

static gboolean
scanner_thread_cleanup(gpointer data)
{
    scanner_t *scanner = data;
    gpointer ret;

    g_debug("cleanup scanner work thread");
    ret = g_thread_join(scanner->thread);
    g_assert(ret == scanner);

    scanner->thread = NULL;
    scanner->cleanup_thread_idler = 0;

    if (scanner->pending_stop) {
        g_dbus_method_invocation_return_value(scanner->pending_stop, NULL);
        g_object_unref(scanner->pending_stop);
        scanner->pending_stop =  NULL;
    }

    g_list_free_full(scanner->pending_scan,
                     (GDestroyNotify)scanner_pending_free);
    scanner->pending_scan = NULL;

    if (scanner->mounts.pending && !scanner->mounts.timer)
        scan_mountpoints(scanner);
    else {
        scanner_is_scanning_changed(scanner);
        scanner_write_lock_changed(scanner);
    }

    return FALSE;
}

/*
 * Note on thread usage and locks (or lack of locks):
 *
 * The main thread is responsible for launching the worker thread,
 * setting 'scanner->thread' pointer, which is later checked *ONLY* by
 * main thread. When the thread is done, it will notify the main
 * thread with scanner_thread_cleanup() so it can unset the pointer
 * and do whatever it needs, so 'scanner->thread' is exclusively
 * managed by main thread.
 *
 * The other shared data 'scanner->pending_scan' is managed by the
 * main thread only when 'scanner->thread' is unset. If there is a
 * worker thread the main thread should never touch that list, thus
 * there is *NO NEED FOR LOCKS*.
 *
 * The thread will stop its work by checking 'scanner->pending_stop',
 * this is also done without a lock as there is no need for such thing
 * given above. The stop is also voluntary and it can happen on a
 * second iteration of work.
 */
static gpointer
scanner_thread_work(gpointer data)
{
    GList *lst;
    scanner_t *scanner = data;

    g_debug("started scanner thread");

    lst = scanner->pending_scan;
    scanner->pending_scan = NULL;
    while (lst) {
        scanner_pending_t *pending;
        lms_t *lms;

        if (scanner->pending_stop)
            break;

        pending = lst->data;
        lst = g_list_delete_link(lst, lst);

        g_debug("scan category: %s", pending->category);
        lms = setup_lms(pending->category, scanner);
        if (lms) {
            while (pending->paths) {
                char *path;
                scan_progress_t *sp = NULL;

                if (scanner->pending_stop)
                    break;

                path = pending->paths->data;
                pending->paths = g_list_delete_link(pending->paths,
                                                    pending->paths);

                g_debug("scan category %s, path %s", pending->category, path);

                if (!omit_scan_progress) {
                    sp = g_new0(scan_progress_t, 1);
                    sp->conn = g_object_ref(scanner->conn);
                    sp->category = g_strdup(pending->category);
                    sp->path = g_strdup(path);
                    scanner->scan_progress = sp;
                }

                if (!scanner->pending_stop)
                    lms_check(lms, path);
                if (!scanner->pending_stop &&
                    g_file_test(path, G_FILE_TEST_EXISTS))
                    lms_process(lms, path);

                if (sp)
                    g_idle_add(report_scan_progress_and_free, sp);

                g_free(path);
            }
            lms_free(lms);
        }

        scanner_pending_free(pending);
    }

    g_debug("finished scanner thread");

    if (delete_older_than >= 0) {
        g_debug("Delete from DB files with dtime older than %d days.",
                delete_older_than);
        do_delete_old();
    }

    if (vacuum) {
        GTimer *timer = g_timer_new();

        g_debug("Starting SQL VACUUM...");
        g_timer_start(timer);
        do_vacuum();
        g_timer_stop(timer);
        g_debug("Finished VACUUM in %0.3f seconds.",
                g_timer_elapsed(timer, NULL));
        g_timer_destroy(timer);
    }

    scanner->cleanup_thread_idler = g_idle_add(scanner_thread_cleanup, scanner);

    return scanner;
}

static void
do_scan(scanner_t *scanner)
{
    scanner->thread = g_thread_new("scanner", scanner_thread_work, scanner);

    scanner_is_scanning_changed(scanner);
    scanner_write_lock_changed(scanner);
}

static void
dbus_scanner_scan(GDBusMethodInvocation *inv, scanner_t *scanner, GVariant *params)
{
    if (scanner->thread) {
        g_dbus_method_invocation_return_dbus_error(
            inv, "org.lightmediascanner.AlreadyScanning",
            "Scanner was already scanning.");
        return;
    }

    if (scanner->write_lock) {
        g_dbus_method_invocation_return_dbus_error(
            inv, "org.lightmediascanner.WriteLocked",
            "Data Base has a write lock for another process.");
        return;
    }

    dbus_scanner_scan_params_set(scanner, params);

    do_scan(scanner);

    g_dbus_method_invocation_return_value(inv, NULL);
}

static void
dbus_scanner_stop(GDBusMethodInvocation *inv, scanner_t *scanner)
{
    if (!scanner->thread) {
        g_dbus_method_invocation_return_dbus_error(
            inv, "org.lightmediascanner.NotScanning",
            "Scanner was already stopped.");
        return;
    }
    if (scanner->pending_stop) {
        g_dbus_method_invocation_return_dbus_error(
            inv, "org.lightmediascanner.AlreadyStopping",
            "Scanner was already being stopped.");
        return;
    }

    scanner->pending_stop = g_object_ref(inv);
}

static void
dbus_scanner_request_write_lock(GDBusMethodInvocation *inv, scanner_t *scanner, const char *sender)
{
    if (check_write_locked(scanner)) {
        if (scanner->write_lock && strcmp(scanner->write_lock, sender) == 0)
            g_dbus_method_invocation_return_value(inv, NULL);
        else if (scanner->write_lock)
            g_dbus_method_invocation_return_dbus_error(
                inv, "org.lightmediascanner.AlreadyLocked",
                "Scanner is already locked");
        else
            g_dbus_method_invocation_return_dbus_error(
                inv, "org.lightmediascanner.IsScanning",
                "Scanner is scanning and can't grant a write lock");
        return;
    }

    scanner_acquire_write_lock(scanner, sender);
    g_dbus_method_invocation_return_value(inv, NULL);
}

static void
dbus_scanner_release_write_lock(GDBusMethodInvocation *inv, scanner_t *scanner, const char *sender)
{
    if (!scanner->write_lock || strcmp(scanner->write_lock, sender) != 0) {
        g_dbus_method_invocation_return_dbus_error(
            inv, "org.lightmediascanner.NotLocked",
            "Scanner was not locked by you.");
        return;
    }

    scanner_release_write_lock(scanner);
    g_dbus_method_invocation_return_value(inv, NULL);
}

static void
scanner_method_call(GDBusConnection *conn, const char *sender, const char *opath, const char *iface, const char *method, GVariant *params, GDBusMethodInvocation *inv, gpointer data)
{
    scanner_t *scanner = data;

    if (strcmp(method, "Scan") == 0)
        dbus_scanner_scan(inv, scanner, params);
    else if (strcmp(method, "Stop") == 0)
        dbus_scanner_stop(inv, scanner);
    else if (strcmp(method, "RequestWriteLock") == 0)
        dbus_scanner_request_write_lock(inv, scanner, sender);
    else if (strcmp(method, "ReleaseWriteLock") == 0)
        dbus_scanner_release_write_lock(inv, scanner, sender);
}

static void
scanner_update_id_changed(scanner_t *scanner)
{
    if (scanner->changed_props.idler == 0)
        scanner->changed_props.idler = g_idle_add(scanner_dbus_props_changed,
                                                  scanner);

    scanner->changed_props.update_id = TRUE;
}

static void
scanner_categories_changed(scanner_t *scanner)
{
    if (scanner->changed_props.idler == 0)
        scanner->changed_props.idler = g_idle_add(scanner_dbus_props_changed,
                                                  scanner);

    scanner->changed_props.categories = TRUE;
}

static GVariant *
scanner_get_prop(GDBusConnection *conn, const char *sender, const char *opath, const char *iface, const char *prop,  GError **error, gpointer data)
{
    scanner_t *scanner = data;
    GVariant *ret;

    if (strcmp(prop, "DataBasePath") == 0)
        ret = g_variant_new_string(db_path);
    else if (strcmp(prop, "IsScanning") == 0)
        ret = g_variant_new_boolean(scanner->thread != NULL);
    else if (strcmp(prop, "WriteLocked") == 0)
        ret = g_variant_new_boolean(check_write_locked(scanner));
    else if (strcmp(prop, "UpdateID") == 0) {
        guint64 update_id = 0;

        if (!check_write_locked(scanner))
            update_id = get_update_id();
        if (update_id > 0 && update_id != scanner->update_id) {
            scanner->update_id = update_id;
            scanner_update_id_changed(scanner);
        }
        ret = g_variant_new_uint64(scanner->update_id);
    } else if (strcmp(prop, "Categories") == 0)
        ret = categories_get_variant();
    else
        ret = NULL;

    return ret;
}

struct scanner_should_monitor_data {
    const char *mountpoint;
    gboolean should_monitor;
};

static void
category_should_monitor(gpointer key, gpointer value, gpointer user_data)
{
    const scanner_category_t *sc = value;
    struct scanner_should_monitor_data *data = user_data;

    if (data->should_monitor)
        return;

    if (scanner_category_allows_path(sc->dirs, data->mountpoint)) {
        data->should_monitor = TRUE;
        return;
    }
}

static gboolean
should_monitor(const char *mountpoint)
{
    struct scanner_should_monitor_data data = {mountpoint, FALSE};
    g_hash_table_foreach(categories, category_should_monitor, &data);
    return data.should_monitor;
}

static GList *
scanner_mounts_parse(scanner_t *scanner)
{
    GList *paths = NULL;
    GIOStatus status;
    GError *error = NULL;

    status = g_io_channel_seek_position(scanner->mounts.channel, 0, G_SEEK_SET,
                                        &error);
    if (error) {
        g_warning("Couldn't rewind mountinfo channel: %s", error->message);
        g_free(error);
        return NULL;
    }

    do {
        gchar *str = NULL;
        gsize len = 0;
        int mount_id, parent_id, major, minor;
        char root[1024], mountpoint[1024];

        status = g_io_channel_read_line(scanner->mounts.channel, &str, NULL,
                                        &len, &error);
        switch (status) {
        case G_IO_STATUS_AGAIN:
            continue;
        case G_IO_STATUS_NORMAL:
            str[len] = '\0';
            break;
        case G_IO_STATUS_ERROR:
            g_warning("Couldn't read line of mountinfo: %s", error->message);
            g_free(error);
        case G_IO_STATUS_EOF:
            return g_list_sort(paths, (GCompareFunc)strcmp);
        }

        if (sscanf(str, "%d %d %d:%d %1023s %1023s", &mount_id, &parent_id,
                   &major, &minor, root, mountpoint) != 6)
            g_warning("Error parsing mountinfo line: %s", str);
        else {
            char *mp = g_strcompress(mountpoint);

            if (!should_monitor(mp)) {
                g_debug("Ignored mountpoint: %s. Not in any category.", mp);
                g_free(mp);
            } else {
                g_debug("Got mountpoint: %s", mp);
                paths = g_list_prepend(paths, mp);
            }
        }

        g_free(str);
    } while (TRUE);
}

static void
scanner_mount_pending_add_or_delete(scanner_t *scanner, gchar *mountpoint)
{
    GList *itr;

    for (itr = scanner->mounts.pending; itr != NULL; itr = itr->next) {
        if (strcmp(itr->data, mountpoint) == 0) {
            g_free(mountpoint);
            return;
        }
    }

    scanner->mounts.pending = g_list_prepend(scanner->mounts.pending,
                                             mountpoint);
}

static void
category_scan_pending_mountpoints(gpointer key, gpointer value, gpointer user_data)
{
    scanner_pending_t *pending = NULL;
    scanner_category_t *sc = value;
    scanner_t *scanner = user_data;
    GList *n;

    for (n = scanner->mounts.pending; n != NULL; n = n->next) {
        const char *mountpoint = n->data;

        if (scanner_category_allows_path(sc->dirs, mountpoint)) {
            if (!pending)
                pending = scanner_pending_get_or_add(scanner, sc->category);

            scanner_pending_add(pending, NULL, mountpoint);
        }
    }
}

static void
scan_mountpoints(scanner_t *scanner)
{
    g_assert(scanner->thread == NULL);

    g_hash_table_foreach(categories, category_scan_pending_mountpoints,
                         scanner);
    g_list_free_full(scanner->mounts.pending, g_free);
    scanner->mounts.pending = NULL;

    do_scan(scanner);
}

static gboolean
on_scan_mountpoints_timeout(gpointer data)
{
    scanner_t *scanner = data;

    if (!scanner->thread)
        scan_mountpoints(scanner);

    scanner->mounts.timer = 0;
    return FALSE;
}

static gboolean
on_mounts_changed(GIOChannel *source, GIOCondition cond, gpointer data)
{
    scanner_t *scanner = data;
    GList *oldpaths = scanner->mounts.paths;
    GList *newpaths = scanner_mounts_parse(scanner);
    GList *o, *n;
    GList *current = NULL;

    for (o = oldpaths, n = newpaths; o != NULL && n != NULL;) {
        int r = strcmp(o->data, n->data);
        if (r == 0) {
            current = g_list_prepend(current, o->data);
            g_free(n->data);
            o = o->next;
            n = n->next;
        } else if (r < 0) { /* removed */
            scanner_mount_pending_add_or_delete(scanner, o->data);
            o = o->next;
        } else { /* added (goes to both pending and current) */
            current = g_list_prepend(current, g_strdup(n->data));
            scanner_mount_pending_add_or_delete(scanner, n->data);
            n = n->next;
        }
    }

    for (; o != NULL; o = o->next)
        scanner_mount_pending_add_or_delete(scanner, o->data);
    for (; n != NULL; n = n->next) {
        current = g_list_prepend(current, g_strdup(n->data));
        scanner_mount_pending_add_or_delete(scanner, n->data);
    }

    scanner->mounts.paths = g_list_reverse(current);

    g_list_free(oldpaths);
    g_list_free(newpaths);

    if (scanner->mounts.timer) {
        g_source_remove(scanner->mounts.timer);
        scanner->mounts.timer = 0;
    }
    if (scanner->mounts.pending) {
        scanner->mounts.timer = g_timeout_add(SCAN_MOUNTPOINTS_TIMEOUT * 1000,
                                              on_scan_mountpoints_timeout,
                                              scanner);
    }

    return TRUE;
}

static void
scanner_destroyed(gpointer data)
{
    scanner_t *scanner = data;

    g_free(scanner->write_lock);

    if (scanner->mounts.timer)
        g_source_remove(scanner->mounts.timer);
    if (scanner->mounts.watch)
        g_source_remove(scanner->mounts.watch);
    if (scanner->mounts.channel)
        g_io_channel_unref(scanner->mounts.channel);
    g_list_free_full(scanner->mounts.paths, g_free);
    g_list_free_full(scanner->mounts.pending, g_free);

    if (scanner->write_lock_name_watcher)
        g_bus_unwatch_name(scanner->write_lock_name_watcher);

    if (scanner->thread) {
        g_warning("Shutdown while scanning, wait...");
        g_thread_join(scanner->thread);
    }

    if (scanner->cleanup_thread_idler) {
        g_source_remove(scanner->cleanup_thread_idler);
        scanner_thread_cleanup(scanner);
    }

    if (scanner->changed_props.idler) {
        g_source_remove(scanner->changed_props.idler);
        scanner_dbus_props_changed(scanner);
    }

    g_assert(scanner->thread == NULL);
    g_assert(scanner->pending_scan == NULL);
    g_assert(scanner->cleanup_thread_idler == 0);
    g_assert(scanner->pending_stop == NULL);
    g_assert(scanner->changed_props.idler == 0);

    g_free(scanner);
}

static const GDBusInterfaceVTable scanner_vtable = {
    scanner_method_call,
    scanner_get_prop,
    NULL
};

static void
on_name_acquired(GDBusConnection *conn, const gchar *name, gpointer data)
{
    GDBusInterfaceInfo *iface;
    GError *error = NULL;
    unsigned id;
    scanner_t *scanner;

    scanner = g_new0(scanner_t, 1);
    g_assert(scanner != NULL);
    scanner->conn = conn;
    scanner->pending_scan = NULL;
    scanner->update_id = get_update_id();

    iface = g_dbus_node_info_lookup_interface(introspection_data, BUS_IFACE);

    id = g_dbus_connection_register_object(conn,
                                           BUS_PATH,
                                           iface,
                                           &scanner_vtable,
                                           scanner,
                                           scanner_destroyed,
                                           NULL);
    g_assert(id > 0);

    scanner->mounts.channel = g_io_channel_new_file("/proc/self/mountinfo",
                                                    "r", &error);
    if (error) {
      g_warning("No /proc/self/mountinfo file: %s. Disabled mount monitoring.",
                error->message);
      g_error_free(error);
    } else {
        scanner->mounts.watch = g_io_add_watch(scanner->mounts.channel,
                                               G_IO_ERR,
                                               on_mounts_changed, scanner);
        scanner->mounts.paths = scanner_mounts_parse(scanner);
    }

    if (startup_scan) {
        g_debug("Do startup scan");
        g_hash_table_foreach(categories, scan_params_all, scanner);
        do_scan(scanner);
    }

    scanner_update_id_changed(scanner);
    scanner_write_lock_changed(scanner);
    scanner_is_scanning_changed(scanner);
    scanner_categories_changed(scanner);

    g_debug("Acquired name org.lightmediascanner and registered object");
}

static gboolean
str_array_find(const GArray *arr, const char *str)
{
    char **itr;
    for (itr = (char **)arr->data; *itr; itr++)
        if (strcmp(*itr, str) == 0)
            return TRUE;

    return FALSE;
}

static scanner_category_t *
scanner_category_get_or_add(const char *category)
{
    scanner_category_t *sc = g_hash_table_lookup(categories, category);
    if (sc)
        return sc;

    sc = scanner_category_new(category);
    g_hash_table_insert(categories, sc->category, sc);
    return sc;
}

static void
scanner_category_add_parser(scanner_category_t *sc, const char *parser)
{
    char *p;

    if (str_array_find(sc->parsers, parser))
        return;

    p = g_strdup(parser);
    g_array_append_val(sc->parsers, p);
}

static void
scanner_category_add_dir(scanner_category_t *sc, const char *dir)
{
    char *p;

    if (str_array_find(sc->dirs, dir))
        return;

    p = g_strdup(dir);
    g_array_append_val(sc->dirs, p);
}

static int
populate_categories(void *data, const char *path)
{
    struct lms_parser_info *info;
    const char * const *itr;
    long do_parsers = (long)data;

    info = lms_parser_info(path);
    if (!info)
        return 1;

    if (strcmp(info->name, "dummy") == 0)
        goto end;

    if (!info->categories)
        goto end;


    for (itr = info->categories; *itr != NULL; itr++) {
        scanner_category_t *sc;

        if (strcmp(*itr, "all") == 0)
            continue;

        sc = scanner_category_get_or_add(*itr);

        if (do_parsers)
            scanner_category_add_parser(sc, path);
    }

end:
    lms_parser_info_free(info);

    return 1;
}

static int
populate_category_all_parsers(void *data, const char *path)
{
    struct lms_parser_info *info;
    scanner_category_t *sc = data;

    info = lms_parser_info(path);
    if (!info)
        return 1;

    if (strcmp(info->name, "dummy") != 0)
        scanner_category_add_parser(sc, path);

    lms_parser_info_free(info);
    return 1;
}

static int
populate_category_parsers(void *data, const char *path, const struct lms_parser_info *info)
{
    scanner_category_t *sc = data;

    if (strcmp(info->name, "dummy") != 0)
        scanner_category_add_parser(sc, path);

    return 1;
}

static void
_populate_parser_internal(const char *category, const char *parser)
{
    scanner_category_t *sc = scanner_category_get_or_add(category);

    if (strcmp(parser, "all") == 0)
        lms_parsers_list(populate_category_all_parsers, sc);
    else if (strcmp(parser, "all-category") == 0)
        lms_parsers_list_by_category(sc->category, populate_category_parsers,
                                     sc);
    else
        scanner_category_add_parser(sc, parser);
}

static void
populate_parser_foreach(gpointer key, gpointer value, gpointer user_data)
{
    const char *category = key;
    const char *parser = user_data;
    _populate_parser_internal(category, parser);
}

static void
populate_parser(const char *category, const char *parser)
{
    if (!category)
        g_hash_table_foreach(categories, populate_parser_foreach,
                             (gpointer)parser);
    else
        _populate_parser_internal(category, parser);
}

static void
_populate_dir_internal(const char *category, const char *dir)
{
    scanner_category_t *sc = scanner_category_get_or_add(category);

    if (strcmp(dir, "defaults") != 0)
        scanner_category_add_dir(sc, dir);
    else {
        struct {
            const char *cat;
            const char *path;
        } *itr, defaults[] = {
            {"audio", g_get_user_special_dir(G_USER_DIRECTORY_MUSIC)},
            {"video", g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS)},
            {"picture", g_get_user_special_dir(G_USER_DIRECTORY_PICTURES)},
            {"multimedia", g_get_user_special_dir(G_USER_DIRECTORY_MUSIC)},
            {"multimedia", g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS)},
            {"multimedia", g_get_user_special_dir(G_USER_DIRECTORY_PICTURES)},
            {NULL, NULL}
        };
        for (itr = defaults; itr->cat != NULL; itr++) {
            if (strcmp(itr->cat, category) == 0) {
                if (itr->path && itr->path[0] != '\0')
                    scanner_category_add_dir(sc, itr->path);
                else
                    g_warning("Requested default directories but "
                              "xdg-user-dirs is not setup. Category %s",
                              category);
            }
        }
    }
}

static void
populate_dir_foreach(gpointer key, gpointer value, gpointer user_data)
{
    const char *category = key;
    const char *dir = user_data;
    _populate_dir_internal(category, dir);
}

static void
populate_dir(const char *category, const char *dir)
{
    if (!category)
        g_hash_table_foreach(categories, populate_dir_foreach,
                             (gpointer)dir);
    else
        _populate_dir_internal(category, dir);
}

static void
debug_categories(gpointer key, gpointer value, gpointer user_data)
{
    const scanner_category_t *sc = value;
    const char * const *itr;

    g_debug("category: %s", sc->category);

    if (sc->parsers->len) {
        for (itr = (const char * const *)sc->parsers->data; *itr != NULL; itr++)
            g_debug("  parser: %s", *itr);
    } else
            g_debug("  parser: <none>");

    if (sc->dirs->len) {
        for (itr = (const char * const *)sc->dirs->data; *itr != NULL; itr++)
            g_debug("  dir...: %s", *itr);
    } else
            g_debug("  dir...: <none>");
}

static gboolean
on_sig_term(gpointer data)
{
    GMainLoop *loop = data;

    g_debug("got SIGTERM, exit.");
    g_main_loop_quit(loop);
    return FALSE;
}

static gboolean
on_sig_int(gpointer data)
{
    GMainLoop *loop = data;

    g_debug("got SIGINT, exit.");
    g_main_loop_quit(loop);
    return FALSE;
}

int
main(int argc, char *argv[])
{
    int ret = EXIT_SUCCESS;
    unsigned id;
    GMainLoop *loop;
    GError *error = NULL;
    GOptionContext *opt_ctx;
    char **parsers = NULL;
    char **dirs = NULL;
    GOptionEntry opt_entries[] = {
        {"db-path", 'p', 0, G_OPTION_ARG_FILENAME, &db_path,
         "Path to LightMediaScanner SQLit3 data base, "
         "defaults to \"~/.config/lightmediascannerd/db.sqlite3\".",
         "PATH"},
        {"commit-interval", 'c', 0, G_OPTION_ARG_INT, &commit_interval,
         "Execute SQL COMMIT after NUMBER files are processed, "
         "defaults to 100.",
         "NUMBER"},
        {"slave-timeout", 't', 0, G_OPTION_ARG_INT, &slave_timeout,
         "Number of seconds to wait for slave to reply, otherwise kills it. "
         "Defaults to 60.",
         "SECONDS"},
        {"delete-older-than", 'd', 0, G_OPTION_ARG_INT, &delete_older_than,
         "Delete from database files that have 'dtime' older than the given "
         "number of DAYS. If not specified LightMediaScanner will keep the "
         "files in the database even if they are gone from the file system "
         "and if they appear again and have the same 'mtime' and 'size' "
         "it will be restored ('dtime' set to 0) without the need to parse "
         "the file again (much faster). This is useful for removable media. "
         "Use a negative number to disable this behavior. "
         "Defaults to 30.",
         "DAYS"},
        {"vacuum", 'V', 0, G_OPTION_ARG_NONE, &vacuum,
         "Execute SQL VACUUM after every scan.", NULL},
        {"startup-scan", 'S', 0, G_OPTION_ARG_NONE, &startup_scan,
         "Execute full scan on startup.", NULL},
        {"omit-scan-progress", 0, 0, G_OPTION_ARG_NONE, &omit_scan_progress,
         "Omit the ScanProgress signal during scans. This will avoid the "
         "overhead of D-Bus signal emission and may slightly improve the "
         "performance, but will make the listener user-interfaces less "
         "responsive as they won't be able to tell the user what is happening.",
         NULL},
        {"charset", 'C', 0, G_OPTION_ARG_STRING_ARRAY, &charsets,
         "Extra charset to use. (Multiple use)", "CHARSET"},
        {"parser", 'P', 0, G_OPTION_ARG_STRING_ARRAY, &parsers,
         "Parsers to use, defaults to all. Format is 'category:parsername' or "
         "'parsername' to apply parser to all categories. The special "
         "parsername 'all' declares all known parsers, while 'all-category' "
         "declares all parsers of that category. If one parser is provided, "
         "then no defaults are used, you can pre-populate all categories "
         "with their parsers by using --parser=all-category.",
         "CATEGORY:PARSER"},
        {"directory", 'D', 0, G_OPTION_ARG_STRING_ARRAY, &dirs,
         "Directories to use, defaults to FreeDesktop.Org standard. "
         "Format is 'category:directory' or 'path' to "
         "apply directory to all categories. The special directory "
         "'defaults' declares all directories used by default for that "
         "category. If one directory is provided, then no defaults are used, "
         "you can pre-populate all categories with their directories by "
         "using --directory=defaults.",
         "CATEGORY:DIRECTORY"},
        {NULL, 0, 0, 0, NULL, NULL, NULL}
    };

    opt_ctx = g_option_context_new(
        "\nLightMediaScanner D-Bus daemon.\n\n"
        "Usually there is no need to declare options, defaults should "
        "be good. However one may want the defaults and in addition to scan "
        "everything in an USB with:\n\n"
        "\tlightmediascannerd --directory=defaults --parser=all-category "
        "--directory=usb:/media/usb --parser=usb:all");
    g_option_context_add_main_entries(opt_ctx, opt_entries,
                                      "lightmediascannerd");
    if (!g_option_context_parse(opt_ctx, &argc, &argv, &error)) {
        g_option_context_free(opt_ctx);
        g_error("Option parsing failed: %s\n", error->message);
        g_error_free(error);
        return EXIT_FAILURE;
    }

    g_option_context_free(opt_ctx);

    categories = g_hash_table_new_full(
        g_str_hash, g_str_equal, NULL,
        (GDestroyNotify)scanner_category_destroy);

    if (!db_path)
        db_path = g_strdup_printf("%s/lightmediascannerd/db.sqlite3",
                                  g_get_user_config_dir());
    if (!g_file_test(db_path, G_FILE_TEST_EXISTS)) {
        char *dname = g_path_get_dirname(db_path);
        if (!g_file_test(dname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
            if (g_mkdir_with_parents(dname, 0755) != 0) {
                g_error("Couldn't create directory %s", dname);
                g_free(dname);
                ret = EXIT_FAILURE;
                goto end_options;
            }
        }
        g_free(dname);
    }

    if (!parsers)
        lms_parsers_list(populate_categories, (void *)1L);
    else {
        char **itr;

        lms_parsers_list(populate_categories, (void *)0L);

        for (itr = parsers; *itr != NULL; itr++) {
            char *sep = strchr(*itr, ':');
            const char *path;
            if (!sep)
                path = *itr;
            else {
                path = sep + 1;
                *sep = '\0';
            }

            if (path[0] != '\0')
                populate_parser(sep ? *itr : NULL, path);

            if (sep)
                *sep = ':';
        }
    }

    if (!dirs)
        populate_dir(NULL, "defaults");
    else {
        char **itr;

        for (itr = dirs; *itr != NULL; itr++) {
            char *sep = strchr(*itr, ':');
            const char *path;
            if (!sep)
                path = *itr;
            else {
                path = sep + 1;
                *sep = '\0';
            }

            if (path[0] != '\0')
                populate_dir(sep ? *itr : NULL, path);

            if (sep)
                *sep = ':';
        }
    }

    g_debug("db-path: %s", db_path);
    g_debug("commit-interval: %d files", commit_interval);
    g_debug("slave-timeout: %d seconds", slave_timeout);
    g_debug("delete-older-than: %d days", delete_older_than);

    if (charsets) {
        char *tmp = g_strjoinv(", ", charsets);
        g_debug("charsets: %s", tmp);
        g_free(tmp);
    } else
        g_debug("charsets: <none>");

    g_hash_table_foreach(categories, debug_categories, NULL);

    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    g_assert(introspection_xml != NULL);

    id = g_bus_own_name(G_BUS_TYPE_SESSION, "org.lightmediascanner",
                        G_BUS_NAME_OWNER_FLAGS_NONE,
                        NULL, on_name_acquired, NULL, NULL, NULL);

    g_debug("starting main loop");

    loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGTERM, on_sig_term, loop);
    g_unix_signal_add(SIGINT, on_sig_int, loop);
    g_main_loop_run(loop);

    g_debug("main loop is finished");

    g_bus_unown_name(id);
    g_main_loop_unref(loop);

    g_dbus_node_info_unref(introspection_data);

end_options:
    g_free(db_path);
    g_strfreev(charsets);
    g_strfreev(parsers);
    g_strfreev(dirs);

    if (categories)
        g_hash_table_destroy(categories);

    return ret;
}
