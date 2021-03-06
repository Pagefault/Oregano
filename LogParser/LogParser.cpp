
#include "stdafx.h"

#include "LogParser.hpp"

BOOL WINAPI DllMain(
            __in  HINSTANCE hinstDLL,
            __in  DWORD reason,
            __in  LPVOID reserved )
{

    switch( reason ) {
        case DLL_PROCESS_ATTACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        default:
            /* Unknown DLL callback reason */
            break;
    }

    return TRUE;
}

LogParser::LogParser() :
        opcodesSideEffects(NULL),
        eipLog(NULL),
        _lastCyclePtr(0),
        _maxCycle(0)
{
    // Init what I can't init above
    for (DWORD i = 0; i < NUMBER_OF_REGS; ++i)
    {
        reg[i] = NULL;
    }
}

void LogParser::parse(const char * logFile, Cycle maxCycle)
{
    _maxCycle = maxCycle;
    log.openFile(logFile);

    // Validate input
    if (strlen(logFile) >(MAX_FILE_NAME - strlen(LOG_DB_EXTENTION) - 1))
    {
        // Fuck this shit
        return;
    }
    // Is empty file, or invalid file
    if (log.isEof())
    {
        return;
    }
    char dbFileName[MAX_FILE_NAME] = { 0 };
    sprintf_s(dbFileName, MAX_FILE_NAME, "%s%s", logFile, LOG_DB_EXTENTION);
    dc = new PagedDataContainer(dbFileName);
    rootPage = (DBRootPage *)dc->obtainPage(0);
    DEBUG_PAGE_TAG(0, 'ROOT');
    if (rootPage->oreganoMagic != DB_OREGANO_MAGIC) {
        // This is a new DB
        rootPage->oreganoMagic = DB_OREGANO_MAGIC;
        rootPage->version = OREGANO_VERSION;
        PageIndex * regsIndexsPtr = &rootPage->eipRootPage;
        for (DWORD i = 0; NUMBER_OF_REGS > i; ++i) {
            dc->newPage(regsIndexsPtr);
            DEBUG_PAGE_TAG(*regsIndexsPtr, 'REG0' + i);
            regsIndexsPtr++;
        }
        dc->newPage(&rootPage->memoryRootPage);
        DEBUG_PAGE_TAG(rootPage->memoryRootPage, 'MEM_');
        logVersion = &rootPage->logVersion;
        processorType = &rootPage->processorType;
        logHasMemory = &rootPage->logHasMemory;
        _lastCyclePtr = &rootPage->lastCycle;
        initContext();
        initMemory();
        parse(log);
        dc->endOfData();
        for (DWORD i = 0; NUMBER_OF_REGS > i; ++i) {
            if (NULL != reg[i])
            {
                reg[i]->endOfData();
            }
        }
        memory->endOfData();
    }
    else {
        dc->setReadOnly();
        // Load information from DB
        assert(DB_OREGANO_MAGIC == rootPage->oreganoMagic);
        // TODO: Check version
        logVersion = &rootPage->logVersion;
        processorType = &rootPage->processorType;
        logHasMemory = &rootPage->logHasMemory;
        _lastCyclePtr = &rootPage->lastCycle;
        initContext();
        initMemory();
    }
}

