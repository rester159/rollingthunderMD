#ifndef RT_PACKED_CACHE_H
#define RT_PACKED_CACHE_H
#include <genesis.h>
#define RT_PACKED_CACHE_WORDS 4860
typedef struct {u16 pixels[RT_PACKED_CACHE_WORDS];} RT_PackedWorkspace;
void RT_packedWorkspaceBegin(void*,u16);
void RT_packedWorkspaceEnd(void);
u16 *RT_packedPrefetched(const void*,u16);
void RT_packedExtraEnable(void);
void RT_packedBorrow(u8,void*,u16);
void RT_packedStaging(void*,u16);
void RT_packedPreload(const void*,u16);
void RT_decodePackedPose(const void*,u16*,u16);
void RT_packedPrepareBegin(void);
void RT_packedPreparePublish(void);
u16 *RT_packedPrepareAllocate(u16);
void *RT_prepareCanonicalPose(const void*,u16);
#endif
