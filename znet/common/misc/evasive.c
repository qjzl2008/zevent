
#define DEFAULT_HASH_TBL_SIZE   3097ul  // Default hash table size
#define DEFAULT_COUNT      2       // Default maximum hit count per interval
#define DEFAULT_INTERVAL   1       // Default 1 Second interval
#define DEFAULT_BLOCKING_PERIOD 10      // Default for Detected IPs; blocked for 10 seconds

/* END DoS Evasive Maneuvers Definitions */

/* BEGIN NTT (Named Timestamp Tree) Headers */

enum { ntt_num_primes = 28 };

/* ntt root tree */
struct ntt {
    long size;
    long items;
    struct ntt_node **tbl;
};

/* ntt node (entry in the ntt root tree) */
struct ntt_node {
    char *key;
    time_t timestamp;
    long count;
    struct ntt_node *next;
};

/* ntt cursor */
struct ntt_c {
  long iter_index;
  struct ntt_node *iter_next;
};

struct ntt *ntt_create(long size);
int ntt_destroy(struct ntt *ntt);
struct ntt_node	*ntt_find(struct ntt *ntt, const char *key);
struct ntt_node	*ntt_insert(struct ntt *ntt, const char *key, time_t timestamp);
int ntt_delete(struct ntt *ntt, const char *key);
long ntt_hashcode(struct ntt *ntt, const char *key);	
struct ntt_node *c_ntt_first(struct ntt *ntt, struct ntt_c *c);
struct ntt_node *c_ntt_next(struct ntt *ntt, struct ntt_c *c);

/* END NTT (Named Timestamp Tree) Headers */


/* BEGIN DoS Evasive Maneuvers Globals */

struct ntt *hit_list;	// Our dynamic hash table

static unsigned long hash_table_size = DEFAULT_HASH_TBL_SIZE;
static int count = DEFAULT_COUNT;
static int interval = DEFAULT_INTERVAL;
static int blocking_period = DEFAULT_BLOCKING_PERIOD;
static const char *setwhitelist(cmd_parms *cmd, void *dconfig, const char *ip);
int is_whitelisted(const char *ip);

/* END DoS Evasive Maneuvers Globals */

int create_hit_list() 
{
    /* Create a new hit list for this listener */
    hit_list = ntt_create(hash_table_size);
    return 0;
}

int setwhitelist(const char *ip)
{
  char entry[128];
  snprintf(entry, sizeof(entry), "WHITELIST_%s", ip);
  ntt_insert(hit_list, entry, time(NULL));
  return 0;
}


int access_checker(const char *ip) 
{
      char hash_key[2048];
      struct ntt_node *n;
      time_t t = time(NULL);

      /* Check whitelist */
      if (is_whitelisted(ip)) 
        return 0;

      /* First see if the IP itself is on "hold" */
      n = ntt_find(hit_list, ip);

      if (n != NULL && t-n->timestamp<blocking_period) {
        n->timestamp = time(NULL);
      /* Not on hold, check hit stats */
      } else {
        snprintf(hash_key, 2048, "%s", ip);
        n = ntt_find(hit_list, hash_key);
        if (n != NULL) {

          if (t-n->timestamp<interval && n->count>=count) {
            ntt_insert(hit_list, ip, time(NULL));
          } else {

            /* Reset our hit count list as necessary */
            if (t-n->timestamp>=interval) {
              n->count=0;
            }
          }
          n->timestamp = t;
          n->count++;
        } else {
          ntt_insert(hit_list, hash_key, t);
        }
      }

    return 0;
}

int is_whitelisted(const char *ip) {
  char hashkey[128];
  char octet[4][4];
  char *dip;
  char *oct;
  int i = 0;

  memset(octet, 0, 16);
  dip = strdup(ip);
  if (dip == NULL)
    return 0;

  oct = strtok(dip, ".");
  while(oct != NULL && i<4) {
    if (strlen(oct)<=3) 
      strcpy(octet[i], oct);
    i++;
    oct = strtok(NULL, ".");
  }
  free(dip);

  /* Exact Match */
  snprintf(hashkey, sizeof(hashkey), "WHITELIST_%s", ip); 
  if (ntt_find(hit_list, hashkey)!=NULL)
    return 1;

  /* IPv4 Wildcards */ 
  snprintf(hashkey, sizeof(hashkey), "WHITELIST_%s.*.*.*", octet[0]);
  if (ntt_find(hit_list, hashkey)!=NULL)
    return 1;

  snprintf(hashkey, sizeof(hashkey), "WHITELIST_%s.%s.*.*", octet[0], octet[1]);
  if (ntt_find(hit_list, hashkey)!=NULL)
    return 1;

  snprintf(hashkey, sizeof(hashkey), "WHITELIST_%s.%s.%s.*", octet[0], octet[1], octet[2]);
  if (ntt_find(hit_list, hashkey)!=NULL)
    return 1;

  /* No match */
  return 0;
}

int destroy_hit_list(void *not_used) {
  ntt_destroy(hit_list);
}


/* BEGIN NTT (Named Timestamp Tree) Functions */

static unsigned long ntt_prime_list[ntt_num_primes] = 
{
    53ul,         97ul,         193ul,       389ul,       769ul,
    1543ul,       3079ul,       6151ul,      12289ul,     24593ul,
    49157ul,      98317ul,      196613ul,    393241ul,    786433ul,
    1572869ul,    3145739ul,    6291469ul,   12582917ul,  25165843ul,
    50331653ul,   100663319ul,  201326611ul, 402653189ul, 805306457ul,
    1610612741ul, 3221225473ul, 4294967291ul
};


