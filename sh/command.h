/*
	Glidix Shell

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

#ifndef COMMAND_H_
#define COMMAND_H_

/**
 * File redirection entry.
 */
typedef struct CmdRedir_
{
	/**
	 * Next entry.
	 */
	struct CmdRedir_ *next;
	
	/**
	 * Original file descriptor to redirect.
	 */
	int fd;
	
	/**
	 * Name of the file to redirect to (or descriptor specification in the form "&3").
	 */
	char *targetName;
	
	/**
	 * Flags to open the file with.
	 */
	int oflag;
} CmdRedir;

/**
 * Represents a member of a command group.
 */
typedef struct CmdMember_
{
	/**
	 * Command tokens; environment settings followed by the command, followed by
	 * arguments to the command.
	 */
	char **tokens;
	
	/**
	 * Number of tokens.
	 */
	int numTokens;
	
	/**
	 * Next and previous member of the group.
	 */
	struct CmdMember_* prev;
	struct CmdMember_* next;
	
	/**
	 * Head of the file redirection list.
	 */
	CmdRedir* redir;
	
	/**
	 * Exit status of this command, as returned by wait().
	 */
	int status;
	
	/**
	 * PID of the member. -1 means already dead.
	 */
	pid_t pid;
} CmdMember;

/**
 * Represents a command group.
 */
typedef struct CmdGroup_
{
	/**
	 * Next group in the chain.
	 */
	struct CmdGroup_* next;
	
	/**
	 * First command in the group (the group leader). The entries are sorted right-to-left!
	 */
	CmdMember *firstMember;
	
	/**
	 * If true, this group must return success in order to run the next one
	 * (otherwise it must return failure).
	 */
	int mustSucceed;
} CmdGroup;

/**
 * Represents a command chain.
 */
typedef struct
{
	/**
	 * First member of the chain.
	 */
	CmdGroup *firstGroup;
} CmdChain;

/**
 * Run the specified command, and return the exit status (which must be parsed by the macros
 * in <sys/wait.h>). The 'cmd' string must already be pre-processed. This function destroys
 * the contents of 'cmd', but does not call free() on it!
 */
int cmdRun(char *cmd);

#endif
