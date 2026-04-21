// tree.c — Tree object serialization and construction
//
// PROVIDED functions: get_file_mode, tree_parse, tree_serialize
// TODO functions:     tree_from_index
//
// Binary tree format (per entry, concatenated with no separators):
//   "<mode-as-ascii-octal> <name>\0<32-byte-binary-hash>"
//
// Example single entry (conceptual):
//   "100644 hello.txt\0" followed by 32 raw bytes of SHA-256

#include "tree.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ─── Mode Constants ─────────────────────────────────────────────────────────

#define MODE_FILE      0100644
#define MODE_EXEC      0100755
#define MODE_DIR       0040000

// ─── PROVIDED ───────────────────────────────────────────────────────────────

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (S_ISDIR(st.st_mode))  return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;
    const uint8_t *ptr = (const uint8_t *)data;
    const uint8_t *end = ptr + len;

    while (ptr < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *entry = &tree_out->entries[tree_out->count];

        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        size_t mode_len = space - ptr;
        if (mode_len >= sizeof(mode_str)) return -1;
        memcpy(mode_str, ptr, mode_len);
        entry->mode = strtol(mode_str, NULL, 8);

        ptr = space + 1;

        const uint8_t *null_byte = memchr(ptr, '\0', end - ptr);
        if (!null_byte) return -1;

        size_t name_len = null_byte - ptr;
        if (name_len >= sizeof(entry->name)) return -1;
        memcpy(entry->name, ptr, name_len);
        entry->name[name_len] = '\0';

        ptr = null_byte + 1;

        if (ptr + HASH_SIZE > end) return -1;
        memcpy(entry->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }
    return 0;
}

static int compare_tree_entries(const void *a, const void *b) {
    return strcmp(((const TreeEntry *)a)->name, ((const TreeEntry *)b)->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    size_t max_size = tree->count * 296;
    uint8_t *buffer = malloc(max_size);
    if (!buffer) return -1;

    Tree sorted_tree = *tree;
    qsort(sorted_tree.entries, sorted_tree.count, sizeof(TreeEntry), compare_tree_entries);

    size_t offset = 0;
    for (int i = 0; i < sorted_tree.count; i++) {
        const TreeEntry *entry = &sorted_tree.entries[i];
        int written = sprintf((char *)buffer + offset, "%o %s", entry->mode, entry->name);
        offset += written + 1;
        memcpy(buffer + offset, entry->hash.hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buffer;
    *len_out = offset;
    return 0;
}

// ─── TODO: Implement these ──────────────────────────────────────────────────

static int write_tree_level(IndexEntry *entries, int count, const char *prefix, ObjectID *id_out) {
    Tree tree;
    tree.count = 0;

    int i = 0;
    while (i < count) {
        const char *path = entries[i].path;

        const char *rel = path;
        if (prefix && prefix[0]) {
            rel = path + strlen(prefix) + 1;
        }

        char *slash = strchr(rel, '/');

        if (!slash) {
            // File at this level
            TreeEntry *e = &tree.entries[tree.count++];
            e->mode = entries[i].mode;
            e->hash = entries[i].hash;
            strncpy(e->name, rel, sizeof(e->name) - 1);
            e->name[sizeof(e->name) - 1] = '\0';
            i++;
        } else {
            // Subdirectory — get its name
            char dirname[256];
            size_t dir_len = slash - rel;
            strncpy(dirname, rel, dir_len);
            dirname[dir_len] = '\0';

            // Collect all entries under this subdirectory
            int start = i;
            while (i < count) {
                const char *r = entries[i].path;
                if (prefix && prefix[0]) r = entries[i].path + strlen(prefix) + 1;
                if (strncmp(r, dirname, dir_len) == 0 && r[dir_len] == '/')
                    i++;
                else
                    break;
            }

            // Build sub_prefix for the next recursion level
            char sub_prefix[512];
            if (prefix && prefix[0])
                snprintf(sub_prefix, sizeof(sub_prefix), "%s/%s", prefix, dirname);
            else
                snprintf(sub_prefix, sizeof(sub_prefix), "%s", dirname);

            // Recurse
            ObjectID sub_id;
            if (write_tree_level(entries + start, i - start, sub_prefix, &sub_id) < 0)
                return -1;

            // Add subtree entry
            TreeEntry *e = &tree.entries[tree.count++];
            e->mode = MODE_DIR;
            e->hash = sub_id;
            strncpy(e->name, dirname, sizeof(e->name) - 1);
            e->name[sizeof(e->name) - 1] = '\0';
        }
    }

    void *data;
    size_t len;
    if (tree_serialize(&tree, &data, &len) < 0) return -1;
    int ret = object_write(OBJ_TREE, data, len, id_out);
    free(data);
    return ret;
}

int tree_from_index(ObjectID *id_out) {
    Index idx;
    if (index_load(&idx) < 0) return -1;
    if (idx.count == 0) return -1;
    return write_tree_level(idx.entries, idx.count, "", id_out);
}