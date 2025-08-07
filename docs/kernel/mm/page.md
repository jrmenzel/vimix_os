# Page

A page is the smallest unit of memory the MMU can manage and map. `4KB` is the most common size, but some architectures like AARCH64 optionally also support `64KB` pages. VIMIX only supports `4KB` pages but tries not to have implicit dependencies on this value.

Kernel code uses the `PAGE_SIZE` define from `kernel/page.h`.

User space applications can figure out the page size by calling `sysconf`:

```C
#include <unistd.h>

long page_size = sysconf(_SC_PAGE_SIZE);
```


---
**Overview:** [memory management](memory_management.md)

**Related:** [kernel memory map](memory_map_kernel.md) | [process memory map](memory_map_process.md) | [page](page.md)
