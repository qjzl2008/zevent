#ifndef BTPD_HASHTABLE_H
#define BTPD_HASHTABLE_H

struct htbl_iter {
    struct _htbl *tbl;
    size_t bi;
    size_t cnt;
    void **ptr;
    void *obj;
};

struct _htbl *_htbl_create(float ratio,
    int (*equal)(const void *, const void *),
    uint32_t (*hash)(const void *), size_t keyoff, size_t chainoff);
void _htbl_free(struct _htbl *tbl);
void _htbl_insert(struct _htbl *tbl, void *o);
void *_htbl_remove(struct _htbl *tbl, const void *key);
void *_htbl_find(struct _htbl *tbl, const void *key);
void _htbl_fillv(struct _htbl *tbl, void **v);
void **_htbl_tov(struct _htbl *tbl);
size_t _htbl_size(struct _htbl *tbl);
void *_htbl_iter_first(struct _htbl *tbl, struct htbl_iter *it);
void *_htbl_iter_next(struct htbl_iter *it);
void *_htbl_iter_del(struct htbl_iter *it);

#define HTBL_ENTRY(name) void *name

#define HTBL_TYPE(name, type, ktype, kname, cname) \
 static  struct name * \
name##_create(float ratio, int (*equal)(const void *, const void *), \
    uint32_t (*hash)(const void *)) \
{ \
    return (struct name *) \
        _htbl_create(ratio, equal, hash, offsetof(struct type, kname), \
            offsetof(struct type, cname)); \
} \
\
 static  struct type * \
name##_find(struct name *tbl, const ktype *key) \
{ \
    return (struct type *)_htbl_find((struct _htbl *)tbl, key); \
} \
\
 static  struct type * \
name##_remove(struct name *tbl, const ktype *key) \
{ \
    return (struct type *)_htbl_remove((struct _htbl *)tbl, key); \
} \
\
 static  void \
name##_free(struct name *tbl) \
{ \
    _htbl_free((struct _htbl *)tbl); \
} \
\
 static  void \
name##_insert(struct name *tbl, struct type *o) \
{ \
    _htbl_insert((struct _htbl *)tbl, (void *)o); \
} \
 static  struct type ** \
name##_tov(struct name *tbl) \
{ \
    return (struct type **) _htbl_tov((struct _htbl *)tbl); \
} \
 static  void \
name##_fillv(struct name *tbl, struct type **v) \
{ \
    _htbl_fillv((struct _htbl *)tbl, (void **)v); \
} \
\
 static  size_t \
name##_size(struct name *tbl) \
{ \
    return _htbl_size((struct _htbl *)tbl); \
} \
\
 static  struct type * \
name##_iter_first(struct name *tbl, struct htbl_iter *it) \
{ \
    return (struct type *)_htbl_iter_first((struct _htbl *)tbl, it); \
} \
\
 static  struct type * \
name##_iter_del(struct htbl_iter *it) \
{ \
    return (struct type *)_htbl_iter_del(it); \
} \
 static  struct type * \
name##_iter_next(struct htbl_iter *it) \
{ \
    return (struct type *)_htbl_iter_next(it); \
}

#endif
