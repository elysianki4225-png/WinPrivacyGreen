#include <windows.h>

// A harmless process with the same filename as the Sandboxie manager. Used
// only to test the fail-closed process guard on a Windows CI runner.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Sleep(60000);
    return 0;
}
