#pragma once

void fls_initialize(const char* exe_path);

/* 
 * this function does not work for pointer types. you can not have a global float* and try to
 * serialize that. it can only be data types (int, float, struct, etc) or arrays of
 * data types (float[], not ((float*)float[]) 
 */
void fls_serialize(const char* symbol_name, void* data);

void fls_finish(void);

#if defined(FLS_IMPLEMENTATION)

#if defined(__linux__) || defined(__unix__)

#include <elf.h>
#include <stdio.h>

#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

typedef struct fls_instance_elf_t {
	union {
		struct x64 {
			Elf64_Ehdr header;
			Elf64_Shdr symtab_hdr;
			Elf64_Shdr strtab_hdr;
			Elf64_Shdr shstrtab_hdr;

			Elf64_Sym* symtab;
			Elf64_Phdr* program_hdrs;
		} x64;

		struct x32 {
			Elf32_Ehdr header;
			Elf32_Shdr symtab_hdr;
			Elf32_Shdr strtab_hdr;
			Elf32_Shdr shstrtab_hdr;

			Elf32_Sym* symtab;
			Elf32_Phdr* program_hdrs;
		} x32;
	};

	enum {
		TYPE_X32,
		TYPE_X64
	} type;

	FILE* file;
	char* shstrtab;
	char* strtab;
	uint8_t has_symtab;
	uint8_t has_strtab;
} fls_instance_elf_t;

static fls_instance_elf_t _fls_instance;

void _fls_copy_file(int to, int from) {
	struct stat st;
	fstat(from, &st);

	off_t offset = 0;
	ssize_t result = sendfile(to, from, &offset, st.st_size);
	assert(result == st.st_size);

	lseek(to, 0, SEEK_SET);
}

void _fls_initialize_elf_x64() {
	assert(fread(&_fls_instance.x64.header, sizeof(Elf64_Ehdr), 1, _fls_instance.file));

	assert(fseek(_fls_instance.file, _fls_instance.x64.header.e_shoff, SEEK_SET) == 0);

	Elf64_Shdr* shdrs = malloc(_fls_instance.x64.header.e_shentsize*_fls_instance.x64.header.e_shnum);
	assert(fread(shdrs, _fls_instance.x64.header.e_shentsize, _fls_instance.x64.header.e_shnum, _fls_instance.file));

	assert(fseek(_fls_instance.file, _fls_instance.x64.header.e_phoff, SEEK_SET) == 0);

	_fls_instance.x64.program_hdrs = malloc(_fls_instance.x64.header.e_phentsize*_fls_instance.x64.header.e_phnum);
	assert(fread(_fls_instance.x64.program_hdrs, _fls_instance.x64.header.e_phentsize, _fls_instance.x64.header.e_phnum, _fls_instance.file));

	_fls_instance.x64.shstrtab_hdr = shdrs[_fls_instance.x64.header.e_shstrndx];
	assert(fseek(_fls_instance.file, _fls_instance.x64.shstrtab_hdr.sh_offset, SEEK_SET) == 0);

	_fls_instance.shstrtab = malloc(_fls_instance.x64.shstrtab_hdr.sh_size);
	assert(fread(_fls_instance.shstrtab, _fls_instance.x64.shstrtab_hdr.sh_size, 1, _fls_instance.file));

	for (int i = 0; i < _fls_instance.x64.header.e_shnum; ++i) {
		Elf64_Shdr* shdr = &shdrs[i];
		const char* section_name = _fls_instance.shstrtab + shdr->sh_name;

		if (strcmp(section_name, ".symtab") == 0) {
			_fls_instance.has_symtab = 1;
			_fls_instance.x64.symtab_hdr = *shdr;
		} else if (strcmp(section_name, ".strtab") == 0) {
			_fls_instance.has_strtab = 1;
			_fls_instance.x64.strtab_hdr = *shdr;
		}
	}

	assert(_fls_instance.has_symtab && _fls_instance.has_strtab);

	_fls_instance.strtab = malloc(_fls_instance.x64.strtab_hdr.sh_size);
	assert(fseek(_fls_instance.file, _fls_instance.x64.strtab_hdr.sh_offset, SEEK_SET) == 0);
	assert(fread(_fls_instance.strtab, _fls_instance.x64.strtab_hdr.sh_size, 1, _fls_instance.file));

	_fls_instance.x64.symtab = malloc(_fls_instance.x64.symtab_hdr.sh_size);
	assert(fseek(_fls_instance.file, _fls_instance.x64.symtab_hdr.sh_offset, SEEK_SET) == 0);
	assert(fread(_fls_instance.x64.symtab, _fls_instance.x64.symtab_hdr.sh_size, 1, _fls_instance.file));

	free(shdrs);
}

