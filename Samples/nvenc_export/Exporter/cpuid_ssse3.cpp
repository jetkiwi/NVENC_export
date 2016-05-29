// cpuid.cpp 
// processor: x86, x64
// Use the __cpuid intrinsic to get information about a CPU

#include <stdio.h>

#include <string.h>
#include <intrin.h>
#include "cpuid_ssse3.h"


const char* szCpuidFeatures[] =
{
    "x87 FPU On Chip",
    "Virtual-8086 Mode Enhancement",
    "Debugging Extensions",
    "Page Size Extensions",
    "Time Stamp Counter",
    "RDMSR and WRMSR Support",
    "Physical Address Extensions",
    "Machine Check Exception",
    "CMPXCHG8B Instruction",
    "APIC On Chip",
    "Unknown1",
    "SYSENTER and SYSEXIT",
    "Memory Type Range Registers",
    "PTE Global Bit",
    "Machine Check Architecture",
    "Conditional Move/Compare Instruction",
    "Page Attribute Table",
    "36-bit Page Size Extension",
    "Processor Serial Number",
    "CFLUSH Extension",
    "Unknown2",
    "Debug Store",
    "Thermal Monitor and Clock Ctrl",
    "MMX Technology",
    "FXSAVE/FXRSTOR",
    "SSE Extensions",
    "SSE2 Extensions",
    "Self Snoop",
    "Multithreading Technology",
    "Thermal Monitor",
    "Unknown4",
    "Pending Break Enable"
};

