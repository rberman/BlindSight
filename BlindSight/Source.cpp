#include <windows.h>
#include <wchar.h>
#include "pxcsensemanager.h"

int wmain(int argc, WCHAR* argv[]) {
	// create the PXCSenseManager
	PXCSenseManager *psm = 0;
	psm = PXCSenseManager::CreateInstance();
	if (!psm) {
		wprintf_s(L"Unable to create the PXCSenseManager\n");
		return 1;
	}
	// Retrieve the underlying session created by the PXCSenseManager.
	// The returned instance is an PXCSenseManager internally managed object.
	// Note: Do not release the session!
	PXCSession *session;
	session = psm->QuerySession();
	if (session == NULL) {
		wprintf_s(L"Session not created by PXCSenseManager\n");
		return 2;
	}
	// query the session version
	PXCSession::ImplVersion ver;
	ver = session->QueryVersion();
	// print version to console
	wprintf_s(L" Hello Intel RSSDK Version %d.%d \n", ver.major, ver.minor);
	// enumerate all available modules that are automatically loaded with the RSSDK
	for (int i = 0;; i++) {
		PXCSession::ImplDesc desc;
		if (session->QueryImpl(0, i, &desc) < PXC_STATUS_NO_ERROR) break;
		// Print the module friendly name and iuid (interface unique ID)
		wprintf_s(L"Module[%d]: %s\n", i, desc.friendlyName);
		wprintf_s(L" iuid=%x\n", desc.iuid);
	}
	// close the streams (if any) and release any session and processing module instances
	psm->Release();
	system(“pause”);
	return 0;
}