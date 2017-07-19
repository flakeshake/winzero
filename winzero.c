#include <stdlib.h>
#include <stdio.h>		// fflush()
#include <string.h>		// memset()
#include <math.h>		// for abs()
#include <unistd.h>		// uint_xxt
#include <stdint.h>
#include <stdbool.h>		// for true and false

#include <windows.h>

/*
winzero

Write empty files on Windows , catch I/O errors early.

(c) 2017 Dennis Thekumthala
Licensed under the conditions specified in the accompanying LICENSE file.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH 
THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#define VERSION_STRING "0.0.1"

#define ERROR_WRONG_INVOCATION 32
#define ERROR_FILESIZE_TOO_BIG 33
#define ERROR_FILE_CREATION 64
#define ERROR_NO_MEMORY 65
#define ERROR_FILE_WRITE 127

char last_error_buffer[64];
const unsigned int max_fsize_in_MEBI = 1000000;
const unsigned int MEBI = 1048576;
const unsigned int GIBI = 1073741824;

void print_err(char *in)
{
	if (in == NULL)
		in = "Operation failed";

	DWORD dwError = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,	//dwFlags
		      NULL,	// lpSource
		      dwError,	// dwMessageID
		      0,	// dwLanguageID  , zero means autoselect
		      &last_error_buffer[0],	// lpBuffer
		      64,	// nSize
		      NULL);	// Arguments
	fflush(stdout);
	printf("\n%s because %s", in, last_error_buffer);
}

HANDLE start_zero_file(char *infile)
{
	HANDLE retval = INVALID_HANDLE_VALUE;
	retval = CreateFile(infile,	// file name 
			    GENERIC_WRITE,	// open for write
			    0,	// do not share 
			    NULL,	// default security 
			    CREATE_NEW,	// fails if file exists 
			    FILE_ATTRIBUTE_NORMAL,	// normal file 
			    NULL);	// no template 
	return retval;
}

void die(char *in, int status_code)
{
	print_err(in);
	printf("Aborting.\n");
	exit(status_code);
}

char *generate_pattern_chunk(uint64_t chunk_bytes, char init_byte)
{
	static bool already_run = false;
	static char *chunk;
	if (!already_run) {
		chunk = malloc(chunk_bytes);
		if (chunk == NULL) {
			die("Could not allocate memory for template chunk",
			    ERROR_NO_MEMORY);
		}
		memset(chunk, init_byte, chunk_bytes);
		already_run = true;
	}
	return chunk;
}

char *generate_implicit_zero_chunk(uint64_t chunk_bytes)
{
	static bool already_run = false;
	static char *chunk;
	if (!already_run) {
		chunk = malloc(chunk_bytes);
		if (chunk == NULL) {
			die("Could not allocate memory for template chunk",
			    ERROR_NO_MEMORY);
		}
		already_run = true;
	}
	return chunk;
}

int enlarge_file_by_chunk(HANDLE fhandle, char *chunk, uint64_t chunk_bytes,
			  bool explicit_write)
{
	DWORD num_written = 0;

	unsigned int retval;
	LARGE_INTEGER LiSize_bytes;
	LiSize_bytes.QuadPart = chunk_bytes;
	DWORD dwSize_bytes = chunk_bytes;

	if (explicit_write) {
		retval = WriteFile(fhandle,
				   chunk, dwSize_bytes, &num_written, NULL);
		if (num_written != dwSize_bytes) {
			print_err("Did not write the whole chunk");
		}
		if (retval == 0) {
			print_err("Writing chunk failed");
		}

	} else {
		DWORD dwPtr = INVALID_SET_FILE_POINTER;
		// move relative to current position
		dwPtr = SetFilePointerEx(fhandle,
					 LiSize_bytes, NULL, FILE_CURRENT);

		if (dwPtr == INVALID_SET_FILE_POINTER) {
			print_err("Could not set file pointer for this chunk");
		}
		retval = SetEndOfFile(fhandle);

		if (retval == 0) {
			print_err("Could not extend file with this chunk");
		}
	}

	// Commit changes to disk 
	retval = FlushFileBuffers(fhandle);
	return retval;
}

void end_zero_file(HANDLE fhandle)
{
	CloseHandle(fhandle);
}

char *print_progressbar(unsigned int buff_size, double percent)
{
	static bool already_run;
	static char *buff;
	static unsigned int final_size;
	const unsigned int max_progress_size = 100;

	if (!already_run) {
		final_size = buff_size;
		if (final_size > max_progress_size)
			final_size = max_progress_size;
		buff = malloc(final_size);
		if (buff == NULL) {
			die("Could not allocate memory for progress bar",
			    ERROR_NO_MEMORY);
		}
		buff[final_size] = 0x00;
		already_run = true;
	}

	unsigned int curr_segs = percent * final_size;
	unsigned int num_spaces = final_size - curr_segs;

	memset(buff, '=', curr_segs);
	memset(buff + curr_segs, ' ', num_spaces);

	printf("\rProgress: |%s| %u %%", buff, abs(percent * 100));
	fflush(stdout);

	return buff;
}

void print_usage()
{
	printf("\nwinzero " VERSION_STRING "\n");
	printf("Usage: winzero FILENAME SIZE\nCreate empty files. \n");
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Wrong number of arguments , aborting.\n");
		print_usage();
		return ERROR_WRONG_INVOCATION;
	}

	const unsigned int progress_size = 80;

	char *bar_buffer;
	char *template;
	char fname[256];

	unsigned int max_count = 100;
	double written_MEBI = 0.0;
	uint64_t written_bytes = 0;

	unsigned int fsize_MEBI = atoi(argv[2]);

	memset(&last_error_buffer[0], 0x00, 64);
	strncpy(fname, argv[1], 255);

	if (fsize_MEBI > max_fsize_in_MEBI) {
		printf("Invalid file size. Maximum size is %u MiB\n",
		       max_fsize_in_MEBI);
		return ERROR_FILESIZE_TOO_BIG;
	}
	// limit size of template chunk to 100 MiB max.
	if (fsize_MEBI > 10000)
		max_count = 1000;

	uint32_t chunk_bytes = (fsize_MEBI / max_count) * MEBI;

	HANDLE target = start_zero_file(fname);

	if (target == INVALID_HANDLE_VALUE) {
		die("Could not create file", ERROR_FILE_CREATION);
	}

	printf("Creating file %s , size %u MiB.\n", fname, fsize_MEBI);
#ifdef __WIN32
	// printf("Using 100 chunks of %PRIu64 bytes size.\n" , chunk_bytes);
	printf("Using %u chunks of %u bytes size.\n", max_count, chunk_bytes);
#else
	printf("Using %u chunks of %llu bytes size.\n", max_count, chunk_bytes);
#endif

	// main loop
	for (unsigned int count = 0; count < max_count; count++) {
		template = generate_implicit_zero_chunk(chunk_bytes);
		int retval =
		    enlarge_file_by_chunk(target, template, chunk_bytes, false);
		if (retval == 0) {
			print_err("Could not enlarge file");
			printf("Written %g MiB , aborting at %g percent.\n",
			       written_MEBI, (double)count / max_count);
			return ERROR_FILE_WRITE;
		}
		written_bytes += chunk_bytes;
		written_MEBI = written_bytes / MEBI;
		bar_buffer =
		    print_progressbar(progress_size, (double)count / max_count);

#ifdef DEBUG
		Sleep(100);
#endif

	}

	printf("\nSuccessfully written %g MiB , closing file.\n", written_MEBI);
	end_zero_file(target);
	free(template);
	free(bar_buffer);
	printf("Finished.\n");
	exit(EXIT_SUCCESS);
}
