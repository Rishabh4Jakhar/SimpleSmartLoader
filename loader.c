#include "loader.h"

// Global variables
char *program_path;
Elf32_Ehdr *elf_header;
Elf32_Phdr *program_header;
int file_descriptor;
int fault_count = 0;
int allocated_pages = 0;
ll_i memory_fragmentation = 0;
int entry_address;
int load_segment_count = 0;
Segment *segments;

// Release all allocated resources
void cleanup_loader() {
    elf_header = NULL;
    free(elf_header);
    program_header = NULL;
    free(program_header);
    free(segments);
    program_path = NULL;
    free(program_path);
}

// Load data into a page from the ELF file
void populate_page(Segment *segment, char *program_path, char *page_buffer, uintptr_t address) {
    int page_index = (address - segment->vaddr) / PAGE_SIZE;
    int file = open(program_path, O_RDONLY);
    lseek(file, segment->offset + page_index * PAGE_SIZE, SEEK_SET);
    char *temp_buffer = (char *)malloc(PAGE_SIZE * sizeof(char));
    if (temp_buffer == NULL) {
        perror("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    int bytes_read = read(file, temp_buffer, MIN(PAGE_SIZE, MAX(0, segment->file_size - page_index * PAGE_SIZE)));
    memcpy(page_buffer, temp_buffer, bytes_read);
    close(file);
    free(temp_buffer);
}

// Custom handler for segmentation faults
void handle_segmentation_fault(int signal, siginfo_t *info, void *context) {
    if (info->si_code == SEGV_ACCERR) {
        printf("Access Denied Error: ");
        old_state.sa_sigaction(signal, info, context);
    }
    void *fault_address = info->si_addr;
    Segment *target_segment;
    int segment_found = 0;

    for (int i = 0; i < load_segment_count; i++) {
        if (fault_address >= (void *)segments[i].vaddr && fault_address <= (void *)(segments[i].vaddr + segments[i].mem_size)) {
            target_segment = &segments[i];
            segment_found = 1;
            break;
        }
    }
    if (!segment_found) {
        printf("Address Out of Bounds: ");
        old_state.sa_sigaction(signal, info, context);
    }

    int offset_within_segment = (uintptr_t)info->si_addr - target_segment->vaddr;
    int page_num = offset_within_segment / PAGE_SIZE;
    int page_present = target_segment->data[page_num];
    
    if (page_present != 0) {
        printf("Code Error: ");
        old_state.sa_sigaction(signal, info, context);
    }

    fault_count++;
    void *new_page = mmap((void *)target_segment->vaddr + page_num * PAGE_SIZE, PAGE_SIZE, PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
    if (new_page == MAP_FAILED) {
        perror("Error in mmap\n");
        exit(EXIT_FAILURE);
    }
    allocated_pages++;
    populate_page(target_segment, program_path, new_page, (uintptr_t)info->si_addr);
    mprotect(new_page, PAGE_SIZE, target_segment->perm);
    target_segment->data[page_num] = 1;
}

// Initialize ELF loading and run program
void initialize_and_run_elf(char **exec_path) {
    file_descriptor = open(*exec_path, O_RDONLY);
    if (file_descriptor == -1) {
        perror("File could not be opened\n");
        exit(EXIT_FAILURE);
    }

    off_t file_size = lseek(file_descriptor, 0, SEEK_END);
    lseek(file_descriptor, 0, SEEK_SET);
    char *heap_memory = (char *)malloc(file_size);

    if (!heap_memory) {
        perror("Error: Could not allocate memory");
        exit(1);
    }

    ssize_t bytes_loaded = read(file_descriptor, heap_memory, file_size);
    if (bytes_loaded < 0 || (size_t)bytes_loaded != file_size) {
        perror("File read failed");
        free(heap_memory);
        exit(1);
    }

    elf_header = (Elf32_Ehdr *)heap_memory;
    program_header = (Elf32_Phdr *)(heap_memory + elf_header->e_phoff);
    Elf32_Phdr *current_header = program_header;
    int total_segments = elf_header->e_phnum;
    int i = 0;

    while (i < total_segments) {
        if (current_header->p_type == PT_LOAD) {
            load_segment_count++;
        }
        i++;
        current_header++;
    }

    entry_address = elf_header->e_entry;
    segments = (Segment *)malloc(load_segment_count * sizeof(Segment));
    int j = 0;

    for (int i = 0; i < total_segments; i++) {
        if (program_header[i].p_type == PT_LOAD) {
            Segment *seg = &segments[j];
            seg->perm = 0;

            if (program_header[i].p_flags & PF_X) seg->perm |= 4;
            if (program_header[i].p_flags & PF_R) seg->perm |= 1;
            if (program_header[i].p_flags & PF_W) seg->perm |= 2;

            seg->vaddr = ROUND_UP(program_header[i].p_vaddr, PAGE_SIZE);
            seg->offset = program_header[i].p_offset - (program_header[i].p_vaddr - seg->vaddr);
            seg->mem_size = program_header[i].p_memsz + (program_header[i].p_vaddr - seg->vaddr);
            seg->file_size = program_header[i].p_filesz + (program_header[i].p_vaddr - seg->vaddr);
            memset(seg[i].data, 0, MAX_PAGES * sizeof(int));

            int allocated_memory = ROUND_UP(seg->mem_size, PAGE_SIZE);
            ll_i fragment = allocated_memory - seg->mem_size;
            memory_fragmentation += fragment;
            j++;
        }
    }

    int (*program_start)(void) = (int (*)(void))entry_address;
    int result = program_start();
    printf("Program returned: %d\n", result);
    printf("Page faults: %d\n", fault_count);
    printf("Page allocations: %d\n", allocated_pages);
    printf("Total memory fragmentation: %f KB\n", (double)memory_fragmentation / 1024);
}

// Set up the SIGSEGV handler
void configure_signal_handler() {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = handle_segmentation_fault;
    action.sa_flags = SA_SIGINFO;

    if (sigaction(SIGSEGV, &action, &old_state) == -1) {
        perror("Error setting up signal handler");
        exit(EXIT_FAILURE);
    }
}

// Main function
int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <ELF Executable>\n", argv[0]);
        exit(1);
    }

    program_path = argv[1];
    FILE *elf_file = fopen(argv[1], "rb");
    if (!elf_file) {
        printf("Error: Could not open ELF file.\n");
        exit(1);
    }
    fclose(elf_file);

    configure_signal_handler();
    initialize_and_run_elf(&argv[1]);
    cleanup_loader();
    return 0;
}