/* Find the numeric position in the hash table based on key and modulus */

long ntt_hashcode(struct ntt *ntt, const char *key) {
    unsigned long val = 0;
    for (; *key; ++key) val = 5 * val + *key;
    return(val % ntt->size);
}

/* Creates a single node in the tree */

struct ntt_node *ntt_node_create(const char *key) {
    char *node_key;
    struct ntt_node* node;

    node = (struct ntt_node *) malloc(sizeof(struct ntt_node));
    if (node == NULL) {
	return NULL;
    }
    if ((node_key = strdup(key)) == NULL) {
        free(node);
	return NULL;
    }
    node->key = node_key;
    node->timestamp = time(NULL);
    node->next = NULL;
    return(node);
}

/* Tree initializer */

struct ntt *ntt_create(long size) {
    long i = 0;
    struct ntt *ntt = (struct ntt *) malloc(sizeof(struct ntt));

    if (ntt == NULL)
        return NULL;
    while (ntt_prime_list[i] < size) { i++; }
    ntt->size  = ntt_prime_list[i];
    ntt->items = 0;
    ntt->tbl   = (struct ntt_node **) calloc(ntt->size, sizeof(struct ntt_node *));
    if (ntt->tbl == NULL) {
        free(ntt);
        return NULL;
    }
    return(ntt);
}

/* Find an object in the tree */

struct ntt_node *ntt_find(struct ntt *ntt, const char *key) {
    long hash_code;
    struct ntt_node *node;

    if (ntt == NULL) return NULL;

    hash_code = ntt_hashcode(ntt, key);
    node = ntt->tbl[hash_code];

    while (node) {
        if (!strcmp(key, node->key)) {
            return(node);
        }
        node = node->next;
    }
    return((struct ntt_node *)NULL);
}

/* Insert a node into the tree */

struct ntt_node *ntt_insert(struct ntt *ntt, const char *key, time_t timestamp) {
    long hash_code;
    struct ntt_node *parent;
    struct ntt_node *node;
    struct ntt_node *new_node = NULL;

    if (ntt == NULL) return NULL;

    hash_code = ntt_hashcode(ntt, key);
    parent	= NULL;
    node	= ntt->tbl[hash_code];

    while (node != NULL) {
        if (strcmp(key, node->key) == 0) { 
            new_node = node;
            node = NULL;
        }

	if (new_node == NULL) {
          parent = node;
          node = node->next;
        }
    }

    if (new_node != NULL) {
        new_node->timestamp = timestamp;
        new_node->count = 0;
        return new_node; 
    }

    /* Create a new node */
    new_node = ntt_node_create(key);
    new_node->timestamp = timestamp;
    new_node->timestamp = 0;

    ntt->items++;

    /* Insert */
    if (parent) {  /* Existing parent */
	parent->next = new_node;
        return new_node;  /* Return the locked node */
    }

    /* No existing parent; add directly to hash table */
    ntt->tbl[hash_code] = new_node;
    return new_node;
}

/* Tree destructor */

int ntt_destroy(struct ntt *ntt) {
    struct ntt_node *node, *next;
    struct ntt_c c;

    if (ntt == NULL) return -1;

    node = c_ntt_first(ntt, &c);
    while(node != NULL) {
        next = c_ntt_next(ntt, &c);
        ntt_delete(ntt, node->key);
        node = next;
    }

    free(ntt->tbl);
    free(ntt);
    ntt = (struct ntt *) NULL;

    return 0;
}

/* Delete a single node in the tree */

int ntt_delete(struct ntt *ntt, const char *key) {
    long hash_code;
    struct ntt_node *parent = NULL;
    struct ntt_node *node;
    struct ntt_node *del_node = NULL;

    if (ntt == NULL) return -1;

    hash_code = ntt_hashcode(ntt, key);
    node        = ntt->tbl[hash_code];

    while (node != NULL) {
        if (strcmp(key, node->key) == 0) {
            del_node = node;
            node = NULL;
        }

        if (del_node == NULL) {
          parent = node;
          node = node->next;
        }
    }

    if (del_node != NULL) {

        if (parent) {
            parent->next = del_node->next;
        } else {
            ntt->tbl[hash_code] = del_node->next;
        }

        free(del_node->key);
        free(del_node);
        ntt->items--;

        return 0;
    }

    return -5;
}

/* Point cursor to first item in tree */

struct ntt_node *c_ntt_first(struct ntt *ntt, struct ntt_c *c) {

    c->iter_index = 0;
    c->iter_next = (struct ntt_node *)NULL;
    return(c_ntt_next(ntt, c));
}

/* Point cursor to next iteration in tree */

struct ntt_node *c_ntt_next(struct ntt *ntt, struct ntt_c *c) {
    long index;
    struct ntt_node *node = c->iter_next;

    if (ntt == NULL) return NULL;

    if (node) {
        if (node != NULL) {
            c->iter_next = node->next;
            return (node);
        }
    }

    if (! node) {
        while (c->iter_index < ntt->size) {
            index = c->iter_index++;

            if (ntt->tbl[index]) {
                c->iter_next = ntt->tbl[index]->next;
                return(ntt->tbl[index]);
            }
        }
    }
    return((struct ntt_node *)NULL);
}

/* END NTT (Named Pointer Tree) Functions */

