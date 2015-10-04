#include <vector>
#include "pxcsensemanager.h"
#include "pxcsession.h"

int main() {
	// create the PXCSenseManager
	PXCSenseManager *psm = 0;
	psm = PXCSenseManager::CreateInstance();
	if (!psm) {
		wprintf_s(L"Unable to create the PXCSenseManager\n");
		return 1;
	}

	PXCSession *session;
	session = psm->QuerySession();

	return 0;
}