void LogParser::initContext()
{
    eipLog = new EipLog(rootPage->eipRootPage, dc);
#ifdef X86
    reg[REG_ID_EIP] = NULL; /* eip */
    reg[REG_ID_EDI] = new RegLog("edi",       rootPage->ediRootPage,      dc, 'edi0');
    reg[REG_ID_ESI] = new RegLog("esi",       rootPage->esiRootPage,      dc, 'esi0');
    reg[REG_ID_EBP] = new RegLog("ebp",       rootPage->ebpRootPage,      dc, 'ebp0');
    reg[REG_ID_EBX] = new RegLog("ebx",       rootPage->ebxRootPage,      dc, 'ebx0');
    reg[REG_ID_EDX] = new RegLog("edx",       rootPage->edxRootPage,      dc, 'edx0');
    reg[REG_ID_ECX] = new RegLog("ecx",       rootPage->ecxRootPage,      dc, 'ecx0');
    reg[REG_ID_EAX] = new RegLog("eax",       rootPage->eaxRootPage,      dc, 'eax0');
    reg[REG_ID_ECS] = new RegLog("ecs",       rootPage->ecsRootPage,      dc, 'ecs0');
    reg[REG_ID_EFLAGS] = new RegLog("eflags", rootPage->eflagsRootPage,   dc, 'flg0');
    reg[REG_ID_ESP] = new RegLog("esp",       rootPage->espRootPage,      dc, 'esp0');
    reg[REG_ID_ESS] = NULL; // ess
#elif AMD64
    reg[REG_ID_RIP] = new RegLog("rip",       rootPage->eipRootPage,    dc, 'rip0');
    reg[REG_ID_RDI] = new RegLog("rdi",       rootPage->rdiRootPage,    dc, 'rdi0');
    reg[REG_ID_RSI] = new RegLog("rsi",       rootPage->rsiRootPage,    dc, 'rsi0');
    reg[REG_ID_RBP] = new RegLog("rbp",       rootPage->rbpRootPage,    dc, 'rbp0');
    reg[REG_ID_RBX] = new RegLog("rbx",       rootPage->rbxRootPage,    dc, 'rbx0');
    reg[REG_ID_RDX] = new RegLog("rdx",       rootPage->rdxRootPage,    dc, 'rdx0');
    reg[REG_ID_RCX] = new RegLog("rcx",       rootPage->rcxRootPage,    dc, 'rcx0');
    reg[REG_ID_RAX] = new RegLog("rax",       rootPage->raxRootPage,    dc, 'rax0');
    reg[REG_ID_R8]  = new RegLog("r8",        rootPage->r8RootPage,     dc, 'r8_0');
    reg[REG_ID_R9]  = new RegLog("r9",        rootPage->r9RootPage,     dc, 'r9_0');
    reg[REG_ID_R10] = new RegLog("r10",       rootPage->r10RootPage,    dc, 'r100');
    reg[REG_ID_R11] = new RegLog("r11",       rootPage->r11RootPage,    dc, 'r110');
    reg[REG_ID_R12] = new RegLog("r12",       rootPage->r12RootPage,    dc, 'r120');
    reg[REG_ID_R13] = new RegLog("r13",       rootPage->r13RootPage,    dc, 'r130');
    reg[REG_ID_R14] = new RegLog("r14",       rootPage->r14RootPage,    dc, 'r140');
    reg[REG_ID_R15] = new RegLog("r15",       rootPage->r15RootPage,    dc, 'r150');
    reg[REG_ID_RCS] = new RegLog("rcs",       rootPage->rcsRootPage,    dc, 'rcs0');
    reg[REG_ID_RFLAGS] = new RegLog("rflags", rootPage->rflagsRootPage, dc, 'flg0');
    reg[REG_ID_RSP] = new RegLog("rsp",       rootPage->rspRootPage,    dc, 'rsp0');
    reg[REG_ID_RSS] = new RegLog("rss",       rootPage->rssRootPage,    dc, 'rss0');
#endif
    reg[THREAD_ID]  = new RegLog("thread",    rootPage->threadIdRootPage, dc, 'thr0');
}

void LogParser::initMemory()
{
    memory = new Memory(rootPage->memoryRootPage, dc);
    assert(NULL != memory);
}

LogParser::~LogParser()
{
    for (DWORD i = 0; i < NUMBER_OF_REGS; ++i) {
        if (NULL != reg[i]) {
            delete reg[i];
            reg[i] = NULL;
        }
    }
    if (NULL != eipLog) {
        delete eipLog;
        eipLog = NULL;
    }
    PageIndex * regsIndexsPtr = &rootPage->eipRootPage;
    for (DWORD i = 0; NUMBER_OF_REGS > i; ++i) {
        dc->releasePage(*regsIndexsPtr);
        regsIndexsPtr++;
    }
    delete memory;
    memory = NULL;
    dc->releasePage(rootPage->memoryRootPage);
    dc->releasePage(0);
    delete dc;
    dc = NULL;
}

