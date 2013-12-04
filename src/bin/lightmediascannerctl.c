#include <gio/gio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct app {
    int ret;
    int argc;
    char **argv;
    void (*action)(struct app *app);
    GDBusProxy *proxy;
    GMainLoop *loop;
    GTimer *timer;
};

static void
print_server(GDBusProxy *proxy)
{
    char **props, **itr;
    char *nameowner;

    nameowner = g_dbus_proxy_get_name_owner(proxy);
    if (!nameowner) {
        puts("Server is not running.");
        return;
    }

    printf("Server at %s\n", nameowner);
    props = g_dbus_proxy_get_cached_property_names(proxy);
    if (!props)
        return;

    for (itr = props; *itr != NULL; itr++) {
        GVariant *value = g_dbus_proxy_get_cached_property(proxy, *itr);
        char *str = g_variant_print(value, TRUE);
        printf("\t%s = %s\n", *itr, str);
        g_variant_unref(value);
        g_free(str);
    }
    g_strfreev(props);
    g_free(nameowner);
}


static void
do_status(struct app *app)
{
    print_server(app->proxy);
    g_main_loop_quit(app->loop);
    app->ret = EXIT_SUCCESS;
}

static void
on_properties_changed(GDBusProxy *proxy, GVariant *changed, const char *const *invalidated, gpointer user_data)
{
    struct app *app = user_data;

    printf("%015.3f --- Properties Changed ---\n",
           g_timer_elapsed(app->timer, NULL));

    if (g_variant_n_children(changed) > 0) {
        GVariantIter *itr;
        const char *prop;
        GVariant *value;

        printf("Changed Properties:");
        g_variant_get(changed, "a{sv}", &itr);
        while (g_variant_iter_loop(itr, "{&sv}", &prop, &value)) {
            char *str;
            str = g_variant_print(value, TRUE);
            printf(" %s=%s", prop, str);
            g_free(str);
        }
        g_variant_iter_free(itr);
        printf("\n");
    }

    if (invalidated[0] != NULL) {
        const char * const *itr;
        printf("Invalidated Properties:");
        for (itr = invalidated; *itr != NULL; itr++)
            printf(" %s", *itr);
        printf("\n");
    }

    print_server(proxy);
}

static gboolean
do_delayed_print_server(gpointer data)
{
    GDBusProxy *proxy = data;
    char **props;
    char *nameowner;

    nameowner = g_dbus_proxy_get_name_owner(proxy);
    if (!nameowner) {
        print_server(proxy);
        return FALSE;
    }
    g_free(nameowner);

    props = g_dbus_proxy_get_cached_property_names(proxy);
    if (!props) {
        g_timeout_add(1000, do_delayed_print_server, proxy);
        return FALSE;
    }

    g_strfreev(props);
    print_server(data);
    return FALSE;
}

static void
on_name_owner_notify(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GDBusProxy *proxy = G_DBUS_PROXY(object);
    struct app *app = user_data;

    printf("%015.3f --- Name Owner Changed ---\n",
           g_timer_elapsed(app->timer, NULL));
    do_delayed_print_server(proxy);
}

static void
do_monitor(struct app *app)
{
    app->timer = g_timer_new();
    g_timer_start(app->timer);

    print_server(app->proxy);
    g_signal_connect(app->proxy, "g-properties-changed",
                     G_CALLBACK(on_properties_changed),
                     app);
    g_signal_connect(app->proxy, "notify::g-name-owner",
                     G_CALLBACK(on_name_owner_notify),
                     app);
}

static void
on_properties_changed_check_lock(GDBusProxy *proxy, GVariant *changed, const char *const *invalidated, gpointer user_data)
{
    struct app *app = user_data;
    gboolean lost_lock = FALSE;

    if (g_variant_n_children(changed) > 0) {
        GVariantIter *itr;
        const char *prop;
        GVariant *value;

        g_variant_get(changed, "a{sv}", &itr);
        while (g_variant_iter_loop(itr, "{&sv}", &prop, &value)) {
            if (strcmp(prop, "WriteLocked") == 0) {
                if (!g_variant_get_boolean(value))
                    lost_lock = TRUE;
                break;
            }
        }
        g_variant_iter_free(itr);
    }

    if (invalidated[0] != NULL) {
        const char * const *itr;
        for (itr = invalidated; *itr != NULL; itr++) {
            if (strcmp(*itr, "WriteLocked") == 0) {
                lost_lock = TRUE;
                break;
            }
        }
    }

    if (lost_lock) {
        fputs("Lost lock, exit.\n", stderr);
        app->ret = EXIT_FAILURE;
        g_main_loop_quit(app->loop);
    }
}

