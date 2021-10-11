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
#include "solv_jsonparser.h"
#include "repo_wtr.h"

struct parsedata
{
    Pool *pool;
    Repo *repo;
    Repodata *data;

    Stringpool fnpool;
    Queue fndata;
};


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

    handle = repo_add_solvable(pd->repo);
    s = pool_id2solvable(pool, handle);
    while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_OBJECT_END)
    {
        if (type == JP_STRING && !strcmp(jp->key, "name"))
        {
            s->name = pool_str2id(pool, jp->value, 1);
        }
        else if (type == JP_STRING && !strcmp(jp->key, "version"))
        {
            s->evr = pool_str2id(pool, jp->value, 1);
        }
        else if (type == JP_STRING && !strcmp(jp->key, "arch"))
        {
            s->arch = pool_str2id(pool, jp->value, 1);
        }
        else if (type == JP_ARRAY && !strcmp(jp->key, "depends"))
        {
            type = parse_deps(pd, jp, &s->requires);
        }
        else if (type == JP_ARRAY && !strcmp(jp->key, "conflicts"))
        {
            type = parse_deps(pd, jp, &s->conflicts);
        }
        else if (type == JP_STRING && !strcmp(jp->key, "sha512"))
        {
            repodata_set_checksum(data, handle, SOLVABLE_CHECKSUM, REPOKEY_TYPE_SHA512, jp->value);
        }
        else if (type == JP_STRING && !strcmp(jp->key, "description"))
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
            repodata_set_str(data, s - pool->solvables, SOLVABLE_SUMMARY, jp->value);
        }
        else if (type == JP_STRING && !strcmp(jp->key, "filename"))
        {
            repodata_set_location(data, s - pool->solvables, 0, 0, jp->value);
        }
    }
    return type;
}

static int parse_packages(struct parsedata *pd, struct solv_jsonparser *jp)
{
    int type = JP_ARRAY;
    while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_ARRAY_END)
    {
        if (type == JP_OBJECT)
        {
            char *fn = solv_strdup(jp->key);
            type = parse_package(pd, jp, fn);
            solv_free(fn);
        }
        else
        {
            type = jsonparser_skip(jp, type);
        }
    }
    return type;
}

/*
 * We need to research if we are losing any significant speed when doing:
 * binencode -> JSON -> Temporary JSON file in the Rust side, in order
 * to parse it here and make it a solvable
*/
int repo_add_winter(Repo *repo, FILE *fp, int flags)
{
    Pool *pool = repo->pool;
    struct solv_jsonparser jp;
    struct parsedata pd;
    Repodata *data;
    int type, ret = 0;

    data = repo_add_repodata(repo, flags);

    memset(&pd, 0, sizeof(pd));
    pd.pool = pool;
    pd.repo = repo;
    pd.data = data;

    stringpool_init_empty(&pd.fnpool);
    queue_init(&pd.fndata);

    jsonparser_init(&jp, fp);
    if ((type = jsonparser_parse(&jp)) != JP_ARRAY)
    {
        ret = pool_error(pool, -1, "Repository is not an array of packages");
    }
    else if ((type = parse_packages(&pd, &jp)) != JP_ARRAY_END)
    {
        ret = pool_error(pool, -1, "Array format is corrupted %d", jp.line);
    }
    jsonparser_free(&jp);

    queue_free(&pd.fndata);
    stringpool_free(&pd.fnpool);
    if (!(flags & REPO_NO_INTERNALIZE))
    {
        repodata_internalize(data);
    }

    return ret;
}