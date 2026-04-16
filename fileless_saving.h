/* 
 * Fileless Saving v1.0
 *
 * Copyright (c) 2026 tkpmonke
 * 
 * This software is provided ‘as-is’, without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * Disclamer:
 *
 * Please, for the love of god, don't use this library for literally anything
 * even slightly important. I don't even know if this library is capable of
 * blowing up my computer or not.
 *
 * However if you are insane enough, heres an example :3
 *
 * struct testing_struct {
 * 	int integer;
 * } test_struct = {
 * 	.integer = 5
 * };
 * 
 * void serialize() {
 * 	test_struct.integer += 2;
 * 	fls_initialize(NULL);
 * 	fls_serialize("test_struct", (void*)&test_struct);
 * 	fls_finish();
 * }
 *
 * Now the next time the program is ran, test_struct.integer will be greater by 2 :D
*/

#if !defined(_FILELESS_SAVING_H_)
#define _FILELESS_SAVING_H_

/***** TYPE DECLARATIONS *****/

typedef struct fls_binary_t fls_binary_t;

typedef struct {
	void* (*malloc)(unsigned long);
	void (*free)(void*);
} fls_allocator_t;

/***** FUNCTION DECLARATIONS *****/

void fls_set_global_allocator(fls_allocator_t);

fls_binary_t* fls_initialize(void);

/* 
 * this function does not work for pointer types. you can not have a global float* and try to
 * serialize that. it can only be data types (int, float, struct, etc) or arrays of
 * data types (float[], not ((float*)float[]) 
 */

/*
 * symbol in `symbol_name` *must* be public. this is enabled by default on gcc/clang, but
 * must be enabled with `__declspec(dllexport)` on windows
 */

void fls_serialize_from_symbol_name(fls_binary_t* binary, const char* symbol_name, void* data);

/*
 * this function will serialize data using the location in memory to determine where
 * the data originated from in the binary file.
 *
 * this means it ***can not*** be used with dynamically allocated memory. if you do so
 * the exe file will likely corrupt (or the function will fail, either one is possible)
 */
void fls_serialize_from_pointer(fls_binary_t* binary, void* data, unsigned long size);

/* cleans up from fls_initialize */
void fls_finish(fls_binary_t* binary);

/***** FUNCTION IMPLEMENTATIONS *****/

#if defined(FLS_IMPLEMENTATION)

static fls_allocator_t _fls_allocator = {
	(void*)0,
	(void*)0
};

void fls_set_global_allocator(fls_allocator_t a) {
	_fls_allocator = a;
}

#if defined(__linux__) || defined(__unix__)

#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112
#endif

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <link.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#if defined(__linux)
#include <linux/limits.h>
#elif !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

struct fls_binary_t {
	ElfW(Ehdr) header;
	ElfW(Shdr) symtab_hdr;
	ElfW(Shdr) strtab_hdr;
	ElfW(Shdr) shstrtab_hdr;

	ElfW(Sym)* symtab;
	ElfW(Phdr)* program_hdrs;

	FILE* file;
	char* shstrtab;
	char* strtab;
	uint64_t exe_base_offset;
	uint8_t has_symtab;
	uint8_t has_strtab;
};

void _fls_copy_file(int to, int from) {
	struct stat st;
	fstat(from, &st);

	off_t offset = 0;
	ssize_t result = sendfile(to, from, &offset, st.st_size);
	assert(result == st.st_size);

	lseek(to, 0, SEEK_SET);
}