DWORD LogParser::readSectionTag()
{
    DWORD result = log.readDword();
    // 1 2 3 4 -> 4 3 2 1
    return (
        ((result >> 24)) |
        ((result >>  8) & 0x0000ff00) |
        ((result <<  8) & 0x00ff0000) |
        ((result << 24) & 0xff000000) );
}


BYTE LogParser::getByte( Address address )
{
    return memory->getByte(address);
}

WORD LogParser::getWord( Address address )
{
    return memory->getWord(address);
}

DWORD LogParser::getDword( Address address )
{
    return memory->getDword(address);
}

QWORD LogParser::getQword( Address address )
{
    return memory->getQword(address);
}

void LogParser::setByte( ADDRESS addr, BYTE val )
{
    memory->setByte(*_lastCyclePtr, addr, val);
}

BOOL LogParser::parse(FileReader & log)
{
    BOOL functionResult = FALSE;
    DWORD length;
    DWORD section;

    /* Read OREGANO header */
    length = log.readDword();
    section = readSectionTag();
    if ('OREG' == section) {
        *logVersion    = log.readDword();
        *processorType = log.readDword();
        *logHasMemory  = log.readDword();
        if ((8 == sizeof(ADDRESS)) && (PROCESSOR_TYPE_x86 == *processorType)) {
            /* Use logParser32 to parse 32bit log */
            functionResult = TRUE;
        } else if ((4 == sizeof(ADDRESS)) && (PROCESSOR_TYPE_AMD64 == *processorType)) {
            /* Use logParser64 to parse 64bit log */
            functionResult = TRUE;
        } else {
            while ('TRCE' != section) {
                length = log.readDword();
                section = readSectionTag();
                switch (section) {
                case 'MDLS':
                    /* Read modules header */
                    parseModulesInfo(log);
                    break;
                case 'RNGS':
                    /* Read ranges header */
                    parseRangesInfo(log);
                    break;
                case 'TRCE':
                    /* Trace data start */
                    break;
                default:
                    /* Parse error unknown section */
                    assert(FALSE);
                    break;
                }
            }
            /* Read log */
            functionResult = parseTrace(log);
        }
    }
    return functionResult;
}

BOOL LogParser::parseModulesInfo(FileReader &log)
{
    BOOL functionResult = FALSE;

    DWORD numModules = log.readDword();
    for (DWORD i = 0; i < numModules; ++i) {
        DWORD tagSize = log.readDword();
        DWORD tagName = readSectionTag();
        assert('MODL' == tagName);
        char moduleName[0x100];
        DWORD moduleNameLen = log.readDword();
        log.readData((BYTE *)moduleName, moduleNameLen);
        log.aligenReadTo4();
        ADDRESS moduleAddress   = log.readPointer();
        DWORD moduleSize        = log.readDword();
        Address address(0, moduleAddress);
        BYTE * moduleData = new BYTE[moduleSize];
        log.readData(moduleData, moduleSize);
        memory->setStaticDataChunk(address, moduleSize, moduleData);
        delete [] moduleData;
    }

    return functionResult;
}

BOOL LogParser::parseRangesInfo(FileReader &log)
{
    BOOL functionResult = FALSE;

    DWORD numRanges = log.readDword();
    for (DWORD i = 0; i < numRanges; ++i) {
        DWORD tagSize = log.readDword();
        DWORD tagName = readSectionTag();
        if ('RNGE' == tagName) {
            ADDRESS rangeStart = log.readPointer();
            ADDRESS rangeEnd   = log.readPointer();
            assert(rangeStart < rangeEnd);
        } else {
            /* Invalid tag */
            assert(FALSE);
        }
    }

    return functionResult;
}

