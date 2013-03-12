#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

#define WHEAT_SLAB_SIZE          (4<<10)
#define WHEAT_ALIGN_BYTES        (sizeof(void *)<<1)
#define WHEAT_SLABCLASS_MAX_SIZE 20

#define WHEAT_ALIGN(d, a)     (((d) + (a - 1)) & ~(a - 1))

struct slab {
    uint8_t *start;
    struct slab *next;
};

struct slabClass {
    uint32_t per_slab_items;
    uint32_t per_item_size;

    struct slab *free_slab;
    uint32_t free_item_size;
    uint8_t *free_slab_pos;
};

struct slabcenter {
    struct slabClass slab_classes[WHEAT_SLABCLASS_MAX_SIZE];
    uint32_t max_item_size;
    double factor;
    struct slab *slabs;
    struct slab *bigs;
};

static size_t slabSizeToid(struct slabcenter *center, const size_t size)
{
    int i = 0;
    while (size > center->slab_classes[i].per_item_size && i < WHEAT_SLABCLASS_MAX_SIZE)
        i++;
    if (i == WHEAT_SLABCLASS_MAX_SIZE)
        return 0;
    return i;
}

static struct slab *slabAllocSlab(struct slabcenter *center)
{
    uint8_t *p = malloc(sizeof(struct slab)+WHEAT_SLAB_SIZE);
    if (!p)
        return NULL;
    struct slab *slab = (struct slab *)p;
    slab->start = p + sizeof(struct slab);
    slab->next = center->slabs;
    center->slabs = slab->next;
    return slab;
}

static struct slab *slabAllocBig(struct slabcenter* center, size_t size)
{
    uint8_t *p = malloc(sizeof(struct slab)+size);
    if (!p)
        return NULL;
    struct slab *slab = (struct slab *)p;
    slab->start = p + sizeof(struct slab);
    slab->next = center->bigs;
    center->bigs = slab->next;
    return slab;
}

struct slabcenter *slabcenterCreate(const int item_max, const double factor)
{
    ASSERT(item_max < WHEAT_SLAB_SIZE);
    uint32_t i = 0;
    uint32_t size = WHEAT_ALIGN_BYTES;

    struct slabcenter *c = malloc(sizeof(*c));
    memset(c, 0, sizeof(*c));
    c->factor = factor;
    c->max_item_size = item_max;
    while (i++ <= WHEAT_SLABCLASS_MAX_SIZE && size <= c->max_item_size) {
        size = WHEAT_ALIGN(size, WHEAT_ALIGN_BYTES);

        c->slab_classes[i].per_item_size = size;
        c->slab_classes[i].per_slab_items = (WHEAT_SLAB_SIZE) / size;
        c->slab_classes[i].free_slab = NULL;
        c->slab_classes[i].free_item_size = 0;
        size *= factor;
    }
    return c;
}

void slabcenterDealloc(struct slabcenter *c)
{
    struct slab *s = c->slabs;
    while (s != NULL) {
        free(s);
    }
    free(c);
}

void *slabAlloc(struct slabcenter *center, const size_t size)
{
    if (size == 0)
        return NULL;
    if (size > center->max_item_size) {
        struct slab *big = slabAllocBig(center, size);
        return big->start;
    }
    const size_t id = slabSizeToid(center, size);
    struct slabClass *slab_class = &center->slab_classes[id];
    uint8_t *ptr = NULL;
    uint32_t byte = 0;
    int i = 0;
    int ret = 0;
    if (slab_class->free_slab == NULL || slab_class->free_item_size == 0) {
        slab_class->free_slab = slabAllocSlab(center);
        slab_class->free_item_size = slab_class->per_slab_items;
        slab_class->free_slab_pos = slab_class->free_slab->start;
    }
    ptr = slab_class->free_slab_pos;
    slab_class->free_slab_pos += slab_class->per_item_size;
    return ptr;
}
