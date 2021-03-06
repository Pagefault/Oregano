
#include "stdafx.h"

#include "dllDebugInterface.hpp"

#ifdef _DEBUG

#define RETURN_STATS_FOR_REG(REG_NAME, REG_ID) \
LOG_PARSER_API void statistics##REG_NAME(LogParser * logParser, DWORD * pagesInUse, DWORD * totalPages) { \
    StatisticsInfo * stats = logParser->statisticsReg(REG_ID); \
    *pagesInUse = stats->pagesInUse; \
    *totalPages = stats->totalPages; \
    delete stats; \
    stats = NULL; \
}

#ifdef X86
RETURN_STATS_FOR_REG(Eip, REG_ID_EIP);
RETURN_STATS_FOR_REG(Edi, REG_ID_EDI);
RETURN_STATS_FOR_REG(Esi, REG_ID_ESI);
RETURN_STATS_FOR_REG(Ebp, REG_ID_EBP);
RETURN_STATS_FOR_REG(Ebx, REG_ID_EBX);
RETURN_STATS_FOR_REG(Edx, REG_ID_EDX);
RETURN_STATS_FOR_REG(Ecx, REG_ID_ECX);
RETURN_STATS_FOR_REG(Eax, REG_ID_EAX);
RETURN_STATS_FOR_REG(Eflags, REG_ID_EFLAGS);
RETURN_STATS_FOR_REG(Esp, REG_ID_ESP);
#elif AMD64
RETURN_STATS_FOR_REG(Rip, REG_ID_RIP);
RETURN_STATS_FOR_REG(Rdi, REG_ID_RDI);
RETURN_STATS_FOR_REG(Rsi, REG_ID_RSI);
RETURN_STATS_FOR_REG(Rbp, REG_ID_RBP);
RETURN_STATS_FOR_REG(Rbx, REG_ID_RBX);
RETURN_STATS_FOR_REG(Rdx, REG_ID_RDX);
RETURN_STATS_FOR_REG(Rcx, REG_ID_RCX);
RETURN_STATS_FOR_REG(Rax, REG_ID_RAX);
RETURN_STATS_FOR_REG(R8, REG_ID_R8);
RETURN_STATS_FOR_REG(R9, REG_ID_R9);
RETURN_STATS_FOR_REG(R10, REG_ID_R10);
RETURN_STATS_FOR_REG(R11, REG_ID_R11);
RETURN_STATS_FOR_REG(R12, REG_ID_R12);
RETURN_STATS_FOR_REG(R13, REG_ID_R13);
RETURN_STATS_FOR_REG(R14, REG_ID_R14);
RETURN_STATS_FOR_REG(R15, REG_ID_R15);
RETURN_STATS_FOR_REG(Rcs, REG_ID_RCS);
RETURN_STATS_FOR_REG(Rflags, REG_ID_RFLAGS);
RETURN_STATS_FOR_REG(Rsp, REG_ID_RSP);
RETURN_STATS_FOR_REG(Rss, REG_ID_RSS);
#endif
RETURN_STATS_FOR_REG(ThreadId, THREAD_ID);

LOG_PARSER_API void statisticsMemory(LogParser * logParser, DWORD * pagesInUse, DWORD * totalPages)
{
    StatisticsInfo * stats = logParser->statisticsMemory();

    *pagesInUse = stats->pagesInUse;
    *totalPages = stats->totalPages;

    delete stats;
    stats = NULL;
}

#endif // _DEBUG
