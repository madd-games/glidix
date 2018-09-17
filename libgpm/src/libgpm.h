/*
	Glidix Package Manager

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef GPM_H_
#define GPM_H_

#include <inttypes.h>
#include <sys/types.h>
#include <pthread.h>

/**
 * libgpm errors.
 */
#define	GPM_ERR_NONE			0			/* no error */
#define	GPM_ERR_SUPPORT			1			/* no support */
#define	GPM_ERR_OPENFILE		2			/* could not open file */
#define	GPM_ERR_ALLOC			3			/* cannot allocate buffer */
#define	GPM_ERR_NOTMIP			4			/* not a MIP file */
#define	GPM_ERR_LOCK			5			/* could not acquire lock */
#define	GPM_ERR_DUP			6			/* duplicate */
#define	GPM_ERR_FATAL			7			/* fatal error; state corrupted */

/**
 * Maximum possible size of a block after decompression (fixed at 64KB by the spec).
 */
#define	GPM_BLOCK_MAX			(64*1024)

/**
 * Represents a decoder stream for MIP files. This can be created using gpmCreateDecoder(). This is
 * used to decompress the payload of MIP files, and should not be used directly by applications.
 */
typedef struct
{
	/**
	 * Current decoded block, its size, and our current position within it.
	 */
	uint8_t				block[GPM_BLOCK_MAX];
	size_t				blockSize;
	size_t				blockPos;
	
	/**
	 * The file we are decoding from. It is positioned to the beginning of the next block header.
	 */
	int				fd;
	
	/**
	 * Size of MIP file.
	 */
	size_t				fileSize;
} GPMDecoder;

/**
 * Represents a database context. This is created using gpmCreateContext().
 */
typedef struct
{
	/**
	 * Destination.
	 */
	char*				dest;
	
	/**
	 * Path to base directory ($DEST/etc/gpm).
	 */
	char*				basedir;
	
	/**
	 * Open file descriptor for the $BASEDIR/.lock file, used to hold the
	 * lock for this database.
	 */
	int				lockfd;
	
	/**
	 * File pointer for the GPM log file.
	 */
	FILE*				log;
} GPMContext;

/**
 * A node on the list of packages to install.
 */
typedef struct GPMInstallNode_
{
	/**
	 * Link.
	 */
	struct GPMInstallNode_*		next;
	
	/**
	 * Name of the package.
	 */
	char*				name;
	
	/**
	 * Version.
	 */
	uint32_t			version;
	
	/**
	 * List of "depends" commands for the .pkg file.
	 */
	char*				depcmd;
	
	/**
	 * Path to the setup script.
	 */
	char*				setup;
	/**
	 * Decoder stream for the MIP file if known. It points to the first data record header.
	 * NULL if not known.
	 */
	GPMDecoder*			strm;
} GPMInstallNode;

/**
 * Represents an installation request on a database.
 */
typedef struct
{
	/**
	 * The database in question.
	 */
	GPMContext*			ctx;
	
	/**
	 * First and last "install node".
	 */
	GPMInstallNode*			first;
	GPMInstallNode*			last;
} GPMInstallRequest;

/**
 * Represents installation progress.
 */
typedef struct
{
	/**
	 * Current progress as a value 0-1.
	 */
	float				progress;
	
	/**
	 * Gets set to 1 when the task is done.
	 */
	int				done;
	
	/**
	 * Task status, after 'done' is set to 1. If this is NULL, the task has been successfully
	 * completed. Otherwise, it is set to a string (allocated on the heap) containing an error
	 * message from the task.
	 */
	char*				status;
	
	/**
	 * The thread executing the task; must be joined.
	 */
	pthread_t			thread;
	
	/**
	 * Arbitrary data.
	 */
	void*				data;
} GPMTaskProgress;

/**
 * MIP header.
 */
typedef struct
{
	uint8_t			mhMagic[3];
	uint8_t			mhFlags;
} MIPHeader;

/**
 * Create a decoder on the specified MIP file. On success, returns a GPMDecoder which can later be passed
 * to gpmRead(). On error, it returns NULL, and if 'error' is not NULL, it is set to an error number, which
 * could be:
 *
 * GPM_ERR_SUPPORT		The file uses unsupported compression suites.
 * GPM_ERR_ALLOC		Could not allocate the stream descriptor.
 * GPM_ERR_NOTMIP		The specified file is not a MIP.
 * GPM_ERR_OPENFILE		Could not open file.
 */
GPMDecoder* gpmCreateDecoder(const char *filename, int *error);

/**
 * Read data from the decoder, into the specified buffer, and return the number of bytes read. If it is less
 * than requested, it means end-of-file was reached.
 */
size_t gpmRead(GPMDecoder *strm, void *buffer, size_t size);

/**
 * Destroy a decoder and close the MIP file.
 */
void gpmDestroyDecoder(GPMDecoder *decoder);

/**
 * Return the current progress through the MIP file as a float between 0 and 1. It is safe to call this from
 * other threads than the one that created it.
 */
float gpmGetDecoderProgress(GPMDecoder *decoder);

/**
 * Create a database context with the specified destination. If 'dest' is NULL, then the filesystem root
 * is used as default. On success, returns a GPMContext pointer, which can be used to handle the database.
 * On error, returns NULL, and if 'error' is not NULL, it sets it to an error number:
 *
 * GPM_ERR_ALLOC		A memory allocation error occured.
 * GPM_ERR_LOCK			Could not acquire the database lock.
 */
GPMContext* gpmCreateContext(const char *dest, int *error);

/**
 * Destroy a database context.
 */
void gpmDestroyContext(GPMContext *ctx);

/**
 * Get the currently-installed version of the specified package. Returns 0 if not installed.
 */
uint32_t gpmGetPackageVersion(GPMContext *ctx, const char *name);

/**
 * Rebuild the repository index. Information about the process is written to the GPM log. Returns 0 on success,
 * or -1 on error.
 */
int gpmReindex(GPMContext *ctx);

/**
 * Create an installation request on the specified database. Returns an empty request on success, or NULL
 * on error; if 'error' is not NULL, it is set to an error number:
 *
 * GPM_ERR_ALLOC		Could not allocated memory.
 */
GPMInstallRequest* gpmCreateInstallRequest(GPMContext *ctx, int *error);

/**
 * Add a MIP file to an installation request. Return 0 on success, -1 on error; if 'error' is not NULL,
 * it is set to an error number:
 *
 * GPM_ERR_ALLOC		Could not allocate memory.
 * GPM_ERR_DUP			A MIP file with the same package name was already marked for installation.
 * GPM_ERR_NOTMIP		Not a MIP file.
 * GPM_ERR_OPENFILE		Could not open file.
 * GPM_ERR_SUPPORT		This MIP file uses unsupported features.
 * GPM_ERR_FATAL		Fatal error; request is now corrupted, do not use.
 */
int gpmInstallRequestMIP(GPMInstallRequest *req, const char *filename, int *error);

/**
 * Run an installation task, and return a structure that can be used to track it. Returns NULL on memory
 * allocation error.
 */
GPMTaskProgress* gpmRunInstall(GPMInstallRequest *req);

/**
 * Returns true if the specified task has been completed.
 */
int gpmIsComplete(GPMTaskProgress *prog);

/**
 * Return the current progress of a task, from 0 to 1.
 */
float gpmGetProgress(GPMTaskProgress *prog);

/**
 * Wait for the specified task to finish and return the status. NULL means it succeeded; otherwise, it is
 * a string containing an error message.
 */
char* gpmWait(GPMTaskProgress *prog);

#endif
