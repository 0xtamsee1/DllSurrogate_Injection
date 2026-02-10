#include<windows.h>

// This is the entry point for the DLL.
// It is called when the DLL is loaded or unloaded, and when threads are created or destroyed.
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		// This code runs when the DLL is loaded into a process.
		// In a real attack, this would be where the malicious code is executed.
		// For demonstration purposes, we'll just show a message box.
        MessageBoxA(NULL, "DLL Loaded", "Proof of Concept", MB_OK | MB_ICONINFORMATION);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// This function is exported from the DLL.
extern "C" __declspec(dllexport) void SOCRuleTest()
{
    // This is a placeholder for the malicious function that could be exported.
    // In a real attack, this could be any code that the attacker wants to run.
    // For demonstarton purposes, we'll just show a message box.
    MessageBoxA(NULL, "SOC Rule Test", "Proof of Concept", MB_OK | MB_ICONERROR);
}