static void
do_write_lock(struct app *app)
{
    GVariant *ret;
    GError *error = NULL;
    char *nameowner;

    nameowner = g_dbus_proxy_get_name_owner(app->proxy);
    if (!nameowner) {
        fputs("Server is not running, cannot get write lock!\n", stderr);
        app->ret = EXIT_FAILURE;
        g_main_loop_quit(app->loop);
        return;
    }

    printf("Server at %s, try to get write lock\n", nameowner);
    g_free(nameowner);

    g_signal_connect(app->proxy, "g-properties-changed",
                     G_CALLBACK(on_properties_changed_check_lock),
                     app);

    ret = g_dbus_proxy_call_sync(app->proxy, "RequestWriteLock",
                                 g_variant_new("()"),
                                 G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                                 &error);

    if (!ret) {
        fprintf(stderr, "Could not get write lock: %s\n", error->message);
        g_error_free(error);
        app->ret = EXIT_FAILURE;
        g_main_loop_quit(app->loop);
        return;
    }

    g_variant_unref(ret);
    puts("Got write lock, close program to release it.");
}

static void
on_properties_changed_check_scan(GDBusProxy *proxy, GVariant *changed, const char *const *invalidated, gpointer user_data)
{
    struct app *app = user_data;

    if (g_variant_n_children(changed) > 0) {
        GVariantIter *itr;
        const char *prop;
        GVariant *value;

        g_variant_get(changed, "a{sv}", &itr);
        while (g_variant_iter_loop(itr, "{&sv}", &prop, &value)) {
            if (strcmp(prop, "IsScanning") == 0) {
                if (g_variant_get_boolean(value) && !app->timer) {
                    app->timer = g_timer_new();
                    g_timer_start(app->timer);
                } else if (!g_variant_get_boolean(value) && app->timer) {
                    g_timer_stop(app->timer);
                    app->ret = EXIT_SUCCESS;
                    g_main_loop_quit(app->loop);
                }
                g_variant_unref(value);
                break;
            } else
                g_variant_unref(value);
        }
        g_variant_iter_free(itr);
    }

    if (invalidated[0] != NULL) {
        const char * const *itr;
        for (itr = invalidated; *itr != NULL; itr++) {
            if (strcmp(*itr, "IsScanning") == 0) {
                fputs("Lost server, exit.\n", stderr);
                app->ret = EXIT_FAILURE;
                g_main_loop_quit(app->loop);
                break;
            }
        }
    }
}

static void
populate_scan_params(gpointer key, gpointer value, gpointer user_data)
{
    const char *category = key;
    const GArray *paths = value;
    GVariantBuilder *builder = user_data;
    GVariantBuilder *sub;
    char **itr;

    sub = g_variant_builder_new(G_VARIANT_TYPE("as"));
    for (itr = (char **)paths->data; *itr != NULL; itr++)
        g_variant_builder_add(sub, "s", *itr);

    g_variant_builder_add(builder, "{sv}", category, g_variant_builder_end(sub));
    g_variant_builder_unref(sub);
}

static void
do_free_array(gpointer data)
{
    g_array_free(data, TRUE);
}

static void
do_scan(struct app *app)
{
    GVariantBuilder *builder;
    GVariant *ret;
    GError *error = NULL;
    char *nameowner;
    int i;
    GHashTable *categories;

    nameowner = g_dbus_proxy_get_name_owner(app->proxy);
    if (!nameowner) {
        fputs("Server is not running, cannot start scan!\n", stderr);
        app->ret = EXIT_FAILURE;
        g_main_loop_quit(app->loop);
        return;
    }

    printf("Server at %s, try to start scan\n", nameowner);
    g_free(nameowner);

    g_signal_connect(app->proxy, "g-properties-changed",
                     G_CALLBACK(on_properties_changed_check_scan),
                     app);

    categories = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, do_free_array);
    for (i = 0; i < app->argc; i++) {
        GArray *arr;
        char *arg = app->argv[i];
        char *sep = strchr(arg, ':');
        char *path;


        if (!sep) {
            fprintf(stderr, "Ignored scan parameter: invalid format '%s'\n",
                    arg);
            continue;
        }

        *sep = '\0';
        path = sep + 1;

        arr = g_hash_table_lookup(categories, arg);
        if (!arr) {
            arr = g_array_new(TRUE, FALSE, sizeof(char *));
            g_hash_table_insert(categories, arg, arr);
        }

        if (path[0])
            g_array_append_val(arr, path);
    }

    builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_hash_table_foreach(categories, populate_scan_params, builder);

    ret = g_dbus_proxy_call_sync(app->proxy, "Scan",
                                 g_variant_new("(a{sv})", builder),
                                 G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                                 &error);
    g_variant_builder_unref(builder);
    g_hash_table_destroy(categories);

    if (!ret) {
        fprintf(stderr, "Could not start scan: %s\n", error->message);
        g_error_free(error);
        app->ret = EXIT_FAILURE;
        g_main_loop_quit(app->loop);
        return;
    }

    g_variant_unref(ret);
}

