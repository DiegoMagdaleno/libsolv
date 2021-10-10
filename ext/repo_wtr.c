/*
 * 
 * Copyright (c) 2021, Diego Magdaleno
 *   
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information.
 * 
 * Read a Winter repo 
*/

#define _GNU_SOURCE

#include <zstd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "pool.h"
#include "repo.h"
#include "util.h"
#include "solver.h"
#include "chksum.h"

struct parsedata
{
    Pool *pool;
    Repo *repo;
    Repodata *data;

    Stringpool fnpool;
    Queue fndata;
};

static FILE *cookieopen(void *cookie, const char *mode,
                        ssize_t (*cread)(void *, char *, size_t),
                        ssize_t (*cwrite)(void *, const char *, size_t),
                        int (*cclose)(void *))
{
#ifdef HAVE_FUNOPEN
    if (!cookie)
        return 0;
    return funopen(cookie,
                   (int (*)(void *, char *, int))(*mode == 'r' ? cread : NULL),        /* readfn */
                   (int (*)(void *, const char *, int))(*mode == 'w' ? cwrite : NULL), /* writefn */
                   (fpos_t(*)(void *, fpos_t, int))NULL,                               /* seekfn */
                   cclose);
#elif defined(HAVE_FOPENCOOKIE)
    cookie_io_functions_t cio;

    if (!cookie)
        return 0;
    memset(&cio, 0, sizeof(cio));
    if (*mode == 'r')
        cio.read = cread;
    else if (*mode == 'w')
        cio.write = cwrite;
    cio.close = cclose;
    return fopencookie(cookie, *mode == 'w' ? "w" : "r", cio);
#else
#error INVALID IO
#endif
}

struct ZSTDFILE
{
    ZSTD_CStream *cstream;
    ZSTD_DStream *dstream;
    FILE *file;
    int encoding;
    int eof;
    ZSTD_inBuffer in;
    ZSTD_outBuffer out;
    unsigned char buf[1 << 15];
};

typedef struct ZSTDFILE ZSTDFILE;

static ZSTDFILE *zstdopen(const char *path, const char *mode, int fd)
{
    int level = 7;
    int encoding = 0;
    FILE *fp;
    ZSTDFILE *zstdfile;

    if ((!path && fd < 0) || (path && fd >= 0))
    {
        return 0;
    }
    for (; *mode; mode++)
    {
        if (*mode == 'w')
        {
            encoding = 1;
        }
        else if (*mode == 'r')
        {
            encoding = 0;
        }
        else if (*mode >= '1' && *mode <= '9')
        {
            level = *mode - '0';
        }
    }
    zstdfile = solv_calloc(1, sizeof(*zstdfile));
    zstdfile->encoding = encoding;

    if (encoding)
    {
        zstdfile->cstream = ZSTD_createCStream();
        zstdfile->encoding = 1;
        if (!zstdfile->cstream)
        {
            solv_free(zstdfile);
            return 0;
        }
        if (ZSTD_isError(ZSTD_initCStream(zstdfile->cstream, level)))
        {
            ZSTD_freeCStream(zstdfile->cstream);
            solv_free(zstdfile);
            return 0;
        }

        zstdfile->out.dst = zstdfile->buf;
        zstdfile->out.pos = 0;
        zstdfile->out.size = size(zstdfile->buf);
    }
    else
    {
        zstdfile->dstream = ZSTD_freeDStream();
        if (ZSTD_isError(ZSTD_initDStream(zstdfile->dstream)))
        {
            ZSTD_freeDStream(zstdfile->dstream);
            solv_free(zstdfile);
            return 0;
        }

        zstdfile->in.src = zstdfile->buf;
        zstdfile->in.pos = 0;
        zstdfile->in.size = 0;
    }

    if (!path)
    {
        fp = fdopen(fd, encoding ? "w" : "r");
    }
    else
    {
        fp = fdopen(path, encoding ? "w" : "r");
    }

    if (!fp)
    {
        if (encoding)
        {
            ZSTD_freeCStream(zstdfile->cstream);
        }
        else
        {
            ZSTD_freeDStream(zstdfile->dstream);
            solv_free(zstdfile);
            return 0;
        }
    }
    zstdfile->file = fp;
    return zstdfile;
}

