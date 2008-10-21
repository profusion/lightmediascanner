#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lightmediascanner.h>

static void
usage(const char *prgname)
{
    fprintf(stderr,
            "Usage:\n"
            "\t%s [category]\n"
            "no category means all parsers.\n",
            prgname);
}

static void
print_array(const char * const *array)
{
    if (!array)
        return;

    for (; *array != NULL; array++)
        printf("\t\t%s\n", *array);
}

static void
print_parser(const char *path, const struct lms_parser_info *info)
{
    printf("parser: %s", path);

    if (!info) {
        fputs(" --- no information\n", stdout);
        return;
    }

    fputc('\n', stdout);
    printf("\tname.......: %s\n", info->name);
    printf("\tcategories.:\n");
    print_array(info->categories);
    printf("\tdescription: %s\n", info->description);
    printf("\tversion....: %s\n", info->version);
    printf("\tauthors....:\n");
    print_array(info->authors);
    printf("\turi........: %s\n", info->uri);
    fputc('\n', stdout);
}

static int
list_by_category(void *data, const char *path, const struct lms_parser_info *info)
{
    print_parser(path, info);
    return 1;
}

static int
list_parser(void *data, const char *path)
{
    struct lms_parser_info *info;

    info = lms_parser_info(path);
    print_parser(path, info);
    lms_parser_info_free(info);
    return 1;
}

int
main(int argc, char *argv[])
{
    int i;
    char *category = NULL;

    for (i = 1; i < argc; i++)
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-h") == 0 ||
                strcmp(argv[i], "--help") == 0) {
                usage(argv[0]);
                return 1;
            } else {
                fprintf(stderr, "ERROR: unknown option %s\n", argv[i]);
                return 1;
            }
        } else
            category = argv[i];

    if (category)
        lms_parsers_list_by_category(category, list_by_category, NULL);
    else
        lms_parsers_list(list_parser, NULL);
    return 0;
}
