#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unwind.h>
extern "C" {
#define QC_EXCEPTION_CLASS 0x5143455843455054ULL
struct __qc_exception {
    _Unwind_Exception unwind;
    char* type;
    void* value;
};
static void __qc_cleanup(_Unwind_Reason_Code reason, _Unwind_Exception* exception) {
    (void)reason;
    __qc_exception* e = (__qc_exception*)exception;
    free(e->type);
    free(e);
}
__qc_exception* __qc_create_exception(char* type, void* value) {
    __qc_exception* e = (__qc_exception*)malloc(sizeof(__qc_exception));
    e->unwind.exception_class = QC_EXCEPTION_CLASS;
    e->unwind.exception_cleanup = __qc_cleanup;
    e->type = strdup(type);
    e->value = value;
    return e;
}
void __qc_throw(__qc_exception* e) {
    _Unwind_Reason_Code result = _Unwind_RaiseException(&e->unwind);
    fprintf(stderr, "Uncaught exception: %s\n", e->type);
    abort();
}
#define EXCEPTION_REG 0
#define SELECTOR_REG 1
#define DW_EH_PE_absptr 0x00
#define DW_EH_PE_omit 0xff
#define DW_EH_PE_uleb128 0x01
#define DW_EH_PE_udata2 0x02
#define DW_EH_PE_udata4 0x03
#define DW_EH_PE_udata8 0x04
#define DW_EH_PE_sleb128 0x09
#define DW_EH_PE_sdata2 0x0A
#define DW_EH_PE_sdata4 0x0B
#define DW_EH_PE_sdata8 0x0C
#define DW_EH_PE_pcrel 0x10
#define DW_EH_PE_textrel 0x20
#define DW_EH_PE_datarel 0x30
#define DW_EH_PE_funcrel 0x40
#define DW_EH_PE_indirect 0x80
static uintptr_t __qc_read_uleb128(uint8_t** p) {
    uintptr_t result = 0;
    int shift = 0;
    while (1) {
        uint8_t b = **p;
        (*p)++;
        result |= ((uintptr_t)(b & 0x7f)) << shift;
        if (!(b & 0x80)) return result;
        shift += 7;
    }
}
static intptr_t __qc_read_sleb128(uint8_t** p) {
    intptr_t result = 0;
    int shift = 0;
    uint8_t b;
    do {
        b = **p;
        (*p)++;
        result |= ((intptr_t)(b & 0x7f)) << shift;
        shift += 7;
    } while (b & 0x80);
    if ((shift < sizeof(result) * 8) && (b & 0x40)) { result |= -((intptr_t)1 << shift); }
    return result;
}
#define DW_EH_PE_aligned 0x50

static uintptr_t __qc_read_encoded(uint8_t** p, uint8_t encoding, uintptr_t textBase, uintptr_t dataBase, uintptr_t funcBase) {
    if (encoding == DW_EH_PE_omit) return 0;
    uint8_t application = encoding & 0x70;
    if (application == DW_EH_PE_aligned) {
        uintptr_t address = (uintptr_t)*p;
        uintptr_t alignment = sizeof(uintptr_t);
        address = (address + alignment - 1) & ~(alignment - 1);
        *p = (uint8_t*)address;
        encoding = (encoding & 0x8f) | DW_EH_PE_absptr;
        application = DW_EH_PE_absptr;
    }
    uint8_t* encodedStart = *p;
    intptr_t result = 0;
    switch (encoding & 0x0f) {
    case DW_EH_PE_absptr:
        memcpy(&result, *p, sizeof(uintptr_t));
        *p += sizeof(uintptr_t);
        break;
    case DW_EH_PE_uleb128: result = (intptr_t)__qc_read_uleb128(p); break;
    case DW_EH_PE_udata2: {
        uint16_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = value;
        break;
    }
    case DW_EH_PE_udata4: {
        uint32_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = value;
        break;
    }
    case DW_EH_PE_udata8: {
        uint64_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = (intptr_t)value;
        break;
    }
    case DW_EH_PE_sleb128: result = __qc_read_sleb128(p); break;
    case DW_EH_PE_sdata2: {
        int16_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = value;
        break;
    }
    case DW_EH_PE_sdata4: {
        int32_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = value;
        break;
    }
    case DW_EH_PE_sdata8: {
        int64_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = (intptr_t)value;
        break;
    }
    default: abort();
    }
    switch (application) {
    case DW_EH_PE_absptr: break;
    case DW_EH_PE_pcrel: result += (uintptr_t)encodedStart; break;
    case DW_EH_PE_textrel: result += textBase; break;
    case DW_EH_PE_datarel: result += dataBase; break;
    case DW_EH_PE_funcrel: result += funcBase; break;
    default: abort();
    }
    if (encoding & DW_EH_PE_indirect) {
        uintptr_t indirectValue;
        memcpy(&indirectValue, (void*)(uintptr_t)result, sizeof(indirectValue));
        result = (intptr_t)indirectValue;
    }
    return (uintptr_t)result;
}
_Unwind_Reason_Code __qc_personality(int version, _Unwind_Action actions, uint64_t exceptionClass, _Unwind_Exception* exceptionObject,
                                     _Unwind_Context* context) {
    if (version != 1) return _URC_FATAL_PHASE1_ERROR;
    if (exceptionClass != QC_EXCEPTION_CLASS) return _URC_CONTINUE_UNWIND;

    auto* exception = reinterpret_cast<__qc_exception*>(exceptionObject);
    auto* lsda = reinterpret_cast<uint8_t*>(_Unwind_GetLanguageSpecificData(context));
    if (!lsda) return _URC_CONTINUE_UNWIND;

    uintptr_t funcStart = _Unwind_GetRegionStart(context);
    uintptr_t ip = _Unwind_GetIP(context);
    if (ip != 0) --ip;

    uint8_t* p = lsda;

    uint8_t lpStartEncoding = *p++;
    uintptr_t lpStart = funcStart;
    if (lpStartEncoding != DW_EH_PE_omit) { lpStart = __qc_read_encoded(&p, lpStartEncoding, 0, 0, funcStart); }

    uint8_t typeEncoding = *p++;
    uint8_t* typeTable = nullptr;
    if (typeEncoding != DW_EH_PE_omit) {
        uintptr_t offset = __qc_read_uleb128(&p);
        typeTable = p + offset;
    }

    uint8_t callSiteEncoding = *p++;
    uintptr_t callSiteSize = __qc_read_uleb128(&p);
    uint8_t* callSiteTable = p;
    uint8_t* actionTable = callSiteTable + callSiteSize;

    uintptr_t landingPad = 0;
    uintptr_t actionIndex = 0;
    uintptr_t pc = ip - funcStart;

    for (uint8_t* cs = callSiteTable; cs < actionTable;) {
        uintptr_t start = __qc_read_encoded(&cs, callSiteEncoding, 0, 0, funcStart);
        uintptr_t length = __qc_read_encoded(&cs, callSiteEncoding, 0, 0, funcStart);
        uintptr_t pad = __qc_read_encoded(&cs, callSiteEncoding, 0, 0, funcStart);
        uintptr_t action = __qc_read_uleb128(&cs);

        if (pc >= start && pc < start + length) {
            landingPad = lpStart + pad;
            actionIndex = action;
            break;
        }
    }

    if (landingPad == 0 || actionIndex == 0) { return _URC_CONTINUE_UNWIND; }

    uint8_t* action = actionTable + actionIndex - 1;
    bool matched = false;
    bool hasCleanup = false;
    int selector = 0;
    int selectorIndex = 0;
    bool hasCatchAll = false;
    while (true) {
        int64_t typeIndex = __qc_read_sleb128(&action);
        uint8_t* nextField = action;
        int64_t next = __qc_read_sleb128(&action);
        if (typeIndex == 0) {
            hasCleanup = true;
        } else if (typeIndex > 0 && typeTable != nullptr) {
            size_t typeSize;
            switch (typeEncoding & 0x0f) {
            case DW_EH_PE_absptr: typeSize = sizeof(uintptr_t); break;
            case DW_EH_PE_udata2:
            case DW_EH_PE_sdata2: typeSize = 2; break;
            case DW_EH_PE_udata4:
            case DW_EH_PE_sdata4: typeSize = 4; break;
            case DW_EH_PE_udata8:
            case DW_EH_PE_sdata8: typeSize = 8; break;
            default: abort();
            }
            uint8_t* entry = typeTable - typeIndex * typeSize;
            uintptr_t raw = 0;
            memcpy(&raw, entry, typeSize);
            if (raw == 0) {
                hasCatchAll = true;
            } else {
                ++selectorIndex;
                uintptr_t typeAddr = __qc_read_encoded(&entry, typeEncoding, 0, 0, reinterpret_cast<uintptr_t>(entry));
                const char* catchType = reinterpret_cast<const char*>(typeAddr);
                if (catchType && strcmp(exception->type, catchType) == 0) {
                    matched = true;
                    selector = selectorIndex;
                    break;
                }
            }
        }
        if (next == 0) break;
        action = nextField + next;
    }
    if (!matched && hasCatchAll) {
        matched = true;
        selector = 0;
    }
    if (actions & _UA_SEARCH_PHASE) { return matched ? _URC_HANDLER_FOUND : _URC_CONTINUE_UNWIND; }

    if (actions & _UA_CLEANUP_PHASE) {
        if (matched && (actions & _UA_HANDLER_FRAME)) {
            _Unwind_SetGR(context, 0, reinterpret_cast<uintptr_t>(exceptionObject));
            _Unwind_SetGR(context, 1, static_cast<uintptr_t>(selector));
            _Unwind_SetIP(context, landingPad);
            return _URC_INSTALL_CONTEXT;
        }

        if (hasCleanup) {
            _Unwind_SetGR(context, 0, reinterpret_cast<uintptr_t>(exceptionObject));
            _Unwind_SetGR(context, 1, 0);
            _Unwind_SetIP(context, landingPad);
            return _URC_INSTALL_CONTEXT;
        }
    }

    return _URC_CONTINUE_UNWIND;
}
}
