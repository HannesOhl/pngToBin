// TODO: chunk selection for compressed stream is not very clean
//       just save fp to first idat chunk on first occurence, see hack
#include "ext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdarg.h>

#define BUF_MAX 1024

void die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	if (fmt) vfprintf(stderr, fmt, ap);
	if (!fmt) fprintf(stderr, "Unknown error.\n");
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	}

	exit(EXIT_FAILURE);
}

static char* pname;

typedef struct {
	bool o_flg;
	char o_arg[BUF_MAX];
} Options;
Options opt = {
	.o_flg = false
};

void usage() {
	die("usage: %s input.png [-o output-file]\n", pname);
}

void make_base_name(char* arg, char* base_stripped) {

	const char* sls = strrchr(arg, '/');
	const char* base = sls ? sls + 1
			       : arg;

	const char* ext = strrchr(base, '.');
	size_t base_len = ext ? (size_t) (ext - base)
		              : (size_t) strlen(base);

	memcpy(base_stripped, base, base_len);
	base_stripped[base_len] = '\0';
}

void make_base_path(char* arg, char* base_path) {

	const char* sls = strrchr(arg, '/');
	size_t len = sls ? (size_t) (sls - arg) + 1
			 : (size_t) 0;

	memcpy(base_path, arg, len);
	base_path[len] = '\0';
}

// png uses big-endian
#define CHUNK_TYPE(i,j,k,l) ( (uint32_t) (l) << 24 | (uint32_t) (k) << 16 |	\
			      (uint32_t) (j) <<  8 | (uint32_t) (i) <<  0 )

#define IHDR CHUNK_TYPE('I', 'H', 'D', 'R')
#define IDAT CHUNK_TYPE('I', 'D', 'A', 'T')
#define IEND CHUNK_TYPE('I', 'E', 'N', 'D')

typedef struct {
	uint32_t length;
	uint32_t type;
	uint32_t crc;
} Chunk;

typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t bit_depth;
	uint32_t color_type;
} Ihdr;
Ihdr ihdr;

void header_print(uint8_t* b) {

	printf("8 byte png header:\n");
	printf("byte    1 (hex, should be   89): %02hhX\n", b[0]);
	printf("bytes 2-4 (asc, should be  PNG): %c%c%c\n", b[1], b[2], b[3]);
	printf("bytes 5-6 (hex, should be 0D0A): %02hhX%02hhX\n", b[4], b[5]);
	printf("byte    7 (hex, should be   1A): %02hhX\n", b[6]);
	printf("byte    8 (hex, should be   0A): %02hhX\n", b[7]);
}

bool header_check(uint8_t* b) {
	if (b[0] != 0x89) return false;
	if (b[1] != 0x50) return false;
	if (b[2] != 0x4E) return false;
	if (b[3] != 0x47) return false;
	if (b[4] != 0x0D) return false;
	if (b[5] != 0x0A) return false;
	if (b[6] != 0x1A) return false;
	if (b[7] != 0x0A) return false;
	return true;
}

void signature_verify(FILE* png) {

	const size_t HDR_SIZE = 8;

	uint8_t buffer_hdr[HDR_SIZE];
	fread(buffer_hdr, 1, HDR_SIZE, png);

	header_print(buffer_hdr);
	if (!header_check(buffer_hdr)) {
		fprintf(stderr, "Error, wrong header!\n");
		exit(2);
	} else {
		printf("PNG-header is correct!\n\n");
	}
}

/*
	=================================================================================
	| Chunk layout:									|
   	| 	|   Length  |  Chunk type  |    Chunk data    	|  CRC       |		|
     	|  	|   4 bytes |  4 bytes     |    Length bytes  	|  4 bytes   |		|
 	|										|
	| The pointer into the file has to point to the beginnig of a chunk on		|
 	| function entry!								|
  	|										|
        | We return info about the chunk, ie. length, type and the CRC. Upon leaving 	|
  	| the function the pointer into the file points to the next chunk.		|
 	|										|
        | NO pointer to the chunk data is returned!					|
 	=================================================================================
*/
Chunk chunk_get(FILE* png, size_t* chunk_current) {

	Chunk res = {};

	*chunk_current += 1;

	uint8_t buf[4];
	fread(buf, 1, 4, png);
	uint32_t length = 0;
	memcpy(&length, buf, 4);
	length = ntohl(length);
	res.length = length;

	uint8_t type[4];
	fread(type, 1, 4, png);
	memcpy(&res.type, type, 4);

	uint32_t type_hex = 0;
	memcpy(&type_hex, type, 4);


	printf("chunk %zu:\n", *chunk_current);
	printf("\tchunk info:\n");
	printf("\t\tlength = %lu\n", length);
	printf("\t\t  type = %.4s\n", type);
	printf("\t\t  type = 0x%08" PRIx32 "\n", type_hex);

	// move pointer into file to start of next chunk
	long offset = (long) length + 4;
	fseek(png, offset, SEEK_CUR);

	return res;
}