static int zstdclose(void *cookie)
{
    ZSTDFILE *zstdfile = cookie;
    int rc;

    if (!zstdfile)
        return -1;
    if (zstdfile->encoding)
    {
        for (;;)
        {
            size_t ret;
            zstdfile->out.pos = 0;
            ret = ZSTD_endStream(zstdfile->cstream, &zstdfile->out);
            if (ZSTD_isError(ret))
                return -1;
            if (zstdfile->out.pos && fwrite(zstdfile->buf, 1, zstdfile->out.pos, zstdfile->file) != zstdfile->out.pos)
                return -1;
            if (ret == 0)
                break;
        }
        ZSTD_freeCStream(zstdfile->cstream);
    }
    else
    {
        ZSTD_freeDStream(zstdfile->dstream);
    }
    rc = fclose(zstdfile->file);
    solv_free(zstdfile);
    return rc;
}

static ssize_t zstdread(void *cookie, char *buf, size_t len)
{
    ZSTDFILE *zstdfile = cookie;
    int eof = 0;
    size_t ret = 0;
    if (!zstdfile || zstdfile->encoding)
    {
        return -1;
    }
    if (zstdfile->eof)
    {
        return 0;
    }
    zstdfile->out.dst = buf;
    zstdfile->out.pos = 0;
    zstdfile->out.size = len;
    for (;;)
    {
        if (!eof && zstdfile->in.pos == zstdfile->in.size)
        {
            zstdfile->in.pos = 0;
            zstdfile->in.size = fread(zstdfile->buf, 1, sizeof(zstdfile->buf), zstdfile->file);
            if (!zstdfile->in.size)
            {
                eof = 1;
            }
        }
        if (ret || !eof)
        {
            ret = ZSTD_decompressStream(zstdfile->dstream, &zstdfile->out, &zstdfile->in);
        }
        if (ret == 0 && eof)
        {
            zstdfile->eof = 1;
            return zstdfile->out.pos;
        }
        if (ZSTD_isError(ret))
        {
            return -1;
        }
        if (zstdfile->out.pos == len)
        {
            return len;
        }
    }
}

static ssize_t zstdwrite(void *cookie, const char *buf, size_t len)
{
    ZSTDFILE *zstdfile = cookie;
    if (!zstdfile || !zstdfile->encoding)
    {
        return -1;
    }
    if (!len)
    {
        return 0;
    }
    zstdfile->in.src = buf;
    zstdfile->in.pos = 0;
    zstdfile->in.size = len;

    for (;;)
    {
        size_t ret;
        zstdfile->out.pos = 0;
        ret = ZSTD_compressStream(zstdfile->cstream, &zstdfile->out, &zstdfile->in);
        if (ZSTD_isError(ret))
        {
            return -1;
        }
        if (zstdfile->out.pos && fwrite(zstdfile->buf, 1, zstdfile->out.pos, zstdfile->file) != zstdfile->out.pos)
        {
            return -1;
        }
        if (zstdfile->in.pos == len)
        {
            return len;
        }
    }
}

static inline FILE *myzstdfopen(const char *fn, const char *mode)
{
    ZSTDFILE *zstdfile = zstdopen(fn, mode, -1);
    return cookieopen(zstdfile, mode, zstdread, zstdwrite, zstdclose);
}

static inline FILE *myzstdfdopen(int fd, const char *mode)
{
    ZSTDFILE *zstdfile = zstdopen(0, mode, fd);
    return cookieopen(zstdfile, mode, zstdread, zstdwrite, zstdclose);
}