BOOL LogParser::parseTrace(FileReader &log)
{
    register BYTE changeType = 0;
    BYTE        regId = 0;
    RegValue    regValue;
    Address     address(0, 0);
    BYTE        byteValue;
    WORD        wordValue;
    DWORD       dwordValue;
    QWORD       qwordValue;
    Cycle       lastCycle = *_lastCyclePtr;
    assert(0 == lastCycle);

    while( (FALSE == log.isEof()) && (lastCycle <= _maxCycle) ) {
        changeType = log.readByte();
        switch( changeType ) {
            case EIP_CHANGE_TYPE:
            {
                MACHINE_LONG newEip = log.readPointer();
                eipLog->append(newEip);
                (*_lastCyclePtr)++;
                lastCycle = *_lastCyclePtr;
            }
            break;
            /* I treat the thread id as another register */
            case THREAD_CHANGE:
            {
                regValue.cycle = lastCycle;
                regValue.value = log.readDword();
                reg[THREAD_ID]->append( regValue );
            }
            break;
#ifdef X86
            case REG_ID_EDI:
            case REG_ID_ESI:
            case REG_ID_EBP:
            case REG_ID_EBX:
            case REG_ID_EDX:
            case REG_ID_ECX:
            case REG_ID_EAX:
            case REG_ID_ECS:
            case REG_ID_EFLAGS:
            case REG_ID_ESP:
#elif AMD64
            case REG_ID_RDI:
            case REG_ID_RSI:
            case REG_ID_RBP:
            case REG_ID_RBX:
            case REG_ID_RDX:
            case REG_ID_RCX:
            case REG_ID_RAX:
            case REG_ID_R8:
            case REG_ID_R9:
            case REG_ID_R10:
            case REG_ID_R11:
            case REG_ID_R12:
            case REG_ID_R13:
            case REG_ID_R14:
            case REG_ID_R15:
            case REG_ID_RCS:
            case REG_ID_RFLAGS:
            case REG_ID_RSP:
            case REG_ID_RSS:
#endif
            {
                regId = changeType;
                regValue.cycle = lastCycle;
                regValue.value = log.readDword();
                reg[regId]->append( regValue );
            } /* Regs cases */
            break;

            case BYTEPTR_ACCESS:
            {
                address.cycle = lastCycle;
                address.addr = log.readDword();
                byteValue = log.readByte();
                memory->setByte(address, byteValue);
            } /* BYTEPTR_ACCESS */
            break;

            case WORDPTR_ACCESS:
            {
                address.cycle = lastCycle;
                address.addr = log.readDword();
                wordValue = log.readWord();
                memory->setWord(address, wordValue);
            } /* WORDPTR_ACCESS */
            break;

            case DWORDPTR_ACCESS:
            {
                address.cycle = lastCycle;
                address.addr = log.readDword();
                dwordValue = log.readDword();
                memory->setDword(address, dwordValue);
            } /* DWORDPTR_ACCESS */
            break;

            case QWORDPTR_ACCESS:
            {
                address.cycle = lastCycle;
                address.addr = log.readDword();
                qwordValue = log.readQword();
                memory->setQword(address, qwordValue);
            } /* DWORDPTR_ACCESS */
            break;

            default:
                printf("Parsing failed at position %d\n", log.tell());
                return FALSE;
        } /* switch */
    } /* while */

    log.closeFile();
    return TRUE;
}

DWORD LogParser::findRegEffectiveIndex(DWORD regId, DWORD cycle)
{
    return reg[regId]->findEffectiveIndex(cycle);
}

DWORD LogParser::findEffectiveCycle(DWORD regId, DWORD queryCycle)
{
    if (REG_ID_EIP == regId) {
        DWORD maxEipCycle = eipLog->getNumItems();
        if (queryCycle > maxEipCycle) {
            return INVALID_CYCLE;
        }
        return queryCycle;
    } else if (NUMBER_OF_REGS > regId) {
        DWORD index = findRegEffectiveIndex(regId, queryCycle);
        DWORD regCycle = reg[regId]->getItem(index).cycle;
        return regCycle;
    } else {
        // Invalid reg id
    }
    return INVALID_CYCLE;
};

MACHINE_LONG LogParser::getRegValueById(DWORD regId, DWORD cycle)
{
    if (REG_ID_EIP == regId) {
        DWORD maxEipCycle = eipLog->getNumItems();
        if (cycle > maxEipCycle) {
            return UNKNOWN_MACHINE_LONG;
        }
        return eipLog->getItem(cycle);
    } else if (NUMBER_OF_REGS > regId) {
        DWORD index = findRegEffectiveIndex(regId, cycle);
        if (index >= reg[regId]->getNumItems()) {
            return UNKNOWN_MACHINE_LONG;
        }
        return reg[regId]->getItem(index).value;
    } else {
        // Invalid reg id
    }
    return UNKNOWN_MACHINE_LONG;
}