fls_binary_t* fls_initialize() {
	if (_fls_allocator.malloc == NULL
	 || _fls_allocator.free == NULL) {
		_fls_allocator = (fls_allocator_t){
			malloc,
			free
		};
	}

	fls_binary_t* binary = (fls_binary_t*)_fls_allocator.malloc(sizeof(fls_binary_t));
	memset(binary, 0, sizeof(fls_binary_t));

	static char buf[PATH_MAX];
	ssize_t g = readlink("/proc/self/exe", buf, PATH_MAX);
	assert(g != -1);
	buf[g] = '\0';

	int rfd = open(buf, O_RDONLY);

	unlink(buf);
	int fd = open(buf, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);

	_fls_copy_file(fd, rfd);
	close(rfd);

	binary->file = (FILE*)fdopen(fd, "w+");
	assert(binary->file != NULL && "File could not be loaded");
	
	unsigned char e_ident[EI_NIDENT];
	assert(fread(e_ident, EI_NIDENT, 1, binary->file));

	assert(
		e_ident[EI_MAG0] == ELFMAG0
	 && e_ident[EI_MAG1] == ELFMAG1
	 && e_ident[EI_MAG2] == ELFMAG2
	 && e_ident[EI_MAG3] == ELFMAG3
	);

	fseek(binary->file, 0, SEEK_SET);

	assert(fread(&binary->header, sizeof(ElfW(Ehdr)), 1, binary->file));

	assert(fseek(binary->file, binary->header.e_shoff, SEEK_SET) == 0);

	ElfW(Shdr)* shdrs = _fls_allocator.malloc(binary->header.e_shentsize*binary->header.e_shnum);
	assert(fread(shdrs, binary->header.e_shentsize, binary->header.e_shnum, binary->file));

	assert(fseek(binary->file, binary->header.e_phoff, SEEK_SET) == 0);

	binary->program_hdrs = _fls_allocator.malloc(binary->header.e_phentsize*binary->header.e_phnum);
	assert(fread(binary->program_hdrs, binary->header.e_phentsize, binary->header.e_phnum, binary->file));

	binary->shstrtab_hdr = shdrs[binary->header.e_shstrndx];
	assert(fseek(binary->file, binary->shstrtab_hdr.sh_offset, SEEK_SET) == 0);

	binary->shstrtab = _fls_allocator.malloc(binary->shstrtab_hdr.sh_size);
	assert(fread(binary->shstrtab, binary->shstrtab_hdr.sh_size, 1, binary->file));
 
	int i;
	for (i = 0; i < binary->header.e_shnum; ++i) {
		ElfW(Shdr)* shdr = &shdrs[i];
		const char* section_name = binary->shstrtab + shdr->sh_name;

		if (strcmp(section_name, ".symtab") == 0) {
			binary->has_symtab = 1;
			binary->symtab_hdr = *shdr;
		} else if (strcmp(section_name, ".strtab") == 0) {
			binary->has_strtab = 1;
			binary->strtab_hdr = *shdr;
		}
	}

	assert(binary->has_symtab && binary->has_strtab);

	binary->strtab = _fls_allocator.malloc(binary->strtab_hdr.sh_size);
	assert(fseek(binary->file, binary->strtab_hdr.sh_offset, SEEK_SET) == 0);
	assert(fread(binary->strtab, binary->strtab_hdr.sh_size, 1, binary->file));

	binary->symtab = _fls_allocator.malloc(binary->symtab_hdr.sh_size);
	assert(fseek(binary->file, binary->symtab_hdr.sh_offset, SEEK_SET) == 0);
	assert(fread(binary->symtab, binary->symtab_hdr.sh_size, 1, binary->file));

	_fls_allocator.free(shdrs);

	return binary;
}

/* seeks file to location at addr */
void _fls_resolve_elf_addr(fls_binary_t* binary, Elf64_Addr addr) {
	int i;
	for (i = 0; i < binary->header.e_phnum; ++i) {
		ElfW(Phdr)* phdr = &binary->program_hdrs[i];
		
		if (phdr == NULL || phdr->p_type != PT_LOAD) {
			continue;
		}

		ElfW(Addr) start = phdr->p_vaddr;
		ElfW(Addr) end = phdr->p_vaddr + phdr->p_memsz;

		if (addr >= start && addr < end) {
			ElfW(Off) off = phdr->p_offset + (addr-start);
			fseek(binary->file, off, SEEK_SET);

			return;
		}
	}

	assert(0 && "could not resolve address");
}

void fls_serialize_from_symbol_name(fls_binary_t* binary, const char* symbol_name, void* data) {
	long unsigned int i;
	for (i = 0; i < binary->symtab_hdr.sh_size/sizeof(Elf64_Sym); ++i) {
		ElfW(Sym)* sym = &binary->symtab[i];
		const char* sym_name = sym->st_name + binary->strtab;
		
		if (strcmp(sym_name, symbol_name) == 0) {
			_fls_resolve_elf_addr(binary, sym->st_value);

			fwrite(data, sym->st_size, 1, binary->file);

			break;
		}
	}
}

int _fls_dl_iterate_callback(struct dl_phdr_info* info, size_t size, void* data) {
	(void)size;
	if (info->dlpi_name == NULL || info->dlpi_name[0] == '\0') {
		fls_binary_t* binary = (fls_binary_t*)data;
		binary->exe_base_offset = info->dlpi_addr;
		return 1;
	}
	return 0;
}

void fls_serialize_from_pointer(fls_binary_t* binary, void* data, unsigned long size) {
	if (binary->exe_base_offset == 0) {
		dl_iterate_phdr(_fls_dl_iterate_callback, binary);
	}

	uint64_t va = (uintptr_t)data-binary->exe_base_offset;
	int64_t offset = -1;
 
	int i;
	for (i = 0; i < binary->header.e_phnum; ++i) {
		ElfW(Phdr)* ph = &binary->program_hdrs[i];

		if (va >= ph->p_vaddr && va < ph->p_vaddr + ph->p_memsz) {
			offset = ph->p_offset + (va - ph->p_vaddr);
		}
	}

	assert(offset != -1);
	assert(fseek(binary->file, offset, SEEK_SET) == 0);
	fwrite(data, size, 1, binary->file);
}

void fls_finish(fls_binary_t* binary) {
	if (binary->symtab) {
		_fls_allocator.free(binary->symtab);
	} if (binary->program_hdrs) {
		_fls_allocator.free(binary->program_hdrs);
	}

	if (binary->strtab) {
		_fls_allocator.free(binary->strtab);
	} if (binary->shstrtab) {
		_fls_allocator.free(binary->shstrtab);
	} if (binary->file) {
		fclose(binary->file);
	}
}

#endif /* __linux__ || __unix__ */
#endif /* FLS_IMPLEMENTATION */
#endif /* _FILELESS_SAVING_H_ */