int
get_cpuinfo_has_ssse3(s_cpuid_info * const s)
{
	memset( reinterpret_cast<void *>(s), 0, sizeof(*s) ); // clear it

    s->CPUInfo[0] = -1;
	s->CPUInfo[1] = 0;
	s->CPUInfo[2] = 0;
	s->CPUInfo[3] = 0;
    s->nSteppingID = 0;
    s->nModel = 0;
    s->nFamily = 0;
    s->nProcessorType = 0;
    s->nExtendedmodel = 0;
    s->nExtendedfamily = 0;
    s->nBrandIndex = 0;
    s->nCLFLUSHcachelinesize = 0;
    s->nLogicalProcessors = 0;
    s->nAPICPhysicalID = 0;
    s->nFeatureInfo = 0;
    s->nCacheLineSize = 0;
    s->nL2Associativity = 0;
    s->nCacheSizeK = 0;
    s->nPhysicalAddress = 0;
    s->nVirtualAddress = 0;

    s->nCores = 0;
    s->nCacheType = 0;
    s->nCacheLevel = 0;
    s->nMaxThread = 0;
    s->nSysLineSize = 0;
    s->nPhysicalLinePartitions = 0;
    s->nWaysAssociativity = 0;
    s->nNumberSets = 0;

    unsigned    nIds, nExIds, i;
    int nRet = 0;

    s->bSSE3Instructions = false;
    s->bMONITOR_MWAIT = false;
    s->bCPLQualifiedDebugStore = false;
    s->bVirtualMachineExtensions = false;
    s->bEnhancedIntelSpeedStepTechnology = false;
    s->bThermalMonitor2 = false;
    s->bSupplementalSSE3 = false;
    s->bL1ContextID = false;
    s->bCMPXCHG16B = false;
    s->bxTPRUpdateControl = false;
    s->bPerfDebugCapabilityMSR = false;
    s->bSSE41Extensions = false;
    s->bSSE42Extensions = false;
    s->bPOPCNT = false;

    s->bMultithreading = false;

    s->bLAHF_SAHFAvailable = false;
    s->bCmpLegacy = false;
    s->bSVM = false;
    s->bExtApicSpace = false;
    s->bAltMovCr8 = false;
    s->bLZCNT = false;
    s->bSSE4A = false;
    s->bMisalignedSSE = false;
    s->bPREFETCH = false;
    s->bSKINITandDEV = false;
    s->bSYSCALL_SYSRETAvailable = false;
    s->bExecuteDisableBitAvailable = false;
    s->bMMXExtensions = false;
    s->bFFXSR = false;
    s->b1GBSupport = false;
    s->bRDTSCP = false;
    s->b64Available = false;
    s->b3DNowExt = false;
    s->b3DNow = false;
    s->bNestedPaging = false;
    s->bLBRVisualization = false;
    s->bFP128 = false;
    s->bMOVOptimization = false;

    s->bSelfInit = false;
    s->bFullyAssociative = false;

    // __cpuid with an InfoType argument of 0 returns the number of
    // valid Ids in CPUInfo[0] and the CPU identification string in
    // the other three array elements. The CPU identification string is
    // not in linear order. The code below arranges the information 
    // in a human readable form.
    __cpuid(s->CPUInfo, 0);
    nIds = s->CPUInfo[0];
    memset(s->CPUString, 0, sizeof(s->CPUString));
    *((int*)s->CPUString) = s->CPUInfo[1];
    *((int*)(s->CPUString+4)) = s->CPUInfo[3];
    *((int*)(s->CPUString+8)) = s->CPUInfo[2];

    // Get the information associated with each valid Id
    for (i=0; i<=nIds; ++i)
    {
        __cpuid(s->CPUInfo, i);
        printf_s("\nFor InfoType %d\n", i); 
        printf_s("s->CPUInfo[0] = 0x%x\n", s->CPUInfo[0]);
        printf_s("s->CPUInfo[1] = 0x%x\n", s->CPUInfo[1]);
        printf_s("s->CPUInfo[2] = 0x%x\n", s->CPUInfo[2]);
        printf_s("s->CPUInfo[3] = 0x%x\n", s->CPUInfo[3]);

        // Interpret CPU feature information.
        if  (i == 1)
        {
            s->nSteppingID = s->CPUInfo[0] & 0xf;
            s->nModel = (s->CPUInfo[0] >> 4) & 0xf;
            s->nFamily = (s->CPUInfo[0] >> 8) & 0xf;
            s->nProcessorType = (s->CPUInfo[0] >> 12) & 0x3;
            s->nExtendedmodel = (s->CPUInfo[0] >> 16) & 0xf;
            s->nExtendedfamily = (s->CPUInfo[0] >> 20) & 0xff;
            s->nBrandIndex = s->CPUInfo[1] & 0xff;
            s->nCLFLUSHcachelinesize = ((s->CPUInfo[1] >> 8) & 0xff) * 8;
            s->nLogicalProcessors = ((s->CPUInfo[1] >> 16) & 0xff);
            s->nAPICPhysicalID = (s->CPUInfo[1] >> 24) & 0xff;
            s->bSSE3Instructions = (s->CPUInfo[2] & 0x1) || false;
            s->bMONITOR_MWAIT = (s->CPUInfo[2] & 0x8) || false;
            s->bCPLQualifiedDebugStore = (s->CPUInfo[2] & 0x10) || false;
            s->bVirtualMachineExtensions = (s->CPUInfo[2] & 0x20) || false;
            s->bEnhancedIntelSpeedStepTechnology = (s->CPUInfo[2] & 0x80) || false;
            s->bThermalMonitor2 = (s->CPUInfo[2] & 0x100) || false;
            s->bSupplementalSSE3 = (s->CPUInfo[2] & 0x200) || false;// Supplemental Streaming SIMD Extensions 3 (SSSE3)
            s->bL1ContextID = (s->CPUInfo[2] & 0x300) || false;
            s->bCMPXCHG16B= (s->CPUInfo[2] & 0x2000) || false;
            s->bxTPRUpdateControl = (s->CPUInfo[2] & 0x4000) || false;
            s->bPerfDebugCapabilityMSR = (s->CPUInfo[2] & 0x8000) || false;
            s->bSSE41Extensions = (s->CPUInfo[2] & 0x80000) || false;
            s->bSSE42Extensions = (s->CPUInfo[2] & 0x100000) || false;
            s->bPOPCNT= (s->CPUInfo[2] & 0x800000) || false;
            s->nFeatureInfo = s->CPUInfo[3];
            s->bMultithreading = (s->nFeatureInfo & (1 << 28)) || false;
        }
    }

    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    __cpuid(s->CPUInfo, 0x80000000);
    nExIds = s->CPUInfo[0];
    memset(s->CPUBrandString, 0, sizeof(s->CPUBrandString));

    // Get the information associated with each extended ID.
    for (i=0x80000000; i<=nExIds; ++i)
    {
        __cpuid(s->CPUInfo, i);
        printf_s("\nFor InfoType %x\n", i); 
        printf_s("s->CPUInfo[0] = 0x%x\n", s->CPUInfo[0]);
        printf_s("s->CPUInfo[1] = 0x%x\n", s->CPUInfo[1]);
        printf_s("s->CPUInfo[2] = 0x%x\n", s->CPUInfo[2]);
        printf_s("s->CPUInfo[3] = 0x%x\n", s->CPUInfo[3]);

        if  (i == 0x80000001)
        {
            s->bLAHF_SAHFAvailable = (s->CPUInfo[2] & 0x1) || false;
            s->bCmpLegacy = (s->CPUInfo[2] & 0x2) || false;
            s->bSVM = (s->CPUInfo[2] & 0x4) || false;
            s->bExtApicSpace = (s->CPUInfo[2] & 0x8) || false;
            s->bAltMovCr8 = (s->CPUInfo[2] & 0x10) || false;
            s->bLZCNT = (s->CPUInfo[2] & 0x20) || false;
            s->bSSE4A = (s->CPUInfo[2] & 0x40) || false;
            s->bMisalignedSSE = (s->CPUInfo[2] & 0x80) || false;
            s->bPREFETCH = (s->CPUInfo[2] & 0x100) || false;
            s->bSKINITandDEV = (s->CPUInfo[2] & 0x1000) || false;
            s->bSYSCALL_SYSRETAvailable = (s->CPUInfo[3] & 0x800) || false;
            s->bExecuteDisableBitAvailable = (s->CPUInfo[3] & 0x10000) || false;
            s->bMMXExtensions = (s->CPUInfo[3] & 0x40000) || false;
            s->bFFXSR = (s->CPUInfo[3] & 0x200000) || false;
            s->b1GBSupport = (s->CPUInfo[3] & 0x400000) || false;
            s->bRDTSCP = (s->CPUInfo[3] & 0x8000000) || false;
            s->b64Available = (s->CPUInfo[3] & 0x20000000) || false;
            s->b3DNowExt = (s->CPUInfo[3] & 0x40000000) || false;
            s->b3DNow = (s->CPUInfo[3] & 0x80000000) || false;
        }

        // Interpret CPU brand string and cache information.
        if  (i == 0x80000002)
            memcpy(s->CPUBrandString, s->CPUInfo, sizeof(s->CPUInfo));
        else if  (i == 0x80000003)
            memcpy(s->CPUBrandString + 16, s->CPUInfo, sizeof(s->CPUInfo));
        else if  (i == 0x80000004)
            memcpy(s->CPUBrandString + 32, s->CPUInfo, sizeof(s->CPUInfo));
        else if  (i == 0x80000006)
        {
            s->nCacheLineSize = s->CPUInfo[2] & 0xff;
            s->nL2Associativity = (s->CPUInfo[2] >> 12) & 0xf;
            s->nCacheSizeK = (s->CPUInfo[2] >> 16) & 0xffff;
        }
        else if  (i == 0x80000008)
        {
           s->nPhysicalAddress = s->CPUInfo[0] & 0xff;
           s->nVirtualAddress = (s->CPUInfo[0] >> 8) & 0xff;
        }
        else if  (i == 0x8000000A)
        {
            s->bNestedPaging = (s->CPUInfo[3] & 0x1) || false;
            s->bLBRVisualization = (s->CPUInfo[3] & 0x2) || false;
        }
        else if  (i == 0x8000001A)
        {
            s->bFP128 = (s->CPUInfo[0] & 0x1) || false;
            s->bMOVOptimization = (s->CPUInfo[0] & 0x2) || false;
        }
    }

    // Display all the information in user-friendly format.

    printf_s("\n\nCPU String: %s\n", s->CPUString);

    if  (nIds >= 1)
    {
        if  (s->nSteppingID)
            printf_s("Stepping ID = %d\n", s->nSteppingID);
        if  (s->nModel)
            printf_s("Model = %d\n", s->nModel);
        if  (s->nFamily)
            printf_s("Family = %d\n", s->nFamily);
        if  (s->nProcessorType)
            printf_s("Processor Type = %d\n", s->nProcessorType);
        if  (s->nExtendedmodel)
            printf_s("Extended model = %d\n", s->nExtendedmodel);
        if  (s->nExtendedfamily)
            printf_s("Extended family = %d\n", s->nExtendedfamily);
        if  (s->nBrandIndex)
            printf_s("Brand Index = %d\n", s->nBrandIndex);
        if  (s->nCLFLUSHcachelinesize)
            printf_s("CLFLUSH cache line size = %d\n",
                     s->nCLFLUSHcachelinesize);
        if (s->bMultithreading && (s->nLogicalProcessors > 0))
           printf_s("Logical Processor Count = %d\n", s->nLogicalProcessors);
        if  (s->nAPICPhysicalID)
            printf_s("APIC Physical ID = %d\n", s->nAPICPhysicalID);

        if  (s->nFeatureInfo || s->bSSE3Instructions ||
             s->bMONITOR_MWAIT || s->bCPLQualifiedDebugStore ||
             s->bVirtualMachineExtensions || s->bEnhancedIntelSpeedStepTechnology ||
             s->bThermalMonitor2 || s->bSupplementalSSE3 || s->bL1ContextID || 
             s->bCMPXCHG16B || s->bxTPRUpdateControl || s->bPerfDebugCapabilityMSR || 
             s->bSSE41Extensions || s->bSSE42Extensions || s->bPOPCNT || 
             s->bLAHF_SAHFAvailable || s->bCmpLegacy || s->bSVM ||
             s->bExtApicSpace || s->bAltMovCr8 ||
             s->bLZCNT || s->bSSE4A || s->bMisalignedSSE ||
             s->bPREFETCH || s->bSKINITandDEV || s->bSYSCALL_SYSRETAvailable || 
             s->bExecuteDisableBitAvailable || s->bMMXExtensions || s->bFFXSR || s->b1GBSupport ||
             s->bRDTSCP || s->b64Available || s->b3DNowExt || s->b3DNow || s->bNestedPaging || 
             s->bLBRVisualization || s->bFP128 || s->bMOVOptimization )
        {
            printf_s("\nThe following features are supported:\n");

            if  (s->bSSE3Instructions)
                printf_s("\tSSE3\n");
            if  (s->bMONITOR_MWAIT)
                printf_s("\tMONITOR/MWAIT\n");
            if  (s->bCPLQualifiedDebugStore)
                printf_s("\tCPL Qualified Debug Store\n");
            if  (s->bVirtualMachineExtensions)
                printf_s("\tVirtual Machine Extensions\n");
            if  (s->bEnhancedIntelSpeedStepTechnology)
                printf_s("\tEnhanced Intel SpeedStep Technology\n");
            if  (s->bThermalMonitor2)
                printf_s("\tThermal Monitor 2\n");
            if  (s->bSupplementalSSE3)
                printf_s("\tSupplemental Streaming SIMD Extensions 3\n");
            if  (s->bL1ContextID)
                printf_s("\tL1 Context ID\n");
            if  (s->bCMPXCHG16B)
                printf_s("\tCMPXCHG16B Instruction\n");
            if  (s->bxTPRUpdateControl)
                printf_s("\txTPR Update Control\n");
            if  (s->bPerfDebugCapabilityMSR)
                printf_s("\tPerf\\Debug Capability MSR\n");
            if  (s->bSSE41Extensions)
                printf_s("\tSSE4.1 Extensions\n");
            if  (s->bSSE42Extensions)
                printf_s("\tSSE4.2 Extensions\n");
            if  (s->bPOPCNT)
                printf_s("\tPPOPCNT Instruction\n");

            i = 0;
            nIds = 1;
            while (i < (sizeof(szCpuidFeatures)/sizeof(const char*)))
            {
                if  (s->nFeatureInfo & nIds)
                {
                    printf_s("\t");
                    printf_s(szCpuidFeatures[i]);
                    printf_s("\n");
                }

                nIds <<= 1;
                ++i;
            }
            if (s->bLAHF_SAHFAvailable)
                printf_s("\tLAHF/SAHF in 64-bit mode\n");
            if (s->bCmpLegacy)
                printf_s("\tCore multi-processing legacy mode\n");
            if (s->bSVM)
                printf_s("\tSecure Virtual Machine\n");
            if (s->bExtApicSpace)
                printf_s("\tExtended APIC Register Space\n");
            if (s->bAltMovCr8)
                printf_s("\tAltMovCr8\n");
            if (s->bLZCNT)
                printf_s("\tLZCNT instruction\n");
            if (s->bSSE4A)
                printf_s("\tSSE4A (EXTRQ, INSERTQ, MOVNTSD, MOVNTSS)\n");
            if (s->bMisalignedSSE)
                printf_s("\tMisaligned SSE mode\n");
            if (s->bPREFETCH)
                printf_s("\tPREFETCH and PREFETCHW Instructions\n");
            if (s->bSKINITandDEV)
                printf_s("\tSKINIT and DEV support\n");
            if (s->bSYSCALL_SYSRETAvailable)
                printf_s("\tSYSCALL/SYSRET in 64-bit mode\n");
            if (s->bExecuteDisableBitAvailable)
                printf_s("\tExecute Disable Bit\n");
            if (s->bMMXExtensions)
                printf_s("\tExtensions to MMX Instructions\n");
            if (s->bFFXSR)
                printf_s("\tFFXSR\n");
            if (s->b1GBSupport)
                printf_s("\t1GB page support\n");
            if (s->bRDTSCP)
                printf_s("\tRDTSCP instruction\n");
            if (s->b64Available)
                printf_s("\t64 bit Technology\n");
            if (s->b3DNowExt)
                printf_s("\t3Dnow Ext\n");
            if (s->b3DNow)
                printf_s("\t3Dnow! instructions\n");
            if (s->bNestedPaging)
                printf_s("\tNested Paging\n");
            if (s->bLBRVisualization)
                printf_s("\tLBR Visualization\n");
            if (s->bFP128)
                printf_s("\tFP128 optimization\n");
            if (s->bMOVOptimization)
                printf_s("\tMOVU Optimization\n");
        }
    }

    if  (nExIds >= 0x80000004)
        printf_s("\nCPU Brand String: %s\n", s->CPUBrandString);

    if  (nExIds >= 0x80000006)
    {
        printf_s("Cache Line Size = %d\n", s->nCacheLineSize);
        printf_s("L2 Associativity = %d\n", s->nL2Associativity);
        printf_s("Cache Size = %dK\n", s->nCacheSizeK);
    }


    for (i=0;;i++)
    {
        __cpuidex(s->CPUInfo, 0x4, i);
        if(!(s->CPUInfo[0] & 0xf0)) break;

        if(i == 0)
        {
            s->nCores = s->CPUInfo[0] >> 26;
            printf_s("\n\nNumber of Cores = %d\n", s->nCores + 1);
        }

        s->nCacheType = (s->CPUInfo[0] & 0x1f);
        s->nCacheLevel = (s->CPUInfo[0] & 0xe0) >> 5;
        s->bSelfInit = (s->CPUInfo[0] & 0x100) >> 8;
        s->bFullyAssociative = (s->CPUInfo[0] & 0x200) >> 9;
        s->nMaxThread = (s->CPUInfo[0] & 0x03ffc000) >> 14;
        s->nSysLineSize = (s->CPUInfo[1] & 0x0fff);
        s->nPhysicalLinePartitions = (s->CPUInfo[1] & 0x03ff000) >> 12;
        s->nWaysAssociativity = (s->CPUInfo[1]) >> 22;
        s->nNumberSets = s->CPUInfo[2];

        printf_s("\n");

        printf_s("ECX Index %d\n", i);
        switch (s->nCacheType)
        {
            case 0:
                printf_s("   Type: Null\n");
                break;
            case 1:
                printf_s("   Type: Data Cache\n");
                break;
            case 2:
                printf_s("   Type: Instruction Cache\n");
                break;
            case 3:
                printf_s("   Type: Unified Cache\n");
                break;
            default:
                 printf_s("   Type: Unknown\n");
        }

        printf_s("   Level = %d\n", s->nCacheLevel + 1); 
        if (s->bSelfInit)
        {
            printf_s("   Self Initializing\n");
        }
        else
        {
            printf_s("   Not Self Initializing\n");
        }
        if (s->bFullyAssociative)
        {
            printf_s("   Is Fully Associatve\n");
        }
        else
        {
            printf_s("   Is Not Fully Associatve\n");
        }
        printf_s("   Max Threads = %d\n", 
            s->nMaxThread+1);
        printf_s("   System Line Size = %d\n", 
            s->nSysLineSize+1);
        printf_s("   Physical Line Partions = %d\n", 
            s->nPhysicalLinePartitions+1);
        printf_s("   Ways of Associativity = %d\n", 
            s->nWaysAssociativity+1);
        printf_s("   Number of Sets = %d\n", 
            s->nNumberSets+1);
    }

    return  nRet;
}