DWORD LogParser::findCycleWithEipValue(DWORD targetEip, DWORD startCycle, DWORD endCycle)
{
    int step;
    if (startCycle > endCycle) {
        step = -1;
    } else {
        step = 1;
    }
    for (DWORD cycleIter = startCycle; cycleIter != endCycle; cycleIter += step) {
        if (targetEip == eipLog->getItem(cycleIter)) {
            return cycleIter;
        }
    }
    return 0;
}

DWORD LogParser::findCycleWithRegValue(DWORD regId, DWORD targetValue, DWORD startCycle, DWORD endCycle)
{
    int step;
    DWORD startIndex = reg[regId]->findEffectiveCycle(startCycle);
    DWORD endIndex   = reg[regId]->findEffectiveCycle(endCycle);
    if (startIndex > endIndex) {
        step = -1;
    } else {
        step = 1;
    }
    for (DWORD index = startIndex; index != endIndex; index += step) {
        if (targetValue == reg[regId]->getItem(index).value) {
            return index;
        }
    }
    return 0;
}

RegLogIterBase * LogParser::getRegLogIter(DWORD regId, DWORD cycle)
{
    if (REG_ID_EIP == regId) {
        return (RegLogIterBase *)(new EipLogIter(eipLog, cycle));
    } else if (NUMBER_OF_REGS > regId) {
        return (RegLogIterBase *)(new RegLogIter(reg[regId], cycle));
    } else {
        // Invalid reg id
    }
    return NULL;
}

FindCycleWithEipValue::FindCycleWithEipValue(LogParser * logParser) :
                        logParser(logParser),
                        eipLog(logParser->getEipLog()),
                        currentCycle(0),
                        bottomCycle(0),
                        topCycle(0),
                        target(0),
                        isDone(FALSE)
{
}

void FindCycleWithEipValue::restartSearch()
{
    isDone = FALSE;
    DWORD numValues = eipLog->getNumItems();
    if (bottomCycle >= numValues) {
        isDone = TRUE;
    }
    if (topCycle >= numValues) {
        topCycle = numValues;
    }
    currentCycle = bottomCycle;
    findNext();
}

void FindCycleWithEipValue::newSearch( ADDRESS searchTarget, DWORD startCycle, DWORD endCycle )
{
    bottomCycle = startCycle;
    topCycle = endCycle;
    target = searchTarget;
    restartSearch();
}

void FindCycleWithEipValue::newReverseSearch(ADDRESS searchTarget, DWORD startCycle, DWORD endCycle)
{
    bottomCycle = endCycle;
    topCycle = startCycle;
    target = searchTarget;
    isDone = FALSE;
    DWORD numValues = eipLog->getNumItems();
    if (bottomCycle >= numValues) {
        isDone = TRUE;
    }
    if (topCycle >= numValues) {
        topCycle = numValues;
    }
    currentCycle = topCycle;
    findPrev();

}

void FindCycleWithEipValue::findNext()
{
    while (currentCycle < topCycle)
    {
        MACHINE_LONG currentValue = eipLog->getItem(currentCycle);
        if (target == currentValue)
        {
            return;
        }
        currentCycle++;
    }
    currentCycle = INVALID_CYCLE;
}

void FindCycleWithEipValue::findPrev()
{
    while (bottomCycle < currentCycle)
    {
        MACHINE_LONG currentValue = eipLog->getItem(currentCycle);
        if (target == currentValue)
        {
            return;
        }
        currentCycle--;
    }
    currentCycle = INVALID_CYCLE;
}

void FindCycleWithEipValue::next()
{
    if (INVALID_CYCLE != currentCycle) {
        currentCycle++;
        findNext();
    }
}

void FindCycleWithEipValue::prev()
{
    if (INVALID_CYCLE != currentCycle) {
        currentCycle--;
        findPrev();
    }
}

Cycle FindCycleWithEipValue::current()
{
    return currentCycle;
}

// For Debug
void LogParser::DumpMemoryUsage()
{
    // TODO
}
