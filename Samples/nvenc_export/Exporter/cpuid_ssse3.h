

// cpuid.cpp 
// processor: x86, x64
// Use the __cpuid intrinsic to get information about a CPU

typedef struct {
    char CPUString[0x20];
    char CPUBrandString[0x40];
    int CPUInfo[4];
    int nSteppingID;
    int nModel;
    int nFamily;
    int nProcessorType;
    int nExtendedmodel;
    int nExtendedfamily;
    int nBrandIndex;
    int nCLFLUSHcachelinesize;
    int nLogicalProcessors;
    int nAPICPhysicalID;
    int nFeatureInfo;
    int nCacheLineSize;
    int nL2Associativity;
    int nCacheSizeK;
    int nPhysicalAddress;
    int nVirtualAddress;

    int nCores;
    int nCacheType;
    int nCacheLevel;
    int nMaxThread;
    int nSysLineSize;
    int nPhysicalLinePartitions;
    int nWaysAssociativity;
    int nNumberSets;

    bool    bSSE3Instructions;
    bool    bMONITOR_MWAIT;
    bool    bCPLQualifiedDebugStore;
    bool    bVirtualMachineExtensions;
    bool    bEnhancedIntelSpeedStepTechnology;
    bool    bThermalMonitor2;
    bool    bSupplementalSSE3;
    bool    bL1ContextID;
    bool    bCMPXCHG16B;
    bool    bxTPRUpdateControl;
    bool    bPerfDebugCapabilityMSR;
    bool    bSSE41Extensions;
    bool    bSSE42Extensions;
    bool    bPOPCNT;

    bool    bMultithreading;

    bool    bLAHF_SAHFAvailable;
    bool    bCmpLegacy;
    bool    bSVM;
    bool    bExtApicSpace;
    bool    bAltMovCr8;
    bool    bLZCNT;
    bool    bSSE4A;
    bool    bMisalignedSSE;
    bool    bPREFETCH;
    bool    bSKINITandDEV;
    bool    bSYSCALL_SYSRETAvailable;
    bool    bExecuteDisableBitAvailable;
    bool    bMMXExtensions;
    bool    bFFXSR;
    bool    b1GBSupport;
    bool    bRDTSCP;
    bool    b64Available;
    bool    b3DNowExt;
    bool    b3DNow;
    bool    bNestedPaging;
    bool    bLBRVisualization;
    bool    bFP128;
    bool    bMOVOptimization;

    bool    bSelfInit;
    bool    bFullyAssociative;
} s_cpuid_info;

int
get_cpuinfo_has_ssse3(s_cpuid_info * const i);