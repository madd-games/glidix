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

#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "libgpm.h"

GPMInstallRequest* gpmCreateInstallRequest(GPMContext *ctx, int *error)
{
	GPMInstallRequest *req = (GPMInstallRequest*) malloc(sizeof(GPMInstallRequest));
	if (req == NULL)
	{
		if (error != NULL) *error = GPM_ERR_ALLOC;
		return NULL;
	};
	
	req->ctx = ctx;
	req->first = NULL;
	req->last = NULL;
	
	return req;
};

static int addPkg(GPMInstallRequest *req, const char *pkgname, uint32_t version, GPMDecoder *strm, char *depcmd, char *setup)
{
	GPMInstallNode *scan;
	for (scan=req->first; scan!=NULL; scan=scan->next)
	{
		if (strcmp(scan->name, pkgname) == 0)
		{
			if (scan->version < version || (scan->version == version && strm != NULL))
			{
				scan->version = version;
				scan->strm = strm;
				scan->depcmd = depcmd;
				scan->setup = setup;
			};
			
			return 0;
		};
	};
	
	// not currently in the list, add it
	GPMInstallNode *node = (GPMInstallNode*) malloc(sizeof(GPMInstallNode));
	if (node == NULL) return -1;
	
	node->next = NULL;
	node->name = strdup(pkgname);
	node->version = version;
	node->strm = strm;
	node->depcmd = depcmd;
	node->setup = setup;
	
	if (req->last == NULL)
	{
		req->first = req->last = node;
	}
	else
	{
		req->last->next = node;
		req->last = node;
	};
	
	return 0;
};

void gpmFormatVersion(char *buffer, uint32_t version)
{
	if (version == 0)
	{
		strcpy(buffer, "none");
		return;
	};
	
	uint8_t rc = version & 0xFF;
	uint8_t patch = (version >> 8) & 0xFF;
	uint8_t minor = (version >> 16) & 0xFF;
	uint8_t major = (version >> 24) & 0xFF;
	
	if (rc == 0xFF)
	{
		sprintf(buffer, "%hhu.%hhu.%hhu", major, minor, patch);
	}
	else
	{
		sprintf(buffer, "%hhu.%hhu.%hhu-rc%hhu", major, minor, patch, rc);
	};
};

int gpmInstallRequestMIP(GPMInstallRequest *req, const char *filename, int *error)
{
	GPMDecoder *strm = gpmCreateDecoder(filename, error);
	if (strm == NULL) return -1;
	
	uint64_t features;
	if (gpmRead(strm, &features, 8) != 8)
	{
		gpmDestroyDecoder(strm);
		if (error != NULL) *error = GPM_ERR_SUPPORT;
		return -1;
	};
	
	if (features != 0)
	{
		gpmDestroyDecoder(strm);
		if (error != NULL) *error = GPM_ERR_SUPPORT;
		return -1;
	};
	
	char *pkgname = NULL;
	uint32_t version = 0;
	char *setup = NULL;
		
	char *depcmd = strdup("");

	int ok = 1;
	while (1)
	{
		uint64_t recSize;
		if (gpmRead(strm, &recSize, 8) != 8)
		{
			ok = 0;
			break;
		};
		
		uint8_t type;
		if (gpmRead(strm, &type, 1) != 1)
		{
			ok = 0;
			break;
		};
		
		if (type == 0xFF)
		{
			// end of control area
			break;
		}
		else if (type == 0x80)
		{
			if (pkgname != NULL)
			{
				ok = 0;
				break;
			};
			
			if (gpmRead(strm, &version, 4) != 4)
			{
				ok = 0;
				break;
			};
			
			size_t nameSize = recSize - 13;
			pkgname = (char*) malloc(nameSize+1);
			if (pkgname == NULL)
			{
				ok = 0;
				break;
			};
			
			if (gpmRead(strm, pkgname, nameSize) != nameSize)
			{
				ok = 0;
				break;
			};
			
			pkgname[nameSize] = 0;
		}
		else if (type == 0x81)
		{
			uint32_t depver;
			if (gpmRead(strm, &depver, 4) != 4)
			{
				ok = 0;
				break;
			};
			
			size_t nameSize = recSize - 13;
			char *depname = (char*) malloc(nameSize+1);
			if (depname == NULL)
			{
				ok = 0;
				break;
			};
			
			if (gpmRead(strm, depname, nameSize) != nameSize)
			{
				ok = 0;
				break;
			};
			
			depname[nameSize] = 0;
			if (addPkg(req, depname, depver, NULL, NULL, NULL) != 0)
			{
				ok = 0;
				break;
			};
			
			char verspec[64];
			gpmFormatVersion(verspec, depver);
			
			char *newdepcmd;
			asprintf(&newdepcmd, "%sdepends %s %s\n", depcmd, depname, verspec);
			free(depcmd);
			depcmd = newdepcmd;
			
			free(depname);
		}
		else if (type == 0x82)
		{
			size_t pathSize = recSize - 9;
			setup = (char*) malloc(pathSize+1);
			gpmRead(strm, setup, pathSize);
			setup[pathSize] = 0;
		}
		else
		{
			ok = 0;
			break;
		};
	};
	
	// check
	if (pkgname == NULL || !ok)
	{
		free(pkgname);
		gpmDestroyDecoder(strm);
		if (error != NULL) *error = GPM_ERR_FATAL;
		return -1;
	};
	
	// now add us
	if (addPkg(req, pkgname, version, strm, depcmd, setup) != 0)
	{
		free(pkgname);
		gpmDestroyDecoder(strm);
		if (error != NULL) *error = GPM_ERR_FATAL;
		return -1;
	};
	
	free(pkgname);
	
	// done
	return 0;
};