// Upon entry the pointer into the file points to the first chunk after the IHDR.
// Upon exit the same is true.
void ihdr_info_print(FILE* png, Chunk c) {

		uint8_t ihdr_data[c.length];
		fread(ihdr_data, 1, c.length, png);

		uint32_t width      	    = 0; // +0
		uint32_t height     	    = 0; // +4
		 uint8_t bit_depth  	    = 0; // +8
		 uint8_t color_type 	    = 0; // +9
		 uint8_t compression_method = 0; // +10
		 uint8_t filter_method 	    = 0; // +11
		 uint8_t interlace_method   = 0; // +12

		memcpy(&width		  , ihdr_data +  0, 4);
		ihdr.width = ntohl(width);
		memcpy(&height		  , ihdr_data +  4, 4);
		ihdr.height = ntohl(height);
		memcpy(&bit_depth	  , ihdr_data +  8, 1);
		ihdr.bit_depth = bit_depth;
		memcpy(&color_type	  , ihdr_data +  9, 1);
		ihdr.color_type = color_type;
		memcpy(&compression_method, ihdr_data + 10, 1);
		memcpy(&filter_method     , ihdr_data + 11, 1);
		memcpy(&interlace_method  , ihdr_data + 12, 1);

		printf("\tdata info:\n");
		printf("\t\twidth              = %lu\n", ihdr.width);
		printf("\t\theight             = %lu\n", ihdr.height);
		printf("\t\tbit_depth          = %u\n", bit_depth);
		printf("\t\tcolor_type         = %u\n", color_type);
		printf("\t\tcompression_method = %u\n", compression_method);
		printf("\t\tfilter_method      = %u\n", filter_method);
		printf("\t\tinterlace_method   = %u\n", interlace_method);

		uint8_t ihdr_crc[4];
		fread(ihdr_crc, 1, 4, png);
		uint32_t crc = 0;
		memcpy(&crc, ihdr_crc, 4);

		printf("\tcrc: \n\t\t%08" PRIx32 "\n", crc);
		return;
}

void chunk_idat_number_calculate(FILE* png, size_t* out) {

	size_t chunk_idat_number = 0;
	size_t data_size         = 0;
	size_t chunk_current     = 0;
	Chunk c = chunk_get(png, &chunk_current);
	while (c.type != IEND) {
		if (c.type == IDAT) {
			chunk_idat_number += 1;
			data_size += c.length;
		}
		c = chunk_get(png, &chunk_current);
	}
	printf("Number of IDAT chunks = %zu\n", chunk_idat_number);
	printf("all chunks need %zu Byte memory.\n", data_size);
	*out = data_size;
}

void write_stream_compressed(FILE* png, uint8_t* buffer) {

	size_t offset = 0;
	size_t chunk_current = 0;

	Chunk c = chunk_get(png, &chunk_current);
	long pos_after_chunk = ftell(png);
	printf("c.length = %u\n", c.length);
	fseek(png, -(long)(c.length + 4), SEEK_CUR);
	ihdr_info_print(png, c);
	fseek(png, pos_after_chunk, SEEK_SET);
	while (c.type != IEND) {
		if (c.type == IDAT) {

			long pos_after_chunk = ftell(png);

			fseek(png, -(long)(c.length + 4), SEEK_CUR);

			fread(buffer + offset, 1, c.length, png);
			offset += c.length;

			fseek(png, pos_after_chunk, SEEK_SET);
		}
		c = chunk_get(png, &chunk_current);
	}

	return;
}

int main(int argc, char** argv) {

	pname = argv[0];
	if (argc < 2) usage();

	size_t arg = 2;
	while (arg < argc) {
		if (strncmp(argv[arg++], "-o", 2) == 0) {
			opt.o_flg = true;
			strcpy(opt.o_arg, argv[arg++]);
		}
	}

	FILE* in_png = fopen(argv[1], "rb");

	char base_name[BUF_MAX];
	make_base_name(argv[1], base_name);

	char base_path[BUF_MAX];
	make_base_path(argv[1], base_path);

	char out_path[BUF_MAX];
	if (opt.o_flg) {
		if (opt.o_arg[strlen(opt.o_arg)-1] != '/') {
			snprintf(out_path, sizeof(out_path), "%s",
				 opt.o_arg);
		} else {
			snprintf(out_path, sizeof(out_path), "%stexture_%s.bin",
				 opt.o_arg, base_name);
		}

	} else {
		snprintf(out_path, sizeof(out_path), "%stexture_%s.bin",
			 base_path, base_name);
	}
	FILE* out_bin = fopen(out_path, "wb");

	signature_verify(in_png);

	size_t chunks_idat_size = 0;
	chunk_idat_number_calculate(in_png, &chunks_idat_size);

	rewind(in_png);
	signature_verify(in_png); // hack to skip to first idat chunk

	uint8_t* buffer = calloc((size_t) chunks_idat_size, 1);
	write_stream_compressed(in_png, buffer);

	int rc = decompress_idat_and_write_rgba(buffer, chunks_idat_size,
            					ihdr.width, ihdr.height,
						ihdr.bit_depth, ihdr.color_type,
						out_path);
	if (rc != 0) {
		fprintf(stderr, "Failed to decompress/unfilter: %d\n", rc);
	}


	free(buffer);
	fclose(in_png);
	fclose(out_bin);

	return 0;
}

