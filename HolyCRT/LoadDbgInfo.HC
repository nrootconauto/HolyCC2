internal U8i HCC_DEBUG_SYMS;
internal U8i HCC_DEBUG_FUNC_DATAS;
internal U8i HCC_DEBUG_BREAKPOINTS_INFO;
internal U8i HCC_DEBUG_BREAKPOINTS_ARRAY;
internal U8i HCC_DEBUG_GLOBALS_INFO;
class CFileDebugger;
extern CFileDebugger *FileDebuggerCreate(
														U8i *HCC_DEBUG_SYMS,
														U8i *HCC_DEBUG_FUNC_DATAS,
														U8i *HCC_DEBUG_BREAKPOINTS_INFO,
														U8i *HCC_DEBUG_BREAKPOINTS_ARRAY,
														U8i *HCC_DEBUG_GLOBALS_INFO
														);
try {
static CFileDebugger *HCC_LocalFileDebugger=FileDebuggerCreate(
					&HCC_DEBUG_SYMS,
					&HCC_DEBUG_FUNC_DATAS,
					&HCC_DEBUG_BREAKPOINTS_INFO,
					&HCC_DEBUG_BREAKPOINTS_ARRAY,
					&HCC_DEBUG_GLOBALS_INFO);
					} catch {
							"FAILED;\n";
					}
extern U8i *getenv(U8i *var);
extern U8i DebugStep;
if(getenv("HCC_DEBUG")) {
			DebugStep=1;																					
}