typedef struct
{
	GPMInstallRequest*			req;
	GPMContext*				ctx;
	const char*				pkgname;
	uint32_t				version;
} AddPackageData;

static void* addPackageTask(void *context)
{
	GPMTaskProgress *prog = (GPMTaskProgress*) context;
	AddPackageData *data = (AddPackageData*) prog->data;
	
	uint32_t currentVersion = gpmGetPackageVersion(data->ctx, data->pkgname);
	if (data->version == 0) data->version = gpmGetLatestVersion(data->ctx, data->pkgname);
	if (data->version == 0)
	{
		if (currentVersion != 0)
		{
			asprintf(&prog->status, "Package `%s' is already in the latest version", data->pkgname);
			prog->done = 1;
			return NULL;
		}
		else
		{
			asprintf(&prog->status, "Package `%s' was not found", data->pkgname);
			prog->done = 1;
			return NULL;
		};
	};
	
	if (data->version <= currentVersion)
	{
		asprintf(&prog->status, "Package `%s' is already installed in the selected, or newer, version", data->pkgname);
		prog->done = 1;
		return NULL;
	};
	
	char *url = gpmGetPackageURL(data->ctx, data->pkgname, data->version);
	if (url == NULL)
	{
		char verbuf[64];
		gpmFormatVersion(verbuf, data->version);
		asprintf(&prog->status, "Failed to find a URL for package `%s', version %s", data->pkgname, verbuf);
		prog->done = 1;
		return NULL;
	};
	
	// download the file
	char *temp;
	static int counter = 1;
	asprintf(&temp, "%s/%d.mip", data->ctx->tempdir, __sync_fetch_and_add(&counter, 1));
	FILE *out = fopen(temp, "wb");
	if (out == NULL)
	{
		asprintf(&prog->status, "Failed to create temporary file `%s'", temp);
		free(temp);
		free(url);
		prog->done = 1;
		return NULL;
	};
	
	int status = gpmFetch(data->ctx, out, url, &prog->progress);
	fclose(out);
	
	if (status == -1)
	{
		asprintf(&prog->status, "Failed to download `%s'. See GPM log for details.", url);
		free(url);
		free(temp);
		prog->done = 1;
		return NULL;
	};
	
	free(url);
	
	// try adding to the installation request
	int error;
	if (gpmInstallRequestMIP(data->req, temp, &error) != 0)
	{
		asprintf(&prog->status, "Failed to add to installation request: error %d", error);
		free(temp);
		prog->done = 1;
		return NULL;
	};
	
	// all worked
	prog->status = NULL;
	prog->done = 1;
	return NULL;
};