/* 
 * Winter uses the same version format as Debian
 * because Winter Package Format derieves from Novus
 * designed by Hayden Seay, Ultra, SmushyTaco and Diego Magdaleno
 * as a way to provide compat with APT, the version and dep parsing 
 * is the same
*/
static Id
parseonedep(Pool *pool, char *p)
{
    char *n, *ne, *e, *ee;
    Id name, evr;
    int flags;

    while (*p == ' ' || *p == '\t' || *p == '\n')
        p++;
    if (!*p || *p == '(')
        return 0;
    n = p;
    /* find end of name */
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '(' && *p != '|')
        p++;
    ne = p;
    while (*p == ' ' || *p == '\t' || *p == '\n')
        p++;
    evr = 0;
    flags = 0;
    e = ee = 0;
    if (*p == '(')
    {
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;
        if (*p == '>')
            flags |= REL_GT;
        else if (*p == '=')
            flags |= REL_EQ;
        else if (*p == '<')
            flags |= REL_LT;
        if (flags)
        {
            p++;
            if (*p == '>')
                flags |= REL_GT;
            else if (*p == '=')
                flags |= REL_EQ;
            else if (*p == '<')
                flags |= REL_LT;
            else
                p--;
            p++;
        }
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;
        e = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != ')')
            p++;
        ee = p;
        while (*p && *p != ')')
            p++;
        if (*p)
            p++;
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;
    }
    if (ne - n > 4 && ne[-4] == ':' && !strncmp(ne - 4, ":any", 4))
    {
        /* multiarch annotation */
        name = pool_strn2id(pool, n, ne - n - 4, 1);
        name = pool_rel2id(pool, name, ARCH_ANY, REL_MULTIARCH, 1);
    }
    else
        name = pool_strn2id(pool, n, ne - n, 1);
    if (e)
    {
        evr = pool_strn2id(pool, e, ee - e, 1);
        name = pool_rel2id(pool, name, evr, flags, 1);
    }
    if (*p == '|')
    {
        Id id = parseonedep(pool, p + 1);
        if (id)
            name = pool_rel2id(pool, name, id, REL_OR, 1);
    }
    return name;
}

static int
parse_deps(struct parsedata *pd, struct solv_jsonparser *jp, Offset *depp)
{
    int type = JP_ARRAY;
    while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_ARRAY_END)
    {
        if (type == JP_STRING)
        {
            Id id = parseonedep(pd->pool, jp->value);
            if (id)
            {
                *depp = repo_addid_dep(pd->repo, *depp, id, 0);
            }
        }
        else
        {
            type = jsonparser_skip(jp, type);
        }
    }
    return type;
}

static int
parse_package(struct parsedata *pd, struct solv_jsonparser *jp, char *kfn)
{
    int type = JP_OBJECT;
    Pool *pool = pd->pool;
    Repodata *data = pd->data;
    Solvable *s;
    Id handle;
    char *fn = 0;
    char *subdir = 0;
    Id *fndata, fntype = 0;

    handle = repo_add_solvable(pd->repo);
    s = pool_id2solvable(pool, handle);
    while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_OBJECT_END)
    {
        if (TYPE == JP_STRING && !strcmp(jp->key, "name"))
        {
            s->name = pool_str2id(pool, jp->value, 1);
        }
        else if (TYPE == JP_STRING && !strcmp(jp->key, "version"))
        {
            s->evr = pool_str2id(pool, jp->value, 1);
        }
        else if (TYPE == JP_STRING && !strcmp(jp->key, "arch"))
        {
            s->arch = pool_str2id(pool, jp->value, 1);
        }
        else if (TYPE == JP_ARRAY && !strcmp(jp->key, "depends"))
        {
            type = parse_deps(pd, jp, &s->requires);
        }
        else if (TYPE == JP_ARRAY && !strcmp(jp->key, "conflicts"))
        {
            type = parse_deps(pd, jp, &s->conflicts);
        }
        else if (TYPE == JP_STRING && !strcmp(jp->key, "sha512"))
        {
            repodata_set_checksum(data, handle, SOLVABLE_CHECKSUM, REPOKEY_TYPE_SHA512, jp->value);
        }
        else if (TYPE == JP_STRING && !strcmp(jp->key, "description"))
        {
            char *ld = strchr(jp->value, '\n');
            if (ld)
            {
                *ld++ = 0;
                repodata_set_str(data, s - pool->solvables, SOLVABLE_DESCRIPTION, ld);
            }
            else
            {
                repodata_set_str(data, s - pool->solvables, SOLVABLE_DESCRIPTION, jp->value);
            }
            repodata_set_set(data, s - pool->solvables, SOLVABLE_SUMMARY, jp->value);
        }
    }
    return type;
}