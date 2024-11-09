# OS Assignment 4

## Simple Smart Loader

### Implementation
  1.⁠ ⁠Setup Signal Handler (⁠ initialise_signal ⁠): This initializes a handler for ⁠ SIGSEGV ⁠ signals, redirecting them to the ⁠ segv_handler ⁠ function. The handler distinguishes between invalid memory access and valid page faults due to unallocated segments, allowing the program to continue execution without termination.

2.⁠ ⁠Open the ELF file, the ELF file specified as a command-line argument is opened and read into memory.

3.⁠ The ELF header (⁠ Elf32_Ehdr ⁠) is read to locate the entry point and segment headers.The loader identifies segments of type ⁠ PT_LOAD ⁠, which are  lazily loaded as needed during execution.

4.⁠ Each segment has a ⁠ data ⁠ array to track which pages have been loaded (using flags).

5.⁠ ⁠The loader attempts to execute the program directly by casting the entry point address to a function pointer. Since no memory is initially mapped, accessing the entry point causes a segmentation fault, triggering the custom ⁠ segv_handler ⁠.

6.⁠ ⁠ When a segmentation fault occurs, the handler checks if it’s within the bounds of a known segment. If the fault address falls outside these bounds, it signifies an invalid memory access, and the default SIGSEGV action is invoked.


7.⁠ ⁠ If the fault address is within a segment but not loaded, it’s treated as a page fault.
The faulting page is allocated using ⁠ mmap ⁠, and data is loaded from the ELF file into this page using ⁠ load_page_data ⁠.

8.⁠ ⁠After loading, ⁠the  mprotect ⁠ sets the appropriate permissions (e.g., read, write, execute) for the page.
9.⁠ ⁠The segment’s ⁠ data ⁠ array is updated to mark the page as loaded, and page fault/page allocation counters are incremented for reporting.

10.⁠ ⁠Calculating page offset and read the data from the ELF file.

12.⁠ The appropriate page is mapped and loaded into memory, aligning with the fault address in the segment.

13⁠  ⁠After execution, dynamically allocated memory (for ELF headers, segments, etc.) is freed, ensuring there are no memory leaks.


## Contributions:
`Rishabh Jakhar(2023435)`:

`Uday Pandita(2023563)`:

## Github repository  link:
https://github.com/Rishabh4Jakhar/SimpleSmartLoader 