GPMTaskProgress* gpmInstallRequestAdd(GPMInstallRequest *req, const char *pkgname, uint32_t version)
{
	GPMTaskProgress *prog = (GPMTaskProgress*) malloc(sizeof(GPMTaskProgress));
	if (prog == NULL) return NULL;
	
	AddPackageData *data = (AddPackageData*) malloc(sizeof(AddPackageData));
	data->req = req;
	data->ctx = req->ctx;
	data->pkgname = pkgname;
	data->version = version;
	
	prog->progress = 0;
	prog->done = 0;
	prog->status = NULL;
	prog->data = data;
	
	if (pthread_create(&prog->thread, NULL, addPackageTask, prog) != 0)
	{
		free(prog);
		return NULL;
	};
	
	return prog;
};

static void* installTask(void *context)
{
	GPMTaskProgress *prog = (GPMTaskProgress*) context;
	GPMInstallRequest *req = (GPMInstallRequest*) prog->data;
	
	size_t totalSize = 0;
	
	// check if all dependencies are met
	GPMInstallNode *node;
	for (node=req->first; node!=NULL; node=node->next)
	{
		if (node->strm == NULL)
		{
			// we don't have a MIP file for this, so make sure we already have that
			// package in the database
			uint32_t currentVersion = gpmGetPackageVersion(req->ctx, node->name);
			if (currentVersion < node->version)
			{
				char depver[64];
				gpmFormatVersion(depver, node->version);
				char havever[64];
				gpmFormatVersion(havever, currentVersion);
				asprintf(&prog->status, "dependency not satisfied: need %s %s but have %s",
						node->name, depver, havever);
				prog->done = 1;
				return NULL;
			};
		}
		else
		{
			totalSize += node->strm->fileSize;
		};
	};
	
	// install the packages
	size_t sizeInstalled = 0;
	for (node=req->first; node!=NULL; node=node->next)
	{
		if (node->strm != NULL)
		{
			char *pkgpath;
			asprintf(&pkgpath, "%s/%s.pkg", req->ctx->basedir, node->name);
			
			FILE *pkg = fopen(pkgpath, "w");
			free(pkgpath);
			if (pkg == NULL)
			{
				asprintf(&prog->status, "Failed to open package descriptor file");
				prog->done = 1;
				return NULL;
			};
			
			char verspec[64];
			gpmFormatVersion(verspec, node->version);
			fprintf(pkg, "version %s\n", verspec);
			fprintf(pkg, "%s", node->depcmd);
			
			while (1)
			{
				size_t doneSize = sizeInstalled + lseek(node->strm->fd, 0, SEEK_CUR);
				prog->progress = (float) doneSize / (float) totalSize;
				
				uint64_t recSize;
				if (gpmRead(node->strm, &recSize, 8) != 8)
				{
					break;
				};
				
				uint8_t rectype;
				if (gpmRead(node->strm, &rectype, 1) != 1)
				{
					break;
				};
				
				if (rectype == 0x01)
				{
					// directory
					// first ignore the reserved byte
					uint8_t ignore;
					gpmRead(node->strm, &ignore, 1);
					
					uint16_t mode;
					gpmRead(node->strm, &mode, 2);
					
					size_t nameSize = recSize - 12;
					char *dirpath = (char*) malloc(nameSize+1);
					dirpath[nameSize] = 0;
					
					gpmRead(node->strm, dirpath, nameSize);
					
					if (dirpath[0] == '/')
					{
						char *fullpath;
						asprintf(&fullpath, "%s%s", req->ctx->dest, dirpath);
						mkdir(fullpath, (mode_t) mode);
						fprintf(pkg, "dir %s\n", dirpath);
						free(fullpath);
					};
					
					free(dirpath);
				}
				else if (rectype == 0x02)
				{
					// symbolic link
					size_t nameSize = 0;
					char namebuf[1024];
					char *put = namebuf;
					
					char c;
					do
					{
						gpmRead(node->strm, &c, 1);
						*put++ = c;
						nameSize++;
					} while (c != 0);
					
					size_t targetSize = recSize - 9 - nameSize;
					char *target = (char*) malloc(targetSize + 1);
					target[targetSize] = 0;
					gpmRead(node->strm, target, targetSize);
					
					if (namebuf[0] == '/')
					{
						char *fullpath;
						asprintf(&fullpath, "%s%s", req->ctx->dest, namebuf);
						symlink(target, fullpath);
						fprintf(pkg, "file %s\n", namebuf);
						free(fullpath);
					};
				}
				else if (rectype == 0x03)
				{
					// file
					uint8_t ignore;
					gpmRead(node->strm, &ignore, 1);
					
					uint16_t mode;
					gpmRead(node->strm, &mode, 2);
					
					size_t nameSize = 0;
					char namebuf[1024];
					char *put = namebuf;
					
					char c;
					do
					{
						gpmRead(node->strm, &c, 1);
						*put++ = c;
						nameSize++;
					} while (c != 0);
					
					size_t fileSize = recSize - 12 - nameSize;
					if (namebuf[0] != '/')
					{
						asprintf(&prog->status, "%s: invalid file path: %s", node->name, namebuf);
						prog->done = 1;
						return NULL;
					};
					
					fprintf(pkg, "file %s\n", namebuf);
					
					char *fullpath;
					asprintf(&fullpath, "%s%s", req->ctx->dest, namebuf);
					int fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, (mode_t) mode);
					if (fd == -1)
					{
						asprintf(&prog->status, "%s: cannot open %s: %s", node->name, fullpath, strerror(errno));
						prog->done = 1;
						return NULL;
					};
					
					char copybuf[8*1024];
					while (fileSize)
					{
						size_t copyNow = 8*1024;
						if (copyNow > fileSize) copyNow = fileSize;
						
						gpmRead(node->strm, copybuf, copyNow);
						write(fd, copybuf, copyNow);
						
						fileSize -= copyNow;
						doneSize = sizeInstalled + lseek(node->strm->fd, 0, SEEK_CUR);
						prog->progress = (float) doneSize / (float) totalSize;
					};
					
					close(fd);
				}
				else
				{
					fclose(pkg);
					asprintf(&prog->status, "%s: invalid record: %02hhX", node->name, rectype);
					prog->done = 1;
					return NULL;
				};
			};
			
			fclose(pkg);
			sizeInstalled += node->strm->fileSize;
			
			// allow the setup script to run (if any)
			if (node->setup != NULL)
			{
				char *fullpath;
				asprintf(&fullpath, "%s%s", req->ctx->dest, node->setup);
				pid_t pid = fork();
				if (pid == 0)
				{
					char *lastSlash = strrchr(fullpath, '/');
					if (lastSlash != NULL) *lastSlash = 0;
					char *exname = lastSlash+1;
					
					chdir("/");
					chdir(fullpath);
					execl(exname, exname, NULL);
					_exit(1);
				}
				else
				{
					waitpid(pid, NULL, 0);
				};
				free(fullpath);
			};
			
			gpmDestroyDecoder(node->strm);
			node->strm = NULL;
		};
	};

	// done
	prog->done = 1;
	return NULL;
};

GPMTaskProgress* gpmRunInstall(GPMInstallRequest *req)
{
	GPMTaskProgress *prog = (GPMTaskProgress*) malloc(sizeof(GPMTaskProgress));
	if (prog == NULL) return NULL;
	
	prog->progress = 0;
	prog->done = 0;
	prog->status = NULL;
	prog->data = req;
	
	if (pthread_create(&prog->thread, NULL, installTask, prog) != 0)
	{
		free(prog);
		return NULL;
	};
	
	return prog;
};

int gpmIsComplete(GPMTaskProgress *prog)
{
	return prog->done;
};

float gpmGetProgress(GPMTaskProgress *prog)
{
	return prog->progress;
};

char* gpmWait(GPMTaskProgress *prog)
{
	pthread_join(prog->thread, NULL);
	return prog->status;
};