void _fls_initialize_elf_x32() {
	assert(fread(&_fls_instance.x32.header, sizeof(Elf32_Ehdr), 1, _fls_instance.file));

	assert(fseek(_fls_instance.file, _fls_instance.x32.header.e_shoff, SEEK_SET) == 0);

	Elf32_Shdr* shdrs = malloc(_fls_instance.x32.header.e_shentsize*_fls_instance.x32.header.e_shnum);
	assert(fread(shdrs, _fls_instance.x32.header.e_shentsize, _fls_instance.x32.header.e_shnum, _fls_instance.file));

	assert(fseek(_fls_instance.file, _fls_instance.x32.header.e_phoff, SEEK_SET) == 0);

	_fls_instance.x32.program_hdrs = malloc(_fls_instance.x32.header.e_phentsize*_fls_instance.x32.header.e_phnum);
	assert(fread(_fls_instance.x32.program_hdrs, _fls_instance.x32.header.e_phentsize, _fls_instance.x32.header.e_phnum, _fls_instance.file));

	_fls_instance.x32.shstrtab_hdr = shdrs[_fls_instance.x32.header.e_shstrndx];
	assert(fseek(_fls_instance.file, _fls_instance.x32.shstrtab_hdr.sh_offset, SEEK_SET) == 0);

	_fls_instance.shstrtab = malloc(_fls_instance.x32.shstrtab_hdr.sh_size);
	assert(fread(_fls_instance.shstrtab, _fls_instance.x32.shstrtab_hdr.sh_size, 1, _fls_instance.file));

	for (int i = 0; i < _fls_instance.x32.header.e_shnum; ++i) {
		Elf32_Shdr* shdr = &shdrs[i];
		const char* section_name = _fls_instance.shstrtab + shdr->sh_name;

		if (strcmp(section_name, ".symtab") == 0) {
			_fls_instance.has_symtab = 1;
			_fls_instance.x32.symtab_hdr = *shdr;
		} else if (strcmp(section_name, ".strtab") == 0) {
			_fls_instance.has_strtab = 1;
			_fls_instance.x32.strtab_hdr = *shdr;
		}
	}

	assert(_fls_instance.has_symtab && _fls_instance.has_strtab);

	_fls_instance.strtab = malloc(_fls_instance.x32.strtab_hdr.sh_size);
	assert(fseek(_fls_instance.file, _fls_instance.x32.strtab_hdr.sh_offset, SEEK_SET) == 0);
	assert(fread(_fls_instance.strtab, _fls_instance.x32.strtab_hdr.sh_size, 1, _fls_instance.file));

	_fls_instance.x32.symtab = malloc(_fls_instance.x32.symtab_hdr.sh_size);
	assert(fseek(_fls_instance.file, _fls_instance.x32.symtab_hdr.sh_offset, SEEK_SET) == 0);
	assert(fread(_fls_instance.x32.symtab, _fls_instance.x32.symtab_hdr.sh_size, 1, _fls_instance.file));

	free(shdrs);
}

void fls_initialize(const char* exe_path) {
	memset(&_fls_instance, 0, sizeof(fls_instance_elf_t));

	int rfd = open(exe_path, O_RDONLY);

	unlink(exe_path);
	int fd = open(exe_path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);

	_fls_copy_file(fd, rfd);
	close(rfd);

	_fls_instance.file = fdopen(fd, "w+");
	assert(_fls_instance.file != NULL && "File could not be loaded");
	
	unsigned char e_ident[EI_NIDENT];
	assert(fread(e_ident, EI_NIDENT, 1, _fls_instance.file));

	assert(
		e_ident[EI_MAG0] == ELFMAG0
	 && e_ident[EI_MAG1] == ELFMAG1
	 && e_ident[EI_MAG2] == ELFMAG2
	 && e_ident[EI_MAG3] == ELFMAG3
	);

	fseek(_fls_instance.file, 0, SEEK_SET);
	if (e_ident[EI_CLASS] == ELFCLASS64) {
		printf("using ELF64\n");
		_fls_instance.type = TYPE_X64;
		_fls_initialize_elf_x64();
	} else {
		printf("using ELF32\n");
		_fls_instance.type = TYPE_X32;
		_fls_initialize_elf_x32();
	}
}

