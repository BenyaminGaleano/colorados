#include "vm/frame.h"

static struct hash frame_table;

unsigned
ft_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const fte_t *ftentry = hash_entry (f_, fte_t, helem);
  return hash_bytes (&ftentry->frame, sizeof ftentry->frame);
}

bool
ft_less (
    const struct hash_elem *a,
    const struct hash_elem *b,
    void * aux UNUSED)
{
  fte_t *fte1 = hash_entry(a, fte_t, helem);
  fte_t *fte2 = hash_entry(b, fte_t, helem);

  return fte1->frame < fte2->frame;
}

void
init_frame_table(void)
{
  hash_init(&frame_table, ft_hash, ft_less, NULL);
}