static void
do_stop(struct app *app)
{
    GVariant *ret;
    GError *error = NULL;
    char *nameowner;

    nameowner = g_dbus_proxy_get_name_owner(app->proxy);
    if (!nameowner) {
        fputs("Server is not running, cannot stop scan!\n", stderr);
        app->ret = EXIT_FAILURE;
        g_main_loop_quit(app->loop);
        return;
    }

    printf("Server at %s, try to stop scan\n", nameowner);
    g_free(nameowner);

    ret = g_dbus_proxy_call_sync(app->proxy, "Stop",
                                 g_variant_new("()"),
                                 G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                                 &error);

    if (!ret) {
        fprintf(stderr, "Could not stop scan: %s\n", error->message);
        g_error_free(error);
        app->ret = EXIT_FAILURE;
        g_main_loop_quit(app->loop);
        return;
    }

    app->ret = EXIT_SUCCESS;
    g_main_loop_quit(app->loop);
    g_variant_unref(ret);
}

static gboolean
do_action(gpointer data)
{
    struct app *app = data;
    app->action(app);
    return FALSE;
}

static void
print_help(const char *prog)
{
    printf("Usage:\n"
           "\t%s <action>\n"
           "\n"
           "Action is one of:\n"
           "\tstatus          print server properties and exit.\n"
           "\tmonitor         monitor server and its properties.\n"
           "\twrite-lock      try to get a write-lock and keep it while running.\n"
           "\tscan [params]   start scan. May receive parameters as a series of \n"
           "\t                CATEGORY:PATH to limit scan.\n"
           "\tstop            stop ongoing scan.\n"
           "\thelp            this message.\n"
           "\n",
           prog);
}

int
main(int argc, char *argv[])
{
    GError *error = NULL;
    struct app app;
    int i;

    if (argc < 2) {
        fprintf(stderr, "Missing action, see --help.\n");
        return EXIT_FAILURE;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    if (strcmp(argv[1], "status") == 0)
        app.action = do_status;
    else if (strcmp(argv[1], "monitor") == 0)
        app.action = do_monitor;
    else if (strcmp(argv[1], "write-lock") == 0)
        app.action = do_write_lock;
    else if (strcmp(argv[1], "scan") == 0)
        app.action = do_scan;
    else if (strcmp(argv[1], "stop") == 0)
        app.action = do_stop;
    else if (strcmp(argv[1], "help") == 0) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Unknown action '%s', see --help.\n", argv[1]);
        return EXIT_FAILURE;
    }

    app.timer = NULL;
    app.loop = g_main_loop_new(NULL, FALSE);
    app.proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              NULL,
                                              "org.lightmediascanner",
                                              "/org/lightmediascanner/Scanner1",
                                              "org.lightmediascanner.Scanner1",
                                              NULL,
                                              &error);
    if (error) {
        g_error("Could not create proxy: %s", error->message);
        g_error_free(error);
        return EXIT_FAILURE;
    }

    app.argc = argc - 2;
    app.argv = argv + 2;
    app.ret = EXIT_SUCCESS;

    g_idle_add(do_action, &app);

    g_main_loop_run(app.loop);
    g_object_unref(app.proxy);
    g_main_loop_unref(app.loop);

    if (app.timer) {
        printf("Elapsed time: %0.3f seconds\n",
               g_timer_elapsed(app.timer, NULL));
        g_timer_destroy(app.timer);
    }

    return app.ret;
}