/* seeks _fls_instance.file to location at addr */
void _fls_resolve_elf_x64_addr(Elf64_Addr addr) {
	for (int i = 0; i < _fls_instance.x64.header.e_phnum; ++i) {
		Elf64_Phdr* phdr = &_fls_instance.x64.program_hdrs[i];
		
		if (phdr == NULL || phdr->p_type != PT_LOAD) {
			continue;
		}

		Elf64_Addr start = phdr->p_vaddr;
		Elf64_Addr end = phdr->p_vaddr + phdr->p_memsz;

		if (addr >= start && addr < end) {
			Elf64_Off off = phdr->p_offset + (addr-start);
			fseek(_fls_instance.file, off, SEEK_SET);

			return;
		}
	}

	assert(0 && "could not resolve address");
}

void _fls_resolve_elf_x32_addr(Elf64_Addr addr) {
	for (int i = 0; i < _fls_instance.x32.header.e_phnum; ++i) {
		Elf32_Phdr* phdr = &_fls_instance.x32.program_hdrs[i];
		
		if (phdr == NULL || phdr->p_type != PT_LOAD) {
			continue;
		}

		Elf32_Addr start = phdr->p_vaddr;
		Elf32_Addr end = phdr->p_vaddr + phdr->p_memsz;

		if (addr >= start && addr < end) {
			Elf32_Off off = phdr->p_offset + (addr-start);
			fseek(_fls_instance.file, off, SEEK_SET);

			return;
		}
	}

	assert(0 && "could not resolve address");
}

void _fls_serialize_elf_x64(const char* symbol_name, void* data) {
	for (int i = 0; i < _fls_instance.x64.symtab_hdr.sh_size/sizeof(Elf64_Sym); ++i) {
		Elf64_Sym* sym = &_fls_instance.x64.symtab[i];
		const char* sym_name = sym->st_name + _fls_instance.strtab;
		
		if (strcmp(sym_name, symbol_name) == 0) {
			_fls_resolve_elf_x64_addr(sym->st_value);

			fwrite(data, sym->st_size, 1, _fls_instance.file);

			break;
		}
	}
}

void _fls_serialize_elf_x32(const char* symbol_name, void* data) {
	for (int i = 0; i < _fls_instance.x32.symtab_hdr.sh_size/sizeof(Elf64_Sym); ++i) {
		Elf32_Sym* sym = &_fls_instance.x32.symtab[i];
		const char* sym_name = sym->st_name + _fls_instance.strtab;
		
		if (strcmp(sym_name, symbol_name) == 0) {
			_fls_resolve_elf_x32_addr(sym->st_value);

			fwrite(data, sym->st_size, 1, _fls_instance.file);

			break;
		}
	}
}

void fls_serialize(const char* symbol_name, void* data) {
	if (_fls_instance.type == TYPE_X64) {
		_fls_serialize_elf_x64(symbol_name, data);
	} else {
		_fls_serialize_elf_x32(symbol_name, data);
	}
}

void fls_finish() {
	if (_fls_instance.type == TYPE_X64) {
		if (_fls_instance.x64.symtab) {
			free(_fls_instance.x64.symtab);
		} if (_fls_instance.x64.program_hdrs) {
			free(_fls_instance.x64.program_hdrs);
		}
	} else {
		if (_fls_instance.x32.symtab) {
			free(_fls_instance.x32.symtab);
		} if (_fls_instance.x32.program_hdrs) {
			free(_fls_instance.x32.program_hdrs);
		}
	}

	if (_fls_instance.strtab) {
		free(_fls_instance.strtab);
	} if (_fls_instance.shstrtab) {
		free(_fls_instance.shstrtab);
	} if (_fls_instance.file) {
		fclose(_fls_instance.file);
	}
}

#endif

#endif /* FLS_IMPLEMENTATION */
