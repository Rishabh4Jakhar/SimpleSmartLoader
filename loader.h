#include <stdio.h>
#include <elf.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>

#define PAGE_SIZE 4096
#define MAX_PAGES 10

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ROUND_UP(a, b) (((a + (b - 1)) & -(b)))

#define ll_i long long int

struct sigaction old_state;

typedef struct {
    uintptr_t vaddr; 
    size_t mem_size;
    off_t offset; 
    int perm;
    int file_size;
    int data[MAX_PAGES]; 
} Segment;

// Function Prototypes
void populate_page(Segment *segment, char *program_path, char *page_buffer, uintptr_t address);
void handle_segmentation_fault(int signal, siginfo_t *info, void *context);
void initialize_and_run_elf(char** exec_path);
void configure_signal_handler();
void cleanup_loader();
