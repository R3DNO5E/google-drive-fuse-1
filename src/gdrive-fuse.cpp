#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <iostream>

#include "gdrive.cpp"

#define ACCESS_TOKEN "TOKEN"
static GDrive* g;

static struct options {
    int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
        OPTION("-h", show_help),
        OPTION("--help", show_help),
        FUSE_OPT_END
};

static void *gdrive_fuse_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    puts("init");
    (void) conn;
    cfg->kernel_cache = 1;
    g = new GDrive(ACCESS_TOKEN);
    return NULL;
}

static int gdrive_fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    puts("getattr");
    (void) fi;
    int res = 0;
    puts(path);

    memset(stbuf, 0, sizeof(struct stat));
    if (g->isdir(path)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (g->isfile(path)) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = g->getsize(path);
    } else {
        res = -ENOENT;
    }
    return res;
}

static int
gdrive_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags) {
    puts("readdir");
    puts(path);
    (void) offset;
    (void) fi;
    (void) flags;

    if(!g->isdir(path)){
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, (enum fuse_fill_dir_flags) 0);
    filler(buf, "..", NULL, 0, (enum fuse_fill_dir_flags) 0);
    auto t = g->readdir(path);
    for (auto &e: t.first) {
        filler(buf, e.c_str(), NULL, 0, (enum fuse_fill_dir_flags) 0);
    }
    for (auto &e: t.second) {
        filler(buf, e.c_str(), NULL, 0, (enum fuse_fill_dir_flags) 0);
    }
    return 0;
}

static int gdrive_fuse_open(const char *path, struct fuse_file_info *fi) {
    puts("open");
    if (!g->isfile(path)) {
        return -ENOENT;
    }
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int gdrive_fuse_read(const char *path, char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi) {
    puts("read");
    size_t len;
    (void) fi;

    if (!g->isfile(path)) {
        return -ENOENT;
    }

    len = g->getsize(path);
    if (offset < len) {
        if (offset + size > len) {
            size = len - offset;
        }
        auto c = g->getFile(path);
        for (size_t i = 0; i < size; i++) {
            buf[i] = c[i + offset];
        }
    } else
        size = 0;

    return size;
}

static const struct fuse_operations gdrive_fuse_oper = {
        .getattr    = gdrive_fuse_getattr,
        .open        = gdrive_fuse_open,
        .read        = gdrive_fuse_read,
        .readdir    = gdrive_fuse_readdir,
        .init           = gdrive_fuse_init,
};

static void show_help(const char *progname) {
    printf("usage: %s [options] <mountpoint>\n\n", progname);
    printf("File-system specific options:\n"
           "\n");
}

int main(int argc, char *argv[]) {
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
        return 1;

    if (options.show_help) {
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0][0] = '\0';
    }

    ret = fuse_main(args.argc, args.argv, &gdrive_fuse_oper, NULL);
    fuse_opt_free_args(&args);
    return ret;
}