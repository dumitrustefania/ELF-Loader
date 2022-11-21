/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "exec_parser.h"

static so_exec_t *exec;
static struct sigaction generic_sigaction;
static int timesHandlerCalled;
static int ELF_file;

static void segv_handler(int signum, siginfo_t *info, void *context)
{	/*
	 * Create a 1000 elements vector in the data field of each segment.
	 * Assuming we could have a max of 1000 pages of memory to be mapped for a segment,
	 * segmend.data[k] = 1 would mean that page with index k has already been mapped
	 * into memory for the segment, so we can't map it again.
	 * Only perform this the first time this handler is called
	 */
	if (!timesHandlerCalled) {
		for (int i = 0; i < exec->segments_no; i++)
			exec->segments[i].data = calloc(1000, sizeof(int));

		timesHandlerCalled++;
	}

	/* Extract the address that resulted in SIGSEGV*/
	uintptr_t faulty_address = (uintptr_t)info->si_addr;

	/* Check whether the address is part of my segments or not*/
	for (int i = 0; i < exec->segments_no; i++) {
		so_seg_t segment = exec->segments[i];

		/* Segment's starting address*/
		uintptr_t start_seg = segment.vaddr;
		/* Segment's ending address*/
		uintptr_t end_seg = segment.vaddr + segment.mem_size;

		/* Check whether the faulty address lies between the boundaries of a segment*/
		if (faulty_address >= start_seg && faulty_address <= end_seg) {
			int page_size = getpagesize();
			/* Determine the number of the page where the faulty address is coming from*/
			int pageNumOfFault = (faulty_address - start_seg) / page_size;

			/* Check whether this page has already been mapped for this segment*/
			if (((int *)segment.data)[pageNumOfFault] == 0) {
				/*
				 * Determine the exact address where the new page has to be mapped.
				 * start_seg should represent the start of page 0, so page K should
				 * start at address start_seg + pageNumOfFault * page_size
				 */
				uintptr_t startAddressOfPage = start_seg + pageNumOfFault * page_size;

				/*
				 * Map a new page into memory at the given address
				 * (startAddressOfPage previously determined).
				 * PERM_W ensures write permissions;
				 * MAP_FIXED makes sure the page is created exactly at the given address in memory;
				 * MAP_PRIVATE creates a private mapping (it is mandatory to choose either this one or MAP_SHARED);
				 * MAP_ANONYMOUS is used because the mapping is not backed by any file,
				 * this flag ignores the fd argument, which has to be -1,
				 * the offset argument has to be 0 because of it too;
				 */
				void *mapped_page = mmap(startAddressOfPage, page_size, PERM_W, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

				/* Check whether the memory was not successfully mapped*/
				if (mapped_page == MAP_FAILED)
					exit(-1);

				/*
				 * Zero the whole new page for an easier manipulation
				 * of the .bss zone when copying
				 */
				memset(mapped_page, 0, page_size);

				/* Start copying the data from the segment to the page*/

				/*
				 * We are given the offset where the segment starts,
				 * so the first address that we have to copy is basically
				 * offset + pageNumOfFault * page_size because we have to skip
				 * pageNumOfFault pages, pretending like that data is already
				 * mapped into memory
				 */
				uintptr_t startSegmentCopy = segment.offset + pageNumOfFault * page_size;

				/*
				 * Reset the file offset to the address where the data has
				 * to be copied from
				 */
				lseek(ELF_file, startSegmentCopy, SEEK_SET);

				/*
				 * Determine file end address to check whether the segment is partially
				 * in the .bss or unspecified area
				 */
				uintptr_t endSegmentFile = segment.offset + segment.file_size;

				/*
				 * If the whole page is created within the file size bounds
				 * (so no .bss or unspecified area in our page), we read
				 * 4096 bytes of data from the file to the page
				 */
				if (startSegmentCopy + page_size <= endSegmentFile)
					read(ELF_file, mapped_page, page_size);

				/*
				 * If some part of the page lies ouside the file size bounds
				 * (so if has parts of .bss or unspecified area), we only copy bytes from the
				 * file until reaching the end of the segment file
				 */
				else if (startSegmentCopy <= endSegmentFile)
					read(ELF_file, mapped_page, endSegmentFile - startSegmentCopy);

				/*
				 * If the whole page lies ouside the file size bounds
				 * we don't copy anything and the zone is already zeroed
				 */

				/*
				 * Change the protection of the page.
				 * We do this now because we could't copy the data
				 * to the page if we had no writing rights specified
				 */
				int err = mprotect(startAddressOfPage, page_size, segment.perm);

				/* Check whether the protection rights were not successfully changed*/
				if (err == -1)
					exit(-1);

				/*
				 * Memorize the fact that we already created the page with number
				 * pageNumOfFault for this segment
				 */
				((int *)segment.data)[pageNumOfFault] = 1;

				return;
			}
		}
	}

	/*
	 * The address that resulted in SIGSEGV is not part of any of our segments
	 * or it is part of a segment, but its page is already mapped.
	 * We call the default SIGSEGV handler, that returns SEGFAULT and terminates
	 * the program
	 */
	generic_sigaction.sa_sigaction(signum, info, context);
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);

	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);

	ELF_file = open(path, O_RDONLY);

	if (!exec)
		return -1;

	so_start_exec(exec, argv);

	return -1;